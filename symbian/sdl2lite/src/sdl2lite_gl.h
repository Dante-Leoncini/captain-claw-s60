#ifndef SDL2LITE_GL_H_
#define SDL2LITE_GL_H_

#if defined(__SYMBIAN32__)
#include <GLES/gl.h>
#define SDL2LITE_GLES 1
#else
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#endif

#ifndef GL_UNSIGNED_SHORT_4_4_4_4
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#endif
#ifndef GL_UNSIGNED_SHORT_5_5_5_1
#define GL_UNSIGNED_SHORT_5_5_5_1 0x8034
#endif
#ifndef GL_UNSIGNED_SHORT_5_6_5
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifdef SDL2LITE_GLES
#define sdl2lite_glOrtho glOrthof
#else
#define sdl2lite_glOrtho glOrtho
#endif

#endif
