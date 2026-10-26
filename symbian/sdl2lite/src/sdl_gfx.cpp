// sdl2lite: the SDL2_gfx primitives used by the physics debug view, as lines.
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>

#include <math.h>

static void SetColor(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a, Uint8* saved)
{
    SDL_GetRenderDrawColor(renderer, &saved[0], &saved[1], &saved[2], &saved[3]);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

static void RestoreColor(SDL_Renderer* renderer, const Uint8* saved)
{
    SDL_SetRenderDrawColor(renderer, saved[0], saved[1], saved[2], saved[3]);
}

int polygonRGBA(SDL_Renderer* renderer, const Sint16* vx, const Sint16* vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (n < 2)
        return -1;
    Uint8 saved[4];
    SetColor(renderer, r, g, b, a, saved);
    for (int i = 0; i < n; i++)
    {
        const int j = (i + 1) % n;
        SDL_RenderDrawLine(renderer, vx[i], vy[i], vx[j], vy[j]);
    }
    RestoreColor(renderer, saved);
    return 0;
}

int filledPolygonRGBA(SDL_Renderer* renderer, const Sint16* vx, const Sint16* vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    return polygonRGBA(renderer, vx, vy, n, r, g, b, a);
}

int circleRGBA(SDL_Renderer* renderer, Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    Uint8 saved[4];
    SetColor(renderer, r, g, b, a, saved);
    const int segments = 16;
    for (int i = 0; i < segments; i++)
    {
        const float a0 = 6.2831853f * i / segments;
        const float a1 = 6.2831853f * (i + 1) / segments;
        SDL_RenderDrawLine(renderer, x + (int)((float)cos(a0) * rad), y + (int)((float)sin(a0) * rad),
            x + (int)((float)cos(a1) * rad), y + (int)((float)sin(a1) * rad));
    }
    RestoreColor(renderer, saved);
    return 0;
}
