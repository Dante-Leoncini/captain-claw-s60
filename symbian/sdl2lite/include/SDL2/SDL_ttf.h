#ifndef SDL2LITE_SDL_TTF_H_
#define SDL2LITE_SDL_TTF_H_

#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

// Built-in bitmap font; the file passed to TTF_OpenFont is ignored.
typedef struct TTF_Font TTF_Font;

int TTF_Init(void);
void TTF_Quit(void);
TTF_Font* TTF_OpenFont(const char* file, int ptsize);
void TTF_CloseFont(TTF_Font* font);
int TTF_FontHeight(const TTF_Font* font);
int TTF_SizeText(TTF_Font* font, const char* text, int* w, int* h);
SDL_Surface* TTF_RenderText_Blended(TTF_Font* font, const char* text, SDL_Color fg);
SDL_Surface* TTF_RenderText_Solid(TTF_Font* font, const char* text, SDL_Color fg);
#define TTF_GetError SDL_GetError

#ifdef __cplusplus
}
#endif

#endif
