// sdl2lite image loading: PCX (8-bit paletted / 24-bit) and PNG (8-bit channels, palette,
// no interlace) into 32-bit surfaces. PNG data is inflated with Miniz.
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "sdl2lite_platform.h"
#include "Miniz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int IMG_Init(int flags)
{
    return flags;
}

void IMG_Quit(void)
{
}

static Uint32 Rgba(Uint32 r, Uint32 g, Uint32 b, Uint32 a)
{
    return r | (g << 8) | (b << 16) | (a << 24);
}

//-----------------------------------------------------------------------------
// PCX
//-----------------------------------------------------------------------------

static SDL_Surface* LoadPCX(const Uint8* data, int size)
{
    if (size < 128 || data[0] != 0x0A || data[2] != 1)
    {
        SDL_SetError("Not a RLE PCX file");
        return NULL;
    }
    const int bpp = data[3];
    const int xmin = data[4] | (data[5] << 8);
    const int ymin = data[6] | (data[7] << 8);
    const int xmax = data[8] | (data[9] << 8);
    const int ymax = data[10] | (data[11] << 8);
    const int planes = data[65];
    const int bytesPerLine = data[66] | (data[67] << 8);
    const int w = xmax - xmin + 1;
    const int h = ymax - ymin + 1;
    if (bpp != 8 || (planes != 1 && planes != 3) || w <= 0 || h <= 0)
    {
        SDL_SetError("Unsupported PCX (%d bpp, %d planes)", bpp, planes);
        return NULL;
    }

    const int lineSize = bytesPerLine * planes;
    Uint8* raw = (Uint8*)malloc(lineSize * h);
    if (!raw)
        return NULL;
    const Uint8* p = data + 128;
    const Uint8* end = data + size;
    int out = 0;
    while (out < lineSize * h && p < end)
    {
        Uint8 c = *p++;
        int run = 1;
        if ((c & 0xC0) == 0xC0)
        {
            run = c & 0x3F;
            if (p >= end)
                break;
            c = *p++;
        }
        while (run-- > 0 && out < lineSize * h)
            raw[out++] = c;
    }

    SDL_Surface* s = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0);
    if (!s)
    {
        free(raw);
        return NULL;
    }
    Uint32* px = (Uint32*)s->pixels;
    if (planes == 1)
    {
        const Uint8* pal = (size >= 769 && end[-769] == 0x0C) ? end - 768 : NULL;
        s->palette = (Uint32*)malloc(256 * 4);
        if (s->palette)
        {
            s->paletteSize = 256;
            for (int i = 0; i < 256; i++)
                s->palette[i] = pal ? Rgba(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2], 255) : Rgba(i, i, i, 255);
        }
        for (int y = 0; y < h; y++)
        {
            const Uint8* line = raw + y * lineSize;
            for (int x = 0; x < w; x++)
            {
                const int i = line[x];
                px[y * w + x] = pal ? Rgba(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2], 255) : Rgba(i, i, i, 255);
            }
        }
    }
    else
    {
        for (int y = 0; y < h; y++)
        {
            const Uint8* line = raw + y * lineSize;
            for (int x = 0; x < w; x++)
                px[y * w + x] = Rgba(line[x], line[bytesPerLine + x], line[bytesPerLine * 2 + x], 255);
        }
    }
    free(raw);
    return s;
}

//-----------------------------------------------------------------------------
// PNG
//-----------------------------------------------------------------------------

static Uint32 Be32(const Uint8* p)
{
    return ((Uint32)p[0] << 24) | ((Uint32)p[1] << 16) | ((Uint32)p[2] << 8) | p[3];
}

static int Paeth(int a, int b, int c)
{
    const int p = a + b - c;
    const int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    if (pa <= pb && pa <= pc)
        return a;
    return pb <= pc ? b : c;
}

static SDL_Surface* LoadPNG(const Uint8* data, int size)
{
    static const Uint8 sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (size < 33 || memcmp(data, sig, 8) != 0)
    {
        SDL_SetError("Not a PNG file");
        return NULL;
    }

    int w = 0, h = 0, depth = 0, colorType = 0, interlace = 0;
    Uint8 palette[256 * 4];
    memset(palette, 255, sizeof(palette));
    int trnsGray = -1, trnsR = -1, trnsG = -1, trnsB = -1;
    Uint8* idat = NULL;
    int idatSize = 0;

    const Uint8* p = data + 8;
    const Uint8* end = data + size;
    while (p + 12 <= end)
    {
        const Uint32 len = Be32(p);
        const Uint8* type = p + 4;
        const Uint8* body = p + 8;
        if (body + len > end)
            break;
        if (!memcmp(type, "IHDR", 4) && len >= 13)
        {
            w = (int)Be32(body);
            h = (int)Be32(body + 4);
            depth = body[8];
            colorType = body[9];
            interlace = body[12];
        }
        else if (!memcmp(type, "PLTE", 4))
        {
            for (Uint32 i = 0; i < len / 3 && i < 256; i++)
            {
                palette[i * 4 + 0] = body[i * 3 + 0];
                palette[i * 4 + 1] = body[i * 3 + 1];
                palette[i * 4 + 2] = body[i * 3 + 2];
            }
        }
        else if (!memcmp(type, "tRNS", 4))
        {
            if (colorType == 3)
            {
                for (Uint32 i = 0; i < len && i < 256; i++)
                    palette[i * 4 + 3] = body[i];
            }
            else if (colorType == 0 && len >= 2)
            {
                trnsGray = (body[0] << 8) | body[1];
            }
            else if (colorType == 2 && len >= 6)
            {
                trnsR = (body[0] << 8) | body[1];
                trnsG = (body[2] << 8) | body[3];
                trnsB = (body[4] << 8) | body[5];
            }
        }
        else if (!memcmp(type, "IDAT", 4))
        {
            Uint8* grown = (Uint8*)realloc(idat, idatSize + len);
            if (!grown)
            {
                free(idat);
                return NULL;
            }
            idat = grown;
            memcpy(idat + idatSize, body, len);
            idatSize += len;
        }
        else if (!memcmp(type, "IEND", 4))
        {
            break;
        }
        p = body + len + 4;
    }

    int channels = 0;
    switch (colorType)
    {
        case 0: channels = 1; break;
        case 2: channels = 3; break;
        case 3: channels = 1; break;
        case 4: channels = 2; break;
        case 6: channels = 4; break;
    }
    const bool depthOk = depth == 8 || (colorType == 3 && (depth == 1 || depth == 2 || depth == 4));
    if (w <= 0 || h <= 0 || !channels || !depthOk || interlace || !idat)
    {
        free(idat);
        SDL_SetError("Unsupported PNG (type %d, depth %d, interlace %d)", colorType, depth, interlace);
        return NULL;
    }

    const int stride = (w * channels * depth + 7) / 8;
    const int bppBytes = (channels * depth + 7) / 8;
    mz_ulong rawSize = (mz_ulong)(stride + 1) * h;
    Uint8* raw = (Uint8*)malloc(rawSize);
    if (!raw || mz_uncompress(raw, &rawSize, idat, idatSize) != MZ_OK || rawSize != (mz_ulong)(stride + 1) * h)
    {
        free(raw);
        free(idat);
        SDL_SetError("PNG inflate failed");
        return NULL;
    }
    free(idat);

    // unfilter in place: row y starts at raw + y * (stride + 1), first byte is the filter
    for (int y = 0; y < h; y++)
    {
        Uint8* row = raw + y * (stride + 1) + 1;
        const Uint8* prev = y > 0 ? raw + (y - 1) * (stride + 1) + 1 : NULL;
        const int filter = row[-1];
        for (int i = 0; i < stride; i++)
        {
            const int a = i >= bppBytes ? row[i - bppBytes] : 0;
            const int b = prev ? prev[i] : 0;
            const int c = (prev && i >= bppBytes) ? prev[i - bppBytes] : 0;
            switch (filter)
            {
                case 1: row[i] = (Uint8)(row[i] + a); break;
                case 2: row[i] = (Uint8)(row[i] + b); break;
                case 3: row[i] = (Uint8)(row[i] + ((a + b) >> 1)); break;
                case 4: row[i] = (Uint8)(row[i] + Paeth(a, b, c)); break;
                default: break;
            }
        }
    }

    SDL_Surface* s = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0);
    if (!s)
    {
        free(raw);
        return NULL;
    }
    Uint32* px = (Uint32*)s->pixels;
    for (int y = 0; y < h; y++)
    {
        const Uint8* row = raw + y * (stride + 1) + 1;
        Uint32* out = px + y * w;
        for (int x = 0; x < w; x++)
        {
            switch (colorType)
            {
                case 0:
                {
                    const int g = row[x];
                    out[x] = Rgba(g, g, g, g == trnsGray ? 0 : 255);
                    break;
                }
                case 2:
                {
                    const Uint8* c = row + x * 3;
                    const bool t = c[0] == trnsR && c[1] == trnsG && c[2] == trnsB;
                    out[x] = Rgba(c[0], c[1], c[2], t ? 0 : 255);
                    break;
                }
                case 3:
                {
                    int i;
                    if (depth == 8)
                    {
                        i = row[x];
                    }
                    else
                    {
                        const int perByte = 8 / depth;
                        const int shift = (perByte - 1 - (x % perByte)) * depth;
                        i = (row[x / perByte] >> shift) & ((1 << depth) - 1);
                    }
                    const Uint8* c = palette + i * 4;
                    out[x] = Rgba(c[0], c[1], c[2], c[3]);
                    break;
                }
                case 4:
                {
                    const Uint8* c = row + x * 2;
                    out[x] = Rgba(c[0], c[0], c[0], c[1]);
                    break;
                }
                case 6:
                {
                    const Uint8* c = row + x * 4;
                    out[x] = Rgba(c[0], c[1], c[2], c[3]);
                    break;
                }
            }
        }
    }
    free(raw);
    return s;
}

static SDL_Surface* LoadAny(const Uint8* data, int size)
{
    if (size >= 8 && data[0] == 137 && data[1] == 'P')
        return LoadPNG(data, size);
    if (size >= 128 && data[0] == 0x0A)
        return LoadPCX(data, size);
    SDL_SetError("Unknown image format");
    return NULL;
}

SDL_Surface* IMG_LoadPCX_RW(SDL_RWops* src)
{
    if (!src)
        return NULL;
    return LoadPCX(src->here, (int)(src->stop - src->here));
}

SDL_Surface* IMG_LoadPNG_RW(SDL_RWops* src)
{
    if (!src)
        return NULL;
    return LoadPNG(src->here, (int)(src->stop - src->here));
}

SDL_Surface* IMG_Load(const char* file)
{
    FILE* f = fopen(file, "rb");
    if (!f)
    {
        SDL_SetError("Could not open %s", file);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    Uint8* data = size > 0 ? (Uint8*)malloc(size) : NULL;
    SDL_Surface* s = NULL;
    if (data && fread(data, 1, size, f) == (size_t)size)
        s = LoadAny(data, (int)size);
    free(data);
    fclose(f);
    return s;
}

SDL_Texture* IMG_LoadTexture(SDL_Renderer* renderer, const char* file)
{
    SDL_Surface* s = IMG_Load(file);
    if (!s)
        return NULL;
    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
    SDL_FreeSurface(s);
    return t;
}
