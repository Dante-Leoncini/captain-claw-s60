// sdl2lite core: init, errors, log, timer, rects, surfaces, RWops, keyboard and the event queue.
#include <SDL2/SDL.h>
#include "sdl2lite_platform.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Uint32 s_initFlags = 0;
static char s_error[256] = "";
static Uint64 s_startUs = 0;

int SDL_Init(Uint32 flags)
{
    if (!s_initFlags)
        s_startUs = plat_ticks_us();
    s_initFlags |= flags | SDL_INIT_EVENTS | SDL_INIT_TIMER;
    return 0;
}

int SDL_InitSubSystem(Uint32 flags)
{
    return SDL_Init(flags);
}

Uint32 SDL_WasInit(Uint32 flags)
{
    return flags ? (s_initFlags & flags) : s_initFlags;
}

void SDL_Quit(void)
{
    s_initFlags = 0;
}

void SDL_SetMainReady(void)
{
}

const char* SDL_GetError(void)
{
    return s_error;
}

int SDL_SetError(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_error, sizeof(s_error), fmt, ap);
    va_end(ap);
    return -1;
}

char* SDL_GetBasePath(void)
{
    const char* dir = plat_data_dir();
    char* copy = (char*)malloc(strlen(dir) + 1);
    strcpy(copy, dir);
    return copy;
}

void SDL_free(void* p)
{
    free(p);
}

//-----------------------------------------------------------------------------
// Log
//-----------------------------------------------------------------------------

static void LogV(const char* prefix, const char* fmt, va_list ap)
{
    char buf[1024];
    size_t n = strlen(prefix);
    memcpy(buf, prefix, n);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
    plat_log(buf);
}

void SDL_Log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    LogV("", fmt, ap);
    va_end(ap);
}

void SDL_LogInfo(int, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    LogV("", fmt, ap);
    va_end(ap);
}

void SDL_LogWarn(int, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    LogV("WARNING: ", fmt, ap);
    va_end(ap);
}

void SDL_LogError(int, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    LogV("ERROR: ", fmt, ap);
    va_end(ap);
}

//-----------------------------------------------------------------------------
// Timer
//-----------------------------------------------------------------------------

Uint32 SDL_GetTicks(void)
{
    return (Uint32)((plat_ticks_us() - s_startUs) / 1000);
}

Uint64 SDL_GetPerformanceCounter(void)
{
    return plat_ticks_us();
}

Uint64 SDL_GetPerformanceFrequency(void)
{
    return 1000000;
}

void SDL_Delay(Uint32 ms)
{
    plat_delay_ms(ms);
}

//-----------------------------------------------------------------------------
// Rects
//-----------------------------------------------------------------------------

SDL_bool SDL_IntersectRect(const SDL_Rect* a, const SDL_Rect* b, SDL_Rect* result)
{
    if (!a || !b)
        return SDL_FALSE;
    int x0 = a->x > b->x ? a->x : b->x;
    int y0 = a->y > b->y ? a->y : b->y;
    int x1 = (a->x + a->w) < (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
    int y1 = (a->y + a->h) < (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
    if (result)
    {
        result->x = x0;
        result->y = y0;
        result->w = x1 - x0;
        result->h = y1 - y0;
    }
    return (x1 > x0 && y1 > y0) ? SDL_TRUE : SDL_FALSE;
}

SDL_bool SDL_HasIntersection(const SDL_Rect* a, const SDL_Rect* b)
{
    return SDL_IntersectRect(a, b, NULL);
}

//-----------------------------------------------------------------------------
// Surfaces: always 32 bpp, bytes R,G,B,A in memory
//-----------------------------------------------------------------------------

static SDL_PixelFormat s_format =
{
    SDL_PIXELFORMAT_ABGR8888, 32, 4,
    0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0xFF000000u,
    0, 8, 16, 24
};

SDL_Surface* SDL_CreateRGBSurface(Uint32 flags, int width, int height, int, Uint32, Uint32, Uint32, Uint32)
{
    SDL_Surface* s = (SDL_Surface*)calloc(1, sizeof(SDL_Surface));
    if (!s)
        return NULL;
    s->flags = flags;
    s->format = &s_format;
    s->w = width;
    s->h = height;
    s->pitch = width * 4;
    s->refcount = 1;
    if (width > 0 && height > 0)
    {
        s->pixels = calloc(width * height, 4);
        if (!s->pixels)
        {
            free(s);
            SDL_SetError("Out of memory (%dx%d surface)", width, height);
            return NULL;
        }
    }
    return s;
}

void SDL_FreeSurface(SDL_Surface* surface)
{
    if (!surface)
        return;
    if (--surface->refcount > 0)
        return;
    free(surface->pixels);
    free(surface->palette);
    free(surface);
}

int SDL_FillRect(SDL_Surface* dst, const SDL_Rect* rect, Uint32 color)
{
    if (!dst)
        return -1;
    SDL_Rect full = { 0, 0, dst->w, dst->h };
    SDL_Rect r;
    if (!SDL_IntersectRect(rect ? rect : &full, &full, &r))
        return 0;
    for (int y = r.y; y < r.y + r.h; y++)
    {
        Uint32* row = (Uint32*)((Uint8*)dst->pixels + y * dst->pitch) + r.x;
        for (int x = 0; x < r.w; x++)
            row[x] = color;
    }
    return 0;
}

int SDL_SetColorKey(SDL_Surface* surface, int flag, Uint32 key)
{
    if (!surface)
        return -1;
    surface->colorkey_enabled = flag;
    surface->colorkey = key;
    // Claw's paletted art is transparent where it uses palette index 0 (black in the English
    // release, magenta in others): that entry is the key, whatever color was asked for.
    if (surface->palette && surface->paletteSize > 0)
        surface->colorkey = surface->palette[0];
    return 0;
}

Uint32 SDL_MapRGB(const SDL_PixelFormat*, Uint8 r, Uint8 g, Uint8 b)
{
    return (Uint32)r | ((Uint32)g << 8) | ((Uint32)b << 16) | 0xFF000000u;
}

Uint32 SDL_MapRGBA(const SDL_PixelFormat*, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    return (Uint32)r | ((Uint32)g << 8) | ((Uint32)b << 16) | ((Uint32)a << 24);
}

//-----------------------------------------------------------------------------
// RWops
//-----------------------------------------------------------------------------

SDL_RWops* SDL_RWFromConstMem(const void* mem, int size)
{
    SDL_RWops* rw = (SDL_RWops*)malloc(sizeof(SDL_RWops));
    if (!rw)
        return NULL;
    rw->base = (const Uint8*)mem;
    rw->here = rw->base;
    rw->stop = rw->base + size;
    return rw;
}

SDL_RWops* SDL_RWFromMem(void* mem, int size)
{
    return SDL_RWFromConstMem(mem, size);
}

void SDL_FreeRW(SDL_RWops* rw)
{
    free(rw);
}

int SDL_RWclose(SDL_RWops* rw)
{
    free(rw);
    return 0;
}

//-----------------------------------------------------------------------------
// Keyboard
//-----------------------------------------------------------------------------

static Uint8 s_keyState[SDL_NUM_SCANCODES];

const Uint8* SDL_GetKeyboardState(int* numkeys)
{
    if (numkeys)
        *numkeys = SDL_NUM_SCANCODES;
    return s_keyState;
}

SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode key)
{
    if (key & SDLK_SCANCODE_MASK)
        return (SDL_Scancode)(key & ~SDLK_SCANCODE_MASK);
    if (key >= 'a' && key <= 'z')
        return (SDL_Scancode)(SDL_SCANCODE_A + (key - 'a'));
    if (key >= 'A' && key <= 'Z')
        return (SDL_Scancode)(SDL_SCANCODE_A + (key - 'A'));
    if (key >= '1' && key <= '9')
        return (SDL_Scancode)(SDL_SCANCODE_1 + (key - '1'));
    switch (key)
    {
        case '0': return SDL_SCANCODE_0;
        case SDLK_RETURN: return SDL_SCANCODE_RETURN;
        case SDLK_ESCAPE: return SDL_SCANCODE_ESCAPE;
        case SDLK_BACKSPACE: return SDL_SCANCODE_BACKSPACE;
        case SDLK_TAB: return SDL_SCANCODE_TAB;
        case SDLK_SPACE: return SDL_SCANCODE_SPACE;
        case SDLK_BACKQUOTE: return SDL_SCANCODE_GRAVE;
        default: return SDL_SCANCODE_UNKNOWN;
    }
}

SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode sc)
{
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
        return 'a' + (sc - SDL_SCANCODE_A);
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
        return '1' + (sc - SDL_SCANCODE_1);
    switch (sc)
    {
        case SDL_SCANCODE_0: return '0';
        case SDL_SCANCODE_RETURN: return SDLK_RETURN;
        case SDL_SCANCODE_ESCAPE: return SDLK_ESCAPE;
        case SDL_SCANCODE_BACKSPACE: return SDLK_BACKSPACE;
        case SDL_SCANCODE_TAB: return SDLK_TAB;
        case SDL_SCANCODE_SPACE: return SDLK_SPACE;
        case SDL_SCANCODE_GRAVE: return SDLK_BACKQUOTE;
        case SDL_SCANCODE_UNKNOWN: return SDLK_UNKNOWN;
        default: return SDL_SCANCODE_TO_KEYCODE(sc);
    }
}

void SDL_StartTextInput(void)
{
}

void SDL_StopTextInput(void)
{
}

//-----------------------------------------------------------------------------
// Events
//-----------------------------------------------------------------------------

enum { EVENT_QUEUE_SIZE = 128 };
static SDL_Event s_queue[EVENT_QUEUE_SIZE];
static int s_queueHead = 0;
static int s_queueCount = 0;

int SDL_PushEvent(SDL_Event* event)
{
    if (s_queueCount == EVENT_QUEUE_SIZE)
        return 0;
    SDL_Event& e = s_queue[(s_queueHead + s_queueCount) % EVENT_QUEUE_SIZE];
    e = *event;
    if (!e.common.timestamp)
        e.common.timestamp = SDL_GetTicks();
    s_queueCount++;
    return 1;
}

void SDL_PumpEvents(void)
{
    plat_pump_events();
}

int SDL_PollEvent(SDL_Event* event)
{
    if (s_queueCount == 0)
        plat_pump_events();
    if (s_queueCount == 0)
        return 0;
    if (event)
    {
        *event = s_queue[s_queueHead];
        s_queueHead = (s_queueHead + 1) % EVENT_QUEUE_SIZE;
        s_queueCount--;
    }
    return 1;
}

void sdl2lite_on_key(SDL_Scancode scancode, SDL_Keycode sym, int down, int repeat)
{
    if (scancode > SDL_SCANCODE_UNKNOWN && scancode < SDL_NUM_SCANCODES)
        s_keyState[scancode] = down ? 1 : 0;

    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.state = down ? 1 : 0;
    e.key.repeat = repeat ? 1 : 0;
    e.key.keysym.scancode = scancode;
    e.key.keysym.sym = sym;
    SDL_PushEvent(&e);
}

void sdl2lite_on_quit(void)
{
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = SDL_QUIT;
    SDL_PushEvent(&e);
}

void sdl2lite_on_focus(int gained)
{
    if (!gained)
    {
        // Release everything so nothing stays stuck while in the background.
        for (int i = 0; i < SDL_NUM_SCANCODES; i++)
        {
            if (s_keyState[i])
                sdl2lite_on_key((SDL_Scancode)i, SDL_GetKeyFromScancode((SDL_Scancode)i), 0, 0);
        }
    }
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = SDL_WINDOWEVENT;
    e.window.event = gained ? SDL_WINDOWEVENT_RESTORED : SDL_WINDOWEVENT_MINIMIZED;
    SDL_PushEvent(&e);
}

void sdl2lite_on_resize(int w, int h)
{
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = SDL_WINDOWEVENT;
    e.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    e.window.data1 = w;
    e.window.data2 = h;
    SDL_PushEvent(&e);
}

//-----------------------------------------------------------------------------
// Threads: none. A "thread" runs synchronously.
//-----------------------------------------------------------------------------

SDL_Thread* SDL_CreateThread(SDL_ThreadFunction fn, const char*, void* data)
{
    if (fn)
        fn(data);
    return NULL;
}

void SDL_DetachThread(SDL_Thread*)
{
}

int SDL_SetThreadPriority(SDL_ThreadPriority)
{
    return 0;
}
