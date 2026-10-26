// sdl2lite: the subset of the SDL2 API used by OpenClaw, for platforms without SDL2
// (Symbian S60v3). Values match SDL2 where they matter.
#ifndef SDL2LITE_SDL_H_
#define SDL2LITE_SDL_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t Uint8;
typedef int8_t Sint8;
typedef uint16_t Uint16;
typedef int16_t Sint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef uint64_t Uint64;
typedef int64_t Sint64;

typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER SDL_LIL_ENDIAN

#define SDL_INIT_TIMER    0x00000001u
#define SDL_INIT_AUDIO    0x00000010u
#define SDL_INIT_VIDEO    0x00000020u
#define SDL_INIT_EVENTS   0x00004000u
#define SDL_INIT_EVERYTHING 0x0000FFFFu

int SDL_Init(Uint32 flags);
int SDL_InitSubSystem(Uint32 flags);
Uint32 SDL_WasInit(Uint32 flags);
void SDL_Quit(void);
void SDL_SetMainReady(void);
const char* SDL_GetError(void);
int SDL_SetError(const char* fmt, ...);
char* SDL_GetBasePath(void);
void SDL_free(void* p);

// Log
enum { SDL_LOG_CATEGORY_APPLICATION = 0, SDL_LOG_CATEGORY_ERROR, SDL_LOG_CATEGORY_SYSTEM };
void SDL_Log(const char* fmt, ...);
void SDL_LogInfo(int category, const char* fmt, ...);
void SDL_LogWarn(int category, const char* fmt, ...);
void SDL_LogError(int category, const char* fmt, ...);

// Timer
Uint32 SDL_GetTicks(void);
Uint64 SDL_GetPerformanceCounter(void);
Uint64 SDL_GetPerformanceFrequency(void);
void SDL_Delay(Uint32 ms);

// Rect
typedef struct SDL_Point { int x, y; } SDL_Point;
typedef struct SDL_Rect { int x, y; int w, h; } SDL_Rect;

SDL_bool SDL_HasIntersection(const SDL_Rect* a, const SDL_Rect* b);
SDL_bool SDL_IntersectRect(const SDL_Rect* a, const SDL_Rect* b, SDL_Rect* result);

// Pixels / surfaces
typedef struct SDL_Color { Uint8 r, g, b, a; } SDL_Color;

#define SDL_PIXELFORMAT_ARGB8888 0x16362004u
#define SDL_PIXELFORMAT_RGBA8888 0x16462004u
#define SDL_PIXELFORMAT_ABGR8888 0x16762004u
#define SDL_PIXELFORMAT_RGB565 0x15151002u

typedef struct SDL_PixelFormat
{
    Uint32 format;
    Uint8 BitsPerPixel;
    Uint8 BytesPerPixel;
    Uint32 Rmask, Gmask, Bmask, Amask;
    Uint8 Rshift, Gshift, Bshift, Ashift;
} SDL_PixelFormat;

// Pixels are always 32-bit, byte order R,G,B,A in memory.
typedef struct SDL_Surface
{
    Uint32 flags;
    SDL_PixelFormat* format;
    int w, h;
    int pitch;
    void* pixels;
    int colorkey_enabled;
    Uint32 colorkey;
    int refcount;
    // set for images that came from a palette (see SDL_SetColorKey)
    Uint32* palette;
    int paletteSize;
} SDL_Surface;

SDL_Surface* SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
    Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
void SDL_FreeSurface(SDL_Surface* surface);
int SDL_FillRect(SDL_Surface* dst, const SDL_Rect* rect, Uint32 color);
int SDL_SetColorKey(SDL_Surface* surface, int flag, Uint32 key);
Uint32 SDL_MapRGB(const SDL_PixelFormat* format, Uint8 r, Uint8 g, Uint8 b);
Uint32 SDL_MapRGBA(const SDL_PixelFormat* format, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

// RWops (memory only)
typedef struct SDL_RWops
{
    const Uint8* base;
    const Uint8* here;
    const Uint8* stop;
} SDL_RWops;

SDL_RWops* SDL_RWFromMem(void* mem, int size);
SDL_RWops* SDL_RWFromConstMem(const void* mem, int size);
void SDL_FreeRW(SDL_RWops* rw);
int SDL_RWclose(SDL_RWops* rw);

// Video
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;

#define SDL_WINDOWPOS_UNDEFINED 0x1FFF0000u
#define SDL_WINDOWPOS_CENTERED  0x2FFF0000u

enum
{
    SDL_WINDOW_FULLSCREEN = 0x00000001,
    SDL_WINDOW_OPENGL = 0x00000002,
    SDL_WINDOW_SHOWN = 0x00000004,
    SDL_WINDOW_HIDDEN = 0x00000008,
    SDL_WINDOW_RESIZABLE = 0x00000020,
    SDL_WINDOW_FULLSCREEN_DESKTOP = (SDL_WINDOW_FULLSCREEN | 0x00001000)
};

enum
{
    SDL_RENDERER_SOFTWARE = 0x00000001,
    SDL_RENDERER_ACCELERATED = 0x00000002,
    SDL_RENDERER_PRESENTVSYNC = 0x00000004,
    SDL_RENDERER_TARGETTEXTURE = 0x00000008
};

typedef enum { SDL_FLIP_NONE = 0, SDL_FLIP_HORIZONTAL = 1, SDL_FLIP_VERTICAL = 2 } SDL_RendererFlip;

typedef enum { SDL_TEXTUREACCESS_STATIC, SDL_TEXTUREACCESS_STREAMING, SDL_TEXTUREACCESS_TARGET } SDL_TextureAccess;

SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int w, int h, Uint32 flags);
void SDL_DestroyWindow(SDL_Window* window);
void SDL_GetWindowSize(SDL_Window* window, int* w, int* h);
void SDL_SetWindowSize(SDL_Window* window, int w, int h);
Uint32 SDL_GetWindowFlags(SDL_Window* window);
int SDL_SetWindowFullscreen(SDL_Window* window, Uint32 flags);

SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, int index, Uint32 flags);
void SDL_DestroyRenderer(SDL_Renderer* renderer);
int SDL_RenderSetScale(SDL_Renderer* renderer, float scaleX, float scaleY);
void SDL_RenderGetScale(SDL_Renderer* renderer, float* scaleX, float* scaleY);
int SDL_SetRenderDrawColor(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int SDL_GetRenderDrawColor(SDL_Renderer* renderer, Uint8* r, Uint8* g, Uint8* b, Uint8* a);
int SDL_RenderClear(SDL_Renderer* renderer);
int SDL_RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect* srcrect, const SDL_Rect* dstrect);
int SDL_RenderCopyEx(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect* srcrect, const SDL_Rect* dstrect,
    const double angle, const SDL_Point* center, const SDL_RendererFlip flip);
int SDL_RenderFillRect(SDL_Renderer* renderer, const SDL_Rect* rect);
int SDL_RenderDrawRect(SDL_Renderer* renderer, const SDL_Rect* rect);
int SDL_RenderDrawLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2);
int SDL_RenderDrawPoint(SDL_Renderer* renderer, int x, int y);
int SDL_RenderReadPixels(SDL_Renderer* renderer, const SDL_Rect* rect, Uint32 format, void* pixels, int pitch);
void SDL_RenderPresent(SDL_Renderer* renderer);

SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* surface);
// Streaming textures: SDL_PIXELFORMAT_RGB565 only
SDL_Texture* SDL_CreateTexture(SDL_Renderer* renderer, Uint32 format, int access, int w, int h);
int SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rect, const void* pixels, int pitch);
void SDL_DestroyTexture(SDL_Texture* texture);
int SDL_QueryTexture(SDL_Texture* texture, Uint32* format, int* access, int* w, int* h);
int SDL_SetTextureColorMod(SDL_Texture* texture, Uint8 r, Uint8 g, Uint8 b);
int SDL_SetTextureAlphaMod(SDL_Texture* texture, Uint8 alpha);

// Keyboard
typedef enum
{
    SDL_SCANCODE_UNKNOWN = 0,
    SDL_SCANCODE_A = 4, SDL_SCANCODE_B = 5, SDL_SCANCODE_C = 6, SDL_SCANCODE_D = 7, SDL_SCANCODE_E = 8,
    SDL_SCANCODE_F = 9, SDL_SCANCODE_G = 10, SDL_SCANCODE_H = 11, SDL_SCANCODE_I = 12, SDL_SCANCODE_J = 13,
    SDL_SCANCODE_K = 14, SDL_SCANCODE_L = 15, SDL_SCANCODE_M = 16, SDL_SCANCODE_N = 17, SDL_SCANCODE_O = 18,
    SDL_SCANCODE_P = 19, SDL_SCANCODE_Q = 20, SDL_SCANCODE_R = 21, SDL_SCANCODE_S = 22, SDL_SCANCODE_T = 23,
    SDL_SCANCODE_U = 24, SDL_SCANCODE_V = 25, SDL_SCANCODE_W = 26, SDL_SCANCODE_X = 27, SDL_SCANCODE_Y = 28,
    SDL_SCANCODE_Z = 29,
    SDL_SCANCODE_1 = 30, SDL_SCANCODE_2 = 31, SDL_SCANCODE_3 = 32, SDL_SCANCODE_4 = 33, SDL_SCANCODE_5 = 34,
    SDL_SCANCODE_6 = 35, SDL_SCANCODE_7 = 36, SDL_SCANCODE_8 = 37, SDL_SCANCODE_9 = 38, SDL_SCANCODE_0 = 39,
    SDL_SCANCODE_RETURN = 40, SDL_SCANCODE_ESCAPE = 41, SDL_SCANCODE_BACKSPACE = 42, SDL_SCANCODE_TAB = 43,
    SDL_SCANCODE_SPACE = 44, SDL_SCANCODE_GRAVE = 53,
    SDL_SCANCODE_F1 = 58,
    SDL_SCANCODE_PAGEUP = 75, SDL_SCANCODE_PAGEDOWN = 78,
    SDL_SCANCODE_RIGHT = 79, SDL_SCANCODE_LEFT = 80, SDL_SCANCODE_DOWN = 81, SDL_SCANCODE_UP = 82,
    SDL_SCANCODE_KP_ENTER = 88,
    SDL_SCANCODE_LCTRL = 224, SDL_SCANCODE_LSHIFT = 225, SDL_SCANCODE_LALT = 226,
    SDL_SCANCODE_RCTRL = 228, SDL_SCANCODE_RSHIFT = 229, SDL_SCANCODE_RALT = 230,
    SDL_NUM_SCANCODES = 512
} SDL_Scancode;

typedef Sint32 SDL_Keycode;

#define SDLK_SCANCODE_MASK (1 << 30)
#define SDL_SCANCODE_TO_KEYCODE(X) (X | SDLK_SCANCODE_MASK)

enum
{
    SDLK_UNKNOWN = 0,
    SDLK_RETURN = '\r', SDLK_ESCAPE = '\033', SDLK_BACKSPACE = '\b', SDLK_TAB = '\t', SDLK_SPACE = ' ',
    SDLK_BACKQUOTE = '`',
    SDLK_0 = '0', SDLK_1 = '1', SDLK_2 = '2', SDLK_3 = '3', SDLK_4 = '4',
    SDLK_5 = '5', SDLK_6 = '6', SDLK_7 = '7', SDLK_8 = '8', SDLK_9 = '9',
    SDLK_a = 'a', SDLK_b = 'b', SDLK_c = 'c', SDLK_d = 'd', SDLK_e = 'e', SDLK_f = 'f', SDLK_g = 'g',
    SDLK_h = 'h', SDLK_i = 'i', SDLK_j = 'j', SDLK_k = 'k', SDLK_l = 'l', SDLK_m = 'm', SDLK_n = 'n',
    SDLK_o = 'o', SDLK_p = 'p', SDLK_q = 'q', SDLK_r = 'r', SDLK_s = 's', SDLK_t = 't', SDLK_u = 'u',
    SDLK_v = 'v', SDLK_w = 'w', SDLK_x = 'x', SDLK_y = 'y', SDLK_z = 'z',
    SDLK_F1 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F1),
    SDLK_PAGEUP = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PAGEUP),
    SDLK_PAGEDOWN = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PAGEDOWN),
    SDLK_RIGHT = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RIGHT),
    SDLK_LEFT = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LEFT),
    SDLK_DOWN = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_DOWN),
    SDLK_UP = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_UP),
    SDLK_KP_ENTER = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_ENTER),
    SDLK_LCTRL = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LCTRL),
    SDLK_LSHIFT = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LSHIFT),
    SDLK_LALT = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LALT),
    SDLK_RCTRL = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RCTRL),
    SDLK_RSHIFT = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RSHIFT),
    SDLK_RALT = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RALT)
};

typedef struct SDL_Keysym
{
    SDL_Scancode scancode;
    SDL_Keycode sym;
    Uint16 mod;
} SDL_Keysym;

const Uint8* SDL_GetKeyboardState(int* numkeys);
SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode key);
SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode scancode);
void SDL_StartTextInput(void);
void SDL_StopTextInput(void);

// Events
typedef enum
{
    SDL_FIRSTEVENT = 0,
    SDL_QUIT = 0x100,
    SDL_APP_TERMINATING, SDL_APP_LOWMEMORY, SDL_APP_WILLENTERBACKGROUND, SDL_APP_DIDENTERBACKGROUND,
    SDL_APP_WILLENTERFOREGROUND, SDL_APP_DIDENTERFOREGROUND,
    SDL_WINDOWEVENT = 0x200,
    SDL_KEYDOWN = 0x300, SDL_KEYUP, SDL_TEXTEDITING, SDL_TEXTINPUT,
    SDL_MOUSEMOTION = 0x400, SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP, SDL_MOUSEWHEEL,
    SDL_FINGERDOWN = 0x700, SDL_FINGERUP, SDL_FINGERMOTION,
    SDL_USEREVENT = 0x8000,
    SDL_LASTEVENT = 0xFFFF
} SDL_EventType;

typedef enum
{
    SDL_WINDOWEVENT_NONE, SDL_WINDOWEVENT_SHOWN, SDL_WINDOWEVENT_HIDDEN, SDL_WINDOWEVENT_EXPOSED,
    SDL_WINDOWEVENT_MOVED, SDL_WINDOWEVENT_RESIZED, SDL_WINDOWEVENT_SIZE_CHANGED, SDL_WINDOWEVENT_MINIMIZED,
    SDL_WINDOWEVENT_MAXIMIZED, SDL_WINDOWEVENT_RESTORED, SDL_WINDOWEVENT_ENTER, SDL_WINDOWEVENT_LEAVE,
    SDL_WINDOWEVENT_FOCUS_GAINED, SDL_WINDOWEVENT_FOCUS_LOST, SDL_WINDOWEVENT_CLOSE
} SDL_WindowEventID;

#define SDL_BUTTON_LEFT 1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT 3

typedef Sint64 SDL_TouchID;
typedef Sint64 SDL_FingerID;

typedef struct SDL_CommonEvent { Uint32 type; Uint32 timestamp; } SDL_CommonEvent;
typedef struct SDL_WindowEvent
{
    Uint32 type; Uint32 timestamp; Uint32 windowID; Uint8 event; Uint8 padding1, padding2, padding3;
    Sint32 data1; Sint32 data2;
} SDL_WindowEvent;
typedef struct SDL_KeyboardEvent
{
    Uint32 type; Uint32 timestamp; Uint32 windowID; Uint8 state; Uint8 repeat; Uint8 padding2, padding3;
    SDL_Keysym keysym;
} SDL_KeyboardEvent;
#define SDL_TEXTINPUTEVENT_TEXT_SIZE 32
typedef struct SDL_TextInputEvent
{
    Uint32 type; Uint32 timestamp; Uint32 windowID; char text[SDL_TEXTINPUTEVENT_TEXT_SIZE];
} SDL_TextInputEvent;
typedef struct SDL_MouseMotionEvent
{
    Uint32 type; Uint32 timestamp; Uint32 windowID; Uint32 which; Uint32 state; Sint32 x, y, xrel, yrel;
} SDL_MouseMotionEvent;
typedef struct SDL_MouseButtonEvent
{
    Uint32 type; Uint32 timestamp; Uint32 windowID; Uint32 which; Uint8 button; Uint8 state; Uint8 clicks;
    Uint8 padding1; Sint32 x, y;
} SDL_MouseButtonEvent;
typedef struct SDL_MouseWheelEvent
{
    Uint32 type; Uint32 timestamp; Uint32 windowID; Uint32 which; Sint32 x, y; Uint32 direction;
} SDL_MouseWheelEvent;
typedef struct SDL_TouchFingerEvent
{
    Uint32 type; Uint32 timestamp; SDL_TouchID touchId; SDL_FingerID fingerId;
    float x, y, dx, dy, pressure;
} SDL_TouchFingerEvent;
typedef struct SDL_QuitEvent { Uint32 type; Uint32 timestamp; } SDL_QuitEvent;
typedef struct SDL_UserEvent
{
    Uint32 type; Uint32 timestamp; Uint32 windowID; Sint32 code; void* data1; void* data2;
} SDL_UserEvent;

typedef union SDL_Event
{
    Uint32 type;
    SDL_CommonEvent common;
    SDL_WindowEvent window;
    SDL_KeyboardEvent key;
    SDL_TextInputEvent text;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_MouseWheelEvent wheel;
    SDL_TouchFingerEvent tfinger;
    SDL_QuitEvent quit;
    SDL_UserEvent user;
    Uint8 padding[56];
} SDL_Event;

int SDL_PollEvent(SDL_Event* event);
void SDL_PumpEvents(void);
int SDL_PushEvent(SDL_Event* event);

// Audio device lock (the mixer runs on the platform audio thread)
void SDL_LockAudio(void);
void SDL_UnlockAudio(void);

// Threads (only used by the PC music path; never on Symbian)
typedef struct SDL_Thread SDL_Thread;
typedef int (*SDL_ThreadFunction)(void* data);
typedef enum { SDL_THREAD_PRIORITY_LOW, SDL_THREAD_PRIORITY_NORMAL, SDL_THREAD_PRIORITY_HIGH } SDL_ThreadPriority;
SDL_Thread* SDL_CreateThread(SDL_ThreadFunction fn, const char* name, void* data);
void SDL_DetachThread(SDL_Thread* thread);
int SDL_SetThreadPriority(SDL_ThreadPriority priority);

#ifdef __cplusplus
}
#endif

#endif
