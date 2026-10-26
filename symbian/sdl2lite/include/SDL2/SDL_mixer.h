#ifndef SDL2LITE_SDL_MIXER_H_
#define SDL2LITE_SDL_MIXER_H_

#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_S16LSB 0x8010
#define MIX_DEFAULT_FORMAT AUDIO_S16LSB
#define MIX_DEFAULT_FREQUENCY 22050
#define MIX_MAX_VOLUME 128
#define MIX_CHANNELS 8

// Mono samples kept at their own rate and size (signed 8 or 16 bit): resampled while mixing.
typedef struct Mix_Chunk
{
    int allocated;
    Uint8* abuf;
    Uint32 alen;
    Uint8 volume;
    int freq;
    int sampleBytes;
} Mix_Chunk;

typedef struct Mix_Music Mix_Music;

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
void Mix_CloseAudio(void);
int Mix_QuerySpec(int* frequency, Uint16* format, int* channels);
int Mix_AllocateChannels(int numchans);
int Mix_ReserveChannels(int num);
int Mix_GroupChannels(int from, int to, int tag);
int Mix_GroupChannel(int which, int tag);
int Mix_GroupAvailable(int tag);

Mix_Chunk* Mix_LoadWAV_RW(SDL_RWops* src, int freesrc);
void Mix_FreeChunk(Mix_Chunk* chunk);
int Mix_VolumeChunk(Mix_Chunk* chunk, int volume);
int Mix_Volume(int channel, int volume);
int Mix_PlayChannel(int channel, Mix_Chunk* chunk, int loops);
int Mix_HaltChannel(int channel);
void Mix_Pause(int channel);
void Mix_Resume(int channel);
int Mix_Playing(int channel);
int Mix_SetPosition(int channel, Sint16 angle, Uint8 distance);
int Mix_SetDistance(int channel, Uint8 distance);
int Mix_SetPanning(int channel, Uint8 left, Uint8 right);

// Extra stream mixed in on the audio thread (stereo S16 at the device rate), e.g. movie sound
void Mix_HookMusic(void (*mix_func)(void* udata, Uint8* stream, int len), void* arg);

// MIDI music; played by the platform synth.
Mix_Music* Mix_LoadMUS_RW(SDL_RWops* src, int freesrc);
void Mix_FreeMusic(Mix_Music* music);
int Mix_PlayMusic(Mix_Music* music, int loops);
void Mix_PauseMusic(void);
void Mix_ResumeMusic(void);
int Mix_HaltMusic(void);
int Mix_VolumeMusic(int volume);
int Mix_PlayingMusic(void);

#define Mix_GetError SDL_GetError

#ifdef __cplusplus
}
#endif

#endif
