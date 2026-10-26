// What sdl2lite needs from the host platform (implemented in pc/ and symbian/).
#ifndef SDL2LITE_PLATFORM_H_
#define SDL2LITE_PLATFORM_H_

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

// Video: the GL context is current after plat_video_init. Size of the drawable in pixels.
int plat_video_init(int requestedW, int requestedH, int* outW, int* outH);
void plat_video_size(int* w, int* h);
void plat_video_swap(void);
void plat_video_shutdown(void);

// Pumps OS messages; input goes back through the sdl2lite_on_* calls below.
void plat_pump_events(void);

Uint64 plat_ticks_us(void);
void plat_delay_ms(Uint32 ms);

void plat_log(const char* text);

// Directory with the game data, ending in a separator.
const char* plat_data_dir(void);

// Audio: plat_audio_open starts a thread that calls sdl2lite_mix_audio(out, frames) with
// interleaved stereo S16 samples. Lock/unlock guard the mixer state from the game thread.
int plat_audio_open(int rate);
void plat_audio_close(void);
void plat_audio_lock(void);
void plat_audio_unlock(void);

// Standard MIDI file playback by the platform synth. volume 0..128.
void plat_music_play(const void* midi, int size, int loops);
void plat_music_stop(void);
void plat_music_pause(void);
void plat_music_resume(void);
void plat_music_volume(int volume);
int plat_music_playing(void);

// Callbacks into sdl2lite
void sdl2lite_on_key(SDL_Scancode scancode, SDL_Keycode sym, int down, int repeat);
void sdl2lite_on_quit(void);
void sdl2lite_on_focus(int gained);
// the drawable changed size (screen rotation)
void sdl2lite_on_resize(int w, int h);
void sdl2lite_mix_audio(Sint16* out, int frames);

#ifdef __cplusplus
}
#endif

#endif
