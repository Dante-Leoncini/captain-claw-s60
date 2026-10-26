#ifndef SDL2LITE_GFX_PRIMITIVES_H_
#define SDL2LITE_GFX_PRIMITIVES_H_

#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

int polygonRGBA(SDL_Renderer* renderer, const Sint16* vx, const Sint16* vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int filledPolygonRGBA(SDL_Renderer* renderer, const Sint16* vx, const Sint16* vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int circleRGBA(SDL_Renderer* renderer, Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

#ifdef __cplusplus
}
#endif

#endif
