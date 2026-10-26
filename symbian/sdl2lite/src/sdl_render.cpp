// sdl2lite renderer on fixed-function OpenGL (ES 1.1 on Symbian, 1.1 on the PC).
//
// Every texture lives in a shared atlas page so a whole frame draws in a handful of
// batches: tiles, sprites and HUD are quads into one vertex array, flushed only when the
// page changes. Sprites with on/off transparency go to RGBA5551 pages, translucent ones to
// RGBA4444 pages (half the memory of 8888: phones run short of texture memory), images bigger
// than half a page get their own power-of-two texture.
// A surface that is a single color (fade rects, backgrounds) needs no texels at all: it
// draws the white texel of the first page tinted with that color.
#include <SDL2/SDL.h>
#include "sdl2lite_platform.h"
#include "sdl2lite_gl.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum PageFormat
{
    PAGE_5551,
    PAGE_4444,
    PAGE_565
};

struct Shelf
{
    int y, h, x;
};

enum { MAX_SHELVES = 64, PAD = 1 };

struct AtlasPage
{
    GLuint tex;
    int w, h;
    PageFormat format;
    int live;
    int filter;
    bool dedicated;
    // streaming pages: a second texture and the whole page in memory, so each update replaces
    // the texture not in use instead of patching the one the GPU may still be drawing from
    GLuint spare;
    Uint16* staging;
    Shelf shelves[MAX_SHELVES];
    int shelfCount;
    int nextY;
    AtlasPage* next;
};

struct SDL_Texture
{
    AtlasPage* page;
    int w, h;
    float u0, v0, u1, v1;
    Uint8 r, g, b, a;
    // single-color texture: drawn with the white texel, tinted
    bool solid;
    Uint8 sr, sg, sb, sa;
};

struct SDL_Window
{
    int w, h;
    Uint32 flags;
};

struct SDL_Renderer
{
    SDL_Window* window;
    float scaleX, scaleY;
    Uint8 r, g, b, a;
};

static AtlasPage* s_pages = NULL;
static int s_pageSize = 1024;
static float s_whiteU = 0, s_whiteV = 0;
static AtlasPage* s_whitePage = NULL;
static int s_filter = GL_NEAREST;

//-----------------------------------------------------------------------------
// Batch
//-----------------------------------------------------------------------------

enum { MAX_QUADS = 512 };

static GLfloat s_pos[MAX_QUADS * 4 * 2];
static GLfloat s_uv[MAX_QUADS * 4 * 2];
static GLubyte s_col[MAX_QUADS * 4 * 4];
static GLushort s_indices[MAX_QUADS * 6];
static int s_quads = 0;
static AtlasPage* s_batchPage = NULL;
static GLuint s_boundTex = 0;

static int s_statDraws = 0;
static int s_statQuads = 0;

static void Flush()
{
    if (s_quads == 0)
        return;
    AtlasPage* page = s_batchPage;
    if (page->tex != s_boundTex)
    {
        glBindTexture(GL_TEXTURE_2D, page->tex);
        s_boundTex = page->tex;
    }
    if (page->filter != s_filter)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s_filter);
        page->filter = s_filter;
    }
    glDrawElements(GL_TRIANGLES, s_quads * 6, GL_UNSIGNED_SHORT, s_indices);
    s_statDraws++;
    s_statQuads += s_quads;
    s_quads = 0;
}

static void SetupState(SDL_Renderer* renderer)
{
    int w, h;
    plat_video_size(&w, &h);
    renderer->window->w = w;
    renderer->window->h = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    sdl2lite_glOrtho(0, (GLfloat)w, (GLfloat)h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_DITHER);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, s_pos);
    glTexCoordPointer(2, GL_FLOAT, 0, s_uv);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, s_col);
    s_boundTex = 0;
}

static void PushQuad(AtlasPage* page,
    float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
    float u0, float v0, float u1, float v1, bool flipH, bool flipV,
    Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (page != s_batchPage || s_quads == MAX_QUADS)
    {
        Flush();
        s_batchPage = page;
    }
    if (flipH)
    {
        float t = u0; u0 = u1; u1 = t;
    }
    if (flipV)
    {
        float t = v0; v0 = v1; v1 = t;
    }
    GLfloat* p = s_pos + s_quads * 8;
    p[0] = x0; p[1] = y0;
    p[2] = x1; p[3] = y1;
    p[4] = x2; p[5] = y2;
    p[6] = x3; p[7] = y3;
    GLfloat* t = s_uv + s_quads * 8;
    t[0] = u0; t[1] = v0;
    t[2] = u1; t[3] = v0;
    t[4] = u1; t[5] = v1;
    t[6] = u0; t[7] = v1;
    GLubyte* c = s_col + s_quads * 16;
    for (int i = 0; i < 4; i++)
    {
        c[i * 4 + 0] = r;
        c[i * 4 + 1] = g;
        c[i * 4 + 2] = b;
        c[i * 4 + 3] = a;
    }
    s_quads++;
}

static void PushRect(AtlasPage* page, float x, float y, float w, float h,
    float u0, float v0, float u1, float v1, bool flipH, bool flipV, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    PushQuad(page, x, y, x + w, y, x + w, y + h, x, y + h, u0, v0, u1, v1, flipH, flipV, r, g, b, a);
}

//-----------------------------------------------------------------------------
// Atlas
//-----------------------------------------------------------------------------

static int NextPow2(int v)
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

static AtlasPage* NewPage(int w, int h, PageFormat format, bool dedicated)
{
    AtlasPage* page = (AtlasPage*)calloc(1, sizeof(AtlasPage));
    if (!page)
        return NULL;
    page->w = w;
    page->h = h;
    page->format = format;
    page->dedicated = dedicated;
    page->filter = s_filter;

    Flush();
    glGenTextures(1, &page->tex);
    glBindTexture(GL_TEXTURE_2D, page->tex);
    s_boundTex = page->tex;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (format == PAGE_5551)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, NULL);
    else if (format == PAGE_565)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
    if (glGetError() != GL_NO_ERROR)
    {
        glDeleteTextures(1, &page->tex);
        free(page);
        SDL_SetError("glTexImage2D %dx%d failed", w, h);
        return NULL;
    }

    page->next = s_pages;
    s_pages = page;

    int count = 0, bytes = 0;
    for (AtlasPage* p = s_pages; p; p = p->next)
    {
        count++;
        bytes += p->w * p->h * 2;
    }
    SDL_Log("sdl2lite: texture %dx%d %s%s, %d textures %d KB", w, h,
        format == PAGE_5551 ? "5551" : (format == PAGE_565 ? "565" : "4444"),
        dedicated ? " (own)" : "", count, bytes / 1024);
    return page;
}

static void FreePage(AtlasPage* page)
{
    Flush();
    if (s_batchPage == page)
        s_batchPage = NULL;
    AtlasPage** link = &s_pages;
    while (*link && *link != page)
        link = &(*link)->next;
    if (*link)
        *link = page->next;
    if (s_boundTex == page->tex || s_boundTex == page->spare)
        s_boundTex = 0;
    SDL_Log("sdl2lite: texture %dx%d %s freed", page->w, page->h,
        page->format == PAGE_5551 ? "5551" : (page->format == PAGE_565 ? "565" : "4444"));
    glDeleteTextures(1, &page->tex);
    if (page->spare)
        glDeleteTextures(1, &page->spare);
    free(page->staging);
    free(page);
}

static bool PageAlloc(AtlasPage* page, int w, int h, int* outX, int* outY)
{
    // best fitting existing shelf
    int best = -1;
    for (int i = 0; i < page->shelfCount; i++)
    {
        Shelf& s = page->shelves[i];
        if (s.h >= h && s.x + w <= page->w && (best < 0 || s.h < page->shelves[best].h))
            best = i;
    }
    if (best >= 0 && page->shelves[best].h <= h + h / 2 + 4)
    {
        Shelf& s = page->shelves[best];
        *outX = s.x;
        *outY = s.y;
        s.x += w;
        return true;
    }
    if (page->shelfCount < MAX_SHELVES && page->nextY + h <= page->h)
    {
        Shelf& s = page->shelves[page->shelfCount++];
        s.y = page->nextY;
        s.h = h;
        s.x = w;
        page->nextY += h;
        *outX = 0;
        *outY = s.y;
        return true;
    }
    if (best >= 0)
    {
        Shelf& s = page->shelves[best];
        *outX = s.x;
        *outY = s.y;
        s.x += w;
        return true;
    }
    return false;
}

static void ResetPage(AtlasPage* page)
{
    page->shelfCount = 0;
    page->nextY = 0;
}

static void UploadRegion(AtlasPage* page, int x, int y, int w, int h, const Uint32* rgba, int srcW, int srcH,
    int pad)
{
    // Pads by replicating the border texels so bilinear filtering never mixes neighbours.
    Flush();
    if (s_boundTex != page->tex)
    {
        glBindTexture(GL_TEXTURE_2D, page->tex);
        s_boundTex = page->tex;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    Uint16* buf = (Uint16*)malloc(w * h * 2);
    if (!buf)
        return;
    const bool binaryAlpha = page->format == PAGE_5551;
    for (int j = 0; j < h; j++)
    {
        int sy = j - pad;
        if (sy < 0) sy = 0;
        if (sy >= srcH) sy = srcH - 1;
        const Uint32* srow = rgba + sy * srcW;
        Uint16* drow = buf + j * w;
        for (int i = 0; i < w; i++)
        {
            int sx = i - pad;
            if (sx < 0) sx = 0;
            if (sx >= srcW) sx = srcW - 1;
            Uint32 p = srow[sx];
            Uint32 r = p & 0xFF, g = (p >> 8) & 0xFF, b = (p >> 16) & 0xFF, a = p >> 24;
            if (binaryAlpha)
                drow[i] = (Uint16)(((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (a >= 128 ? 1 : 0));
            else
                drow[i] = (Uint16)(((r >> 4) << 12) | ((g >> 4) << 8) | ((b >> 4) << 4) | (a >> 4));
        }
    }
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA,
        binaryAlpha ? GL_UNSIGNED_SHORT_5_5_5_1 : GL_UNSIGNED_SHORT_4_4_4_4, buf);
    free(buf);
}

static void EnsureWhiteTexel()
{
    if (s_whitePage)
        return;
    AtlasPage* page = NewPage(s_pageSize, s_pageSize, PAGE_5551, false);
    if (!page)
        return;
    int x, y;
    PageAlloc(page, 4, 4, &x, &y);
    Uint32 white[16];
    for (int i = 0; i < 16; i++)
        white[i] = 0xFFFFFFFFu;
    UploadRegion(page, x, y, 4, 4, white, 4, 4, 0);
    page->live++;
    s_whitePage = page;
    s_whiteU = (x + 2.0f) / page->w;
    s_whiteV = (y + 2.0f) / page->h;
}

//-----------------------------------------------------------------------------
// Window / renderer
//-----------------------------------------------------------------------------

static SDL_Window s_window;
static SDL_Renderer s_renderer;
static bool s_windowOpen = false;

SDL_Window* SDL_CreateWindow(const char*, int, int, int w, int h, Uint32 flags)
{
    int outW, outH;
    if (!plat_video_init(w, h, &outW, &outH))
    {
        SDL_SetError("Could not create the GL window");
        return NULL;
    }
    s_window.w = outW;
    s_window.h = outH;
    s_window.flags = flags | SDL_WINDOW_SHOWN;
    s_windowOpen = true;
    return &s_window;
}

void SDL_DestroyWindow(SDL_Window*)
{
    if (s_windowOpen)
    {
        plat_video_shutdown();
        s_windowOpen = false;
    }
}

void SDL_GetWindowSize(SDL_Window* window, int* w, int* h)
{
    plat_video_size(&window->w, &window->h);
    if (w)
        *w = window->w;
    if (h)
        *h = window->h;
}

void SDL_SetWindowSize(SDL_Window*, int, int)
{
}

Uint32 SDL_GetWindowFlags(SDL_Window* window)
{
    return window->flags;
}

int SDL_SetWindowFullscreen(SDL_Window* window, Uint32 flags)
{
    window->flags = (window->flags & ~SDL_WINDOW_FULLSCREEN_DESKTOP) | flags;
    return 0;
}

SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, int, Uint32)
{
    s_renderer.window = window;
    s_renderer.scaleX = s_renderer.scaleY = 1.0f;
    s_renderer.r = s_renderer.g = s_renderer.b = 0;
    s_renderer.a = 255;

    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    s_pageSize = (maxSize >= 1024 || maxSize <= 0) ? 1024 : maxSize;
    for (int i = 0; i < MAX_QUADS; i++)
    {
        s_indices[i * 6 + 0] = (GLushort)(i * 4 + 0);
        s_indices[i * 6 + 1] = (GLushort)(i * 4 + 1);
        s_indices[i * 6 + 2] = (GLushort)(i * 4 + 2);
        s_indices[i * 6 + 3] = (GLushort)(i * 4 + 0);
        s_indices[i * 6 + 4] = (GLushort)(i * 4 + 2);
        s_indices[i * 6 + 5] = (GLushort)(i * 4 + 3);
    }
    SetupState(&s_renderer);
    EnsureWhiteTexel();
    SDL_Log("sdl2lite: renderer %s | %s, max texture %d, page %d", (const char*)glGetString(GL_VENDOR),
        (const char*)glGetString(GL_RENDERER), (int)maxSize, s_pageSize);
    return &s_renderer;
}

void SDL_DestroyRenderer(SDL_Renderer*)
{
    Flush();
    while (s_pages)
        FreePage(s_pages);
    s_whitePage = NULL;
}

int SDL_RenderSetScale(SDL_Renderer* renderer, float scaleX, float scaleY)
{
    Flush();
    renderer->scaleX = scaleX;
    renderer->scaleY = scaleY;
    s_filter = (scaleX == 1.0f && scaleY == 1.0f) ? GL_NEAREST : GL_LINEAR;
    return 0;
}

void SDL_RenderGetScale(SDL_Renderer* renderer, float* scaleX, float* scaleY)
{
    if (scaleX)
        *scaleX = renderer->scaleX;
    if (scaleY)
        *scaleY = renderer->scaleY;
}

int SDL_SetRenderDrawColor(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    renderer->r = r;
    renderer->g = g;
    renderer->b = b;
    renderer->a = a;
    return 0;
}

int SDL_GetRenderDrawColor(SDL_Renderer* renderer, Uint8* r, Uint8* g, Uint8* b, Uint8* a)
{
    if (r) *r = renderer->r;
    if (g) *g = renderer->g;
    if (b) *b = renderer->b;
    if (a) *a = renderer->a;
    return 0;
}

int SDL_RenderClear(SDL_Renderer* renderer)
{
    Flush();
    SetupState(renderer);
    glClearColor(renderer->r / 255.0f, renderer->g / 255.0f, renderer->b / 255.0f, renderer->a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return 0;
}

void SDL_RenderPresent(SDL_Renderer*)
{
    Flush();
    const Uint64 beforeSwap = plat_ticks_us();
    plat_video_swap();

    // performance line every 2 seconds
    static Uint64 periodStart = 0;
    static Uint64 swapUs = 0;
    static int frames = 0, draws = 0, quads = 0;
    const Uint64 now = plat_ticks_us();
    swapUs += now - beforeSwap;
    frames++;
    draws += s_statDraws;
    quads += s_statQuads;
    if (periodStart == 0)
        periodStart = now;
    if (now - periodStart >= 2000000)
    {
        const double ms = (now - periodStart) / 1000.0 / frames;
        SDL_Log("[perf] %.1f fps, %.1f ms/frame (swap %.1f), %d draws %d quads per frame", 1000.0 / ms, ms,
            swapUs / 1000.0 / frames, draws / frames, quads / frames);
        periodStart = now;
        swapUs = 0;
        frames = draws = quads = 0;
    }
    s_statDraws = 0;
    s_statQuads = 0;
}

//-----------------------------------------------------------------------------
// Textures
//-----------------------------------------------------------------------------

SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer*, SDL_Surface* surface)
{
    if (!surface || surface->w <= 0 || surface->h <= 0)
    {
        SDL_SetError("Invalid surface");
        return NULL;
    }
    SDL_Texture* tex = (SDL_Texture*)calloc(1, sizeof(SDL_Texture));
    if (!tex)
        return NULL;
    tex->w = surface->w;
    tex->h = surface->h;
    tex->r = tex->g = tex->b = tex->a = 255;

    const int count = surface->w * surface->h;
    Uint32* px = (Uint32*)surface->pixels;
    bool ownCopy = false;
    if (surface->colorkey_enabled || surface->pitch != surface->w * 4)
    {
        Uint32* copy = (Uint32*)malloc(count * 4);
        if (!copy)
        {
            free(tex);
            return NULL;
        }
        for (int y = 0; y < surface->h; y++)
            memcpy(copy + y * surface->w, (Uint8*)surface->pixels + y * surface->pitch, surface->w * 4);
        if (surface->colorkey_enabled)
        {
            const Uint32 key = surface->colorkey & 0x00FFFFFFu;
            for (int i = 0; i < count; i++)
            {
                if ((copy[i] & 0x00FFFFFFu) == key)
                    copy[i] = 0;
            }
        }
        px = copy;
        ownCopy = true;
    }

    bool solid = true;
    bool binaryAlpha = true;
    for (int i = 0; i < count; i++)
    {
        const Uint32 p = px[i];
        if (p != px[0])
            solid = false;
        const Uint32 a = p >> 24;
        // (Claw images mark some invisible pixels with alpha 1: still on/off transparency)
        if (a >= 16 && a <= 239)
            binaryAlpha = false;
    }

    if (solid && s_whitePage)
    {
        const Uint32 p = px[0];
        tex->solid = true;
        tex->sr = (Uint8)(p & 0xFF);
        tex->sg = (Uint8)((p >> 8) & 0xFF);
        tex->sb = (Uint8)((p >> 16) & 0xFF);
        tex->sa = (Uint8)(p >> 24);
        tex->page = s_whitePage;
        s_whitePage->live++;
        tex->u0 = tex->u1 = s_whiteU;
        tex->v0 = tex->v1 = s_whiteV;
    }
    else
    {
        const PageFormat format = binaryAlpha ? PAGE_5551 : PAGE_4444;
        const int pw = tex->w + PAD * 2;
        const int ph = tex->h + PAD * 2;
        AtlasPage* page = NULL;
        int x = 0, y = 0;
        if (pw > s_pageSize / 2 || ph > s_pageSize / 2)
        {
            page = NewPage(NextPow2(tex->w), NextPow2(tex->h), format, true);
            if (page)
                UploadRegion(page, 0, 0, tex->w, tex->h, px, tex->w, tex->h, 0);
        }
        else
        {
            for (AtlasPage* p = s_pages; p; p = p->next)
            {
                if (p->dedicated || p->format != format)
                    continue;
                if (p->live == 0)
                    ResetPage(p);
                if (PageAlloc(p, pw, ph, &x, &y))
                {
                    page = p;
                    break;
                }
            }
            if (!page)
            {
                page = NewPage(s_pageSize, s_pageSize, format, false);
                if (page && !PageAlloc(page, pw, ph, &x, &y))
                    page = NULL;
            }
            if (page)
            {
                UploadRegion(page, x, y, pw, ph, px, tex->w, tex->h, PAD);
                x += PAD;
                y += PAD;
            }
        }
        if (!page)
        {
            if (ownCopy)
                free(px);
            free(tex);
            return NULL;
        }
        page->live++;
        tex->page = page;
        tex->u0 = (float)x / page->w;
        tex->v0 = (float)y / page->h;
        tex->u1 = (float)(x + tex->w) / page->w;
        tex->v1 = (float)(y + tex->h) / page->h;
    }

    if (ownCopy)
        free(px);
    return tex;
}

SDL_Texture* SDL_CreateTexture(SDL_Renderer*, Uint32 format, int, int w, int h)
{
    if (format != SDL_PIXELFORMAT_RGB565 || w <= 0 || h <= 0)
    {
        SDL_SetError("SDL_CreateTexture: only RGB565 streaming textures");
        return NULL;
    }
    SDL_Texture* tex = (SDL_Texture*)calloc(1, sizeof(SDL_Texture));
    if (!tex)
        return NULL;
    AtlasPage* page = NewPage(NextPow2(w), NextPow2(h), PAGE_565, true);
    if (!page)
    {
        free(tex);
        return NULL;
    }
    page->staging = (Uint16*)calloc((size_t)page->w * page->h, sizeof(Uint16));
    if (page->staging)
    {
        glGenTextures(1, &page->spare);
        glBindTexture(GL_TEXTURE_2D, page->spare);
        s_boundTex = page->spare;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, page->filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, page->filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    page->live++;
    tex->page = page;
    tex->w = w;
    tex->h = h;
    tex->r = tex->g = tex->b = tex->a = 255;
    tex->u0 = 0;
    tex->v0 = 0;
    tex->u1 = (float)w / page->w;
    tex->v1 = (float)h / page->h;
    return tex;
}

int SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rect, const void* pixels, int pitch)
{
    if (!texture || !texture->page || texture->page->format != PAGE_565)
        return -1;
    SDL_Rect r = { 0, 0, texture->w, texture->h };
    if (rect)
        r = *rect;
    Flush();
    AtlasPage* page = texture->page;
    if (page->staging && page->spare)
    {
        for (int y = 0; y < r.h; y++)
        {
            memcpy(page->staging + (size_t)(r.y + y) * page->w + r.x, (const Uint8*)pixels + y * pitch, r.w * 2);
        }
        GLuint drawn = page->tex;
        page->tex = page->spare;
        page->spare = drawn;
        glBindTexture(GL_TEXTURE_2D, page->tex);
        s_boundTex = page->tex;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, page->w, page->h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, page->staging);
        return 0;
    }
    if (s_boundTex != texture->page->tex)
    {
        glBindTexture(GL_TEXTURE_2D, texture->page->tex);
        s_boundTex = texture->page->tex;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    if (pitch == r.w * 2)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, r.x, r.y, r.w, r.h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels);
    }
    else
    {
        // (GL ES 1.1 has no row length: one row at a time)
        for (int y = 0; y < r.h; y++)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, r.x, r.y + y, r.w, 1, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                (const Uint8*)pixels + y * pitch);
        }
    }
    return 0;
}

void SDL_DestroyTexture(SDL_Texture* texture)
{
    if (!texture)
        return;
    AtlasPage* page = texture->page;
    // An empty page goes back to the driver: phones have little room for textures, and a page
    // kept for later may never fit what comes next (another format, a level instead of the menu)
    if (page && --page->live <= 0 && page != s_whitePage)
        FreePage(page);
    free(texture);
}

int SDL_QueryTexture(SDL_Texture* texture, Uint32* format, int* access, int* w, int* h)
{
    if (!texture)
        return -1;
    if (format) *format = SDL_PIXELFORMAT_ABGR8888;
    if (access) *access = SDL_TEXTUREACCESS_STATIC;
    if (w) *w = texture->w;
    if (h) *h = texture->h;
    return 0;
}

int SDL_SetTextureColorMod(SDL_Texture* texture, Uint8 r, Uint8 g, Uint8 b)
{
    if (!texture)
        return -1;
    texture->r = r;
    texture->g = g;
    texture->b = b;
    return 0;
}

int SDL_SetTextureAlphaMod(SDL_Texture* texture, Uint8 alpha)
{
    if (!texture)
        return -1;
    texture->a = alpha;
    return 0;
}

//-----------------------------------------------------------------------------
// Drawing
//-----------------------------------------------------------------------------

static void TextureColor(const SDL_Texture* t, Uint8* r, Uint8* g, Uint8* b, Uint8* a)
{
    if (t->solid)
    {
        *r = (Uint8)(t->sr * t->r / 255);
        *g = (Uint8)(t->sg * t->g / 255);
        *b = (Uint8)(t->sb * t->b / 255);
        *a = (Uint8)(t->sa * t->a / 255);
    }
    else
    {
        *r = t->r;
        *g = t->g;
        *b = t->b;
        *a = t->a;
    }
}

static void SourceUV(const SDL_Texture* t, const SDL_Rect* src, float* u0, float* v0, float* u1, float* v1)
{
    if (!src || t->solid)
    {
        *u0 = t->u0; *v0 = t->v0; *u1 = t->u1; *v1 = t->v1;
        return;
    }
    const float du = (t->u1 - t->u0) / t->w;
    const float dv = (t->v1 - t->v0) / t->h;
    *u0 = t->u0 + src->x * du;
    *v0 = t->v0 + src->y * dv;
    *u1 = t->u0 + (src->x + src->w) * du;
    *v1 = t->v0 + (src->y + src->h) * dv;
}

int SDL_RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect* src, const SDL_Rect* dst)
{
    if (!texture)
        return -1;
    float x, y, w, h;
    if (dst)
    {
        x = dst->x * renderer->scaleX;
        y = dst->y * renderer->scaleY;
        w = dst->w * renderer->scaleX;
        h = dst->h * renderer->scaleY;
    }
    else
    {
        x = 0;
        y = 0;
        w = (float)renderer->window->w;
        h = (float)renderer->window->h;
    }
    const float vw = (float)renderer->window->w, vh = (float)renderer->window->h;
    if (x >= vw || y >= vh || x + w <= 0 || y + h <= 0)
        return 0;
    float u0, v0, u1, v1;
    SourceUV(texture, src, &u0, &v0, &u1, &v1);
    Uint8 r, g, b, a;
    TextureColor(texture, &r, &g, &b, &a);
    PushRect(texture->page, x, y, w, h, u0, v0, u1, v1, false, false, r, g, b, a);
    return 0;
}

int SDL_RenderCopyEx(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect* src, const SDL_Rect* dst,
    const double angle, const SDL_Point* center, const SDL_RendererFlip flip)
{
    if (!texture)
        return -1;
    float x, y, w, h;
    if (dst)
    {
        x = dst->x * renderer->scaleX;
        y = dst->y * renderer->scaleY;
        w = dst->w * renderer->scaleX;
        h = dst->h * renderer->scaleY;
    }
    else
    {
        x = 0;
        y = 0;
        w = (float)renderer->window->w;
        h = (float)renderer->window->h;
    }
    float u0, v0, u1, v1;
    SourceUV(texture, src, &u0, &v0, &u1, &v1);
    Uint8 r, g, b, a;
    TextureColor(texture, &r, &g, &b, &a);
    const bool fh = (flip & SDL_FLIP_HORIZONTAL) != 0;
    const bool fv = (flip & SDL_FLIP_VERTICAL) != 0;
    if (angle == 0.0)
    {
        const float vw = (float)renderer->window->w, vh = (float)renderer->window->h;
        if (x >= vw || y >= vh || x + w <= 0 || y + h <= 0)
            return 0;
        PushRect(texture->page, x, y, w, h, u0, v0, u1, v1, fh, fv, r, g, b, a);
        return 0;
    }
    const float cx = x + (center ? center->x * renderer->scaleX : w * 0.5f);
    const float cy = y + (center ? center->y * renderer->scaleY : h * 0.5f);
    const float rad = (float)(angle * 3.14159265358979 / 180.0);
    const float cs = (float)cos(rad), sn = (float)sin(rad);
    float px[4] = { x, x + w, x + w, x };
    float py[4] = { y, y, y + h, y + h };
    for (int i = 0; i < 4; i++)
    {
        const float dx = px[i] - cx, dy = py[i] - cy;
        px[i] = cx + dx * cs - dy * sn;
        py[i] = cy + dx * sn + dy * cs;
    }
    PushQuad(texture->page, px[0], py[0], px[1], py[1], px[2], py[2], px[3], py[3], u0, v0, u1, v1, fh, fv,
        r, g, b, a);
    return 0;
}

static void FillScaled(SDL_Renderer* renderer, float x, float y, float w, float h)
{
    if (!s_whitePage)
        return;
    PushRect(s_whitePage, x, y, w, h, s_whiteU, s_whiteV, s_whiteU, s_whiteV, false, false,
        renderer->r, renderer->g, renderer->b, renderer->a);
}

int SDL_RenderFillRect(SDL_Renderer* renderer, const SDL_Rect* rect)
{
    if (rect)
        FillScaled(renderer, rect->x * renderer->scaleX, rect->y * renderer->scaleY,
            rect->w * renderer->scaleX, rect->h * renderer->scaleY);
    else
        FillScaled(renderer, 0, 0, (float)renderer->window->w, (float)renderer->window->h);
    return 0;
}

int SDL_RenderDrawLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2)
{
    if (!s_whitePage)
        return -1;
    const float ax = x1 * renderer->scaleX, ay = y1 * renderer->scaleY;
    const float bx = x2 * renderer->scaleX, by = y2 * renderer->scaleY;
    float dx = bx - ax, dy = by - ay;
    const float len = (float)sqrt(dx * dx + dy * dy);
    if (len < 0.001f)
    {
        FillScaled(renderer, ax, ay, 1, 1);
        return 0;
    }
    // one pixel wide quad along the line
    const float nx = -dy / len * 0.5f, ny = dx / len * 0.5f;
    PushQuad(s_whitePage, ax + nx, ay + ny, bx + nx, by + ny, bx - nx, by - ny, ax - nx, ay - ny,
        s_whiteU, s_whiteV, s_whiteU, s_whiteV, false, false, renderer->r, renderer->g, renderer->b, renderer->a);
    return 0;
}

int SDL_RenderDrawRect(SDL_Renderer* renderer, const SDL_Rect* rect)
{
    if (!rect)
        return -1;
    const int x0 = rect->x, y0 = rect->y, x1 = rect->x + rect->w - 1, y1 = rect->y + rect->h - 1;
    SDL_RenderDrawLine(renderer, x0, y0, x1, y0);
    SDL_RenderDrawLine(renderer, x1, y0, x1, y1);
    SDL_RenderDrawLine(renderer, x1, y1, x0, y1);
    SDL_RenderDrawLine(renderer, x0, y1, x0, y0);
    return 0;
}

int SDL_RenderDrawPoint(SDL_Renderer* renderer, int x, int y)
{
    FillScaled(renderer, x * renderer->scaleX, y * renderer->scaleY, renderer->scaleX, renderer->scaleY);
    return 0;
}

int SDL_RenderReadPixels(SDL_Renderer* renderer, const SDL_Rect* rect, Uint32, void* pixels, int pitch)
{
    Flush();
    SDL_Rect r = { 0, 0, renderer->window->w, renderer->window->h };
    if (rect)
        r = *rect;
    // Into the surface layout (R,G,B,A bytes), flipped to top-down rows.
    Uint8* tmp = (Uint8*)malloc(r.w * r.h * 4);
    if (!tmp)
        return -1;
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(r.x, renderer->window->h - r.y - r.h, r.w, r.h, GL_RGBA, GL_UNSIGNED_BYTE, tmp);
    for (int y = 0; y < r.h; y++)
        memcpy((Uint8*)pixels + y * pitch, tmp + (r.h - 1 - y) * r.w * 4, r.w * 4);
    free(tmp);
    return 0;
}

extern "C" void sdl2lite_render_stats(int* draws, int* quads, int* pages)
{
    int n = 0;
    for (AtlasPage* p = s_pages; p; p = p->next)
        n++;
    if (draws) *draws = s_statDraws;
    if (quads) *quads = s_statQuads;
    if (pages) *pages = n;
}
