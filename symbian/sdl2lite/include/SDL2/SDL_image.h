#ifndef SDL2LITE_SDL_IMAGE_H_
#define SDL2LITE_SDL_IMAGE_H_

#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { IMG_INIT_PNG = 2 };

int IMG_Init(int flags);
void IMG_Quit(void);
SDL_Surface* IMG_LoadPCX_RW(SDL_RWops* src);
SDL_Surface* IMG_LoadPNG_RW(SDL_RWops* src);
SDL_Surface* IMG_Load(const char* file);
SDL_Texture* IMG_LoadTexture(SDL_Renderer* renderer, const char* file);
#define IMG_GetError SDL_GetError

#ifdef __cplusplus
}
#endif

#endif
