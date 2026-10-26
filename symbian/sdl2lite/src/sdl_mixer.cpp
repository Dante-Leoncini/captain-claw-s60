// sdl2lite mixer: WAV chunks (converted to mono S16 at the device rate when loaded) mixed into
// stereo S16 on the platform audio thread. Music is MIDI, handed to the platform synth.
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "sdl2lite_platform.h"

#include <stdlib.h>
#include <string.h>

enum { MAX_CHANNELS = 32 };

struct Channel
{
    Mix_Chunk* chunk;
    Uint32 pos;         // frame
    Uint32 frac;        // 16 bit fraction of the frame
    int loops;
    bool paused;
    int volume;
    int gainL, gainR;   // 0..256
    int tag;
    bool reserved;
};

static Channel s_channels[MAX_CHANNELS];
static int s_numChannels = 0;
static int s_reserved = 0;
static int s_rate = 22050;
static bool s_open = false;

static void ResetChannel(Channel& c)
{
    c.chunk = NULL;
    c.pos = 0;
    c.frac = 0;
    c.loops = 0;
    c.paused = false;
    c.gainL = c.gainR = 256;
}

int Mix_OpenAudio(int frequency, Uint16, int, int)
{
    if (s_open)
        return 0;
    s_rate = frequency > 22050 ? 22050 : frequency;
    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        ResetChannel(s_channels[i]);
        s_channels[i].volume = MIX_MAX_VOLUME;
        s_channels[i].tag = -1;
        s_channels[i].reserved = false;
    }
    s_numChannels = MIX_CHANNELS;
    if (!plat_audio_open(s_rate))
    {
        SDL_SetError("Could not open the audio device");
        return -1;
    }
    s_open = true;
    return 0;
}

void Mix_CloseAudio(void)
{
    if (!s_open)
        return;
    plat_music_stop();
    plat_audio_close();
    s_open = false;
}

int Mix_QuerySpec(int* frequency, Uint16* format, int* channels)
{
    if (frequency) *frequency = s_rate;
    if (format) *format = AUDIO_S16LSB;
    if (channels) *channels = 2;
    return s_open ? 1 : 0;
}

int Mix_AllocateChannels(int numchans)
{
    if (numchans < 0)
        return s_numChannels;
    if (numchans > MAX_CHANNELS)
        numchans = MAX_CHANNELS;
    plat_audio_lock();
    for (int i = numchans; i < s_numChannels; i++)
        ResetChannel(s_channels[i]);
    s_numChannels = numchans;
    plat_audio_unlock();
    return s_numChannels;
}

int Mix_ReserveChannels(int num)
{
    if (num > s_numChannels)
        num = s_numChannels;
    s_reserved = num;
    for (int i = 0; i < s_numChannels; i++)
        s_channels[i].reserved = i < num;
    return num;
}

int Mix_GroupChannel(int which, int tag)
{
    if (which < 0 || which >= s_numChannels)
        return 0;
    s_channels[which].tag = tag;
    return 1;
}

int Mix_GroupChannels(int from, int to, int tag)
{
    int n = 0;
    for (int i = from; i <= to; i++)
        n += Mix_GroupChannel(i, tag);
    return n;
}

int Mix_GroupAvailable(int tag)
{
    for (int i = 0; i < s_numChannels; i++)
    {
        if ((tag == -1 || s_channels[i].tag == tag) && !s_channels[i].chunk)
            return i;
    }
    return -1;
}

//-----------------------------------------------------------------------------
// WAV loading
//-----------------------------------------------------------------------------

static Uint32 Le32(const Uint8* p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((Uint32)p[3] << 24);
}

static Uint16 Le16(const Uint8* p)
{
    return (Uint16)(p[0] | (p[1] << 8));
}

Mix_Chunk* Mix_LoadWAV_RW(SDL_RWops* src, int freesrc)
{
    if (!src)
        return NULL;
    const Uint8* data = src->here;
    const int size = (int)(src->stop - src->here);
    Mix_Chunk* chunk = NULL;

    if (size < 12 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
    {
        SDL_SetError("Not a WAVE file");
    }
    else
    {
        int fmtTag = 0, channels = 0, rate = 0, bits = 0;
        const Uint8* pcm = NULL;
        Uint32 pcmSize = 0;
        const Uint8* p = data + 12;
        const Uint8* end = data + size;
        while (p + 8 <= end)
        {
            Uint32 len = Le32(p + 4);
            const Uint8* body = p + 8;
            if (body + len > end)
                len = (Uint32)(end - body);
            if (!memcmp(p, "fmt ", 4) && len >= 16)
            {
                fmtTag = Le16(body);
                channels = Le16(body + 2);
                rate = (int)Le32(body + 4);
                bits = Le16(body + 14);
            }
            else if (!memcmp(p, "data", 4))
            {
                pcm = body;
                pcmSize = len;
            }
            p = body + len + (len & 1);
        }

        if (fmtTag != 1 || (bits != 8 && bits != 16) || channels < 1 || channels > 2 || rate <= 0 || !pcm)
        {
            SDL_SetError("Unsupported WAVE (format %d, %d bits, %d channels)", fmtTag, bits, channels);
        }
        else
        {
            const int frameBytes = (bits / 8) * channels;
            const Uint32 frames = pcmSize / frameBytes;
            const int sampleBytes = bits / 8;
            Uint8* out = (Uint8*)malloc(frames ? frames * sampleBytes : 1);
            chunk = (Mix_Chunk*)malloc(sizeof(Mix_Chunk));
            if (!out || !chunk)
            {
                free(out);
                free(chunk);
                chunk = NULL;
            }
            else
            {
                for (Uint32 i = 0; i < frames; i++)
                {
                    int v = 0;
                    for (int c = 0; c < channels; c++)
                    {
                        if (bits == 8)
                            v += (int)pcm[i * frameBytes + c] - 128;
                        else
                            v += (Sint16)Le16(pcm + i * frameBytes + c * 2);
                    }
                    v /= channels;
                    if (bits == 8)
                        ((Sint8*)out)[i] = (Sint8)v;
                    else
                        ((Sint16*)out)[i] = (Sint16)v;
                }
                chunk->allocated = 1;
                chunk->abuf = out;
                chunk->alen = frames * sampleBytes;
                chunk->volume = MIX_MAX_VOLUME;
                chunk->freq = rate;
                chunk->sampleBytes = sampleBytes;
            }
        }
    }

    if (freesrc)
        SDL_RWclose(src);
    return chunk;
}

void Mix_FreeChunk(Mix_Chunk* chunk)
{
    if (!chunk)
        return;
    plat_audio_lock();
    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        if (s_channels[i].chunk == chunk)
            ResetChannel(s_channels[i]);
    }
    plat_audio_unlock();
    if (chunk->allocated)
        free(chunk->abuf);
    free(chunk);
}

int Mix_VolumeChunk(Mix_Chunk* chunk, int volume)
{
    if (!chunk)
        return -1;
    const int prev = chunk->volume;
    if (volume >= 0)
        chunk->volume = (Uint8)(volume > MIX_MAX_VOLUME ? MIX_MAX_VOLUME : volume);
    return prev;
}

int Mix_Volume(int channel, int volume)
{
    if (channel < 0)
    {
        int prev = 0;
        for (int i = 0; i < s_numChannels; i++)
            prev = Mix_Volume(i, volume);
        return prev;
    }
    if (channel >= s_numChannels)
        return 0;
    const int prev = s_channels[channel].volume;
    if (volume >= 0)
        s_channels[channel].volume = volume > MIX_MAX_VOLUME ? MIX_MAX_VOLUME : volume;
    return prev;
}

int Mix_PlayChannel(int channel, Mix_Chunk* chunk, int loops)
{
    if (!chunk || !s_open)
    {
        SDL_SetError("Mix_PlayChannel: no chunk");
        return -1;
    }
    plat_audio_lock();
    if (channel < 0)
    {
        for (int i = s_reserved; i < s_numChannels; i++)
        {
            if (!s_channels[i].chunk)
            {
                channel = i;
                break;
            }
        }
    }
    if (channel < 0 || channel >= s_numChannels)
    {
        plat_audio_unlock();
        SDL_SetError("No free channels available");
        return -1;
    }
    Channel& c = s_channels[channel];
    c.chunk = chunk;
    c.pos = 0;
    c.frac = 0;
    c.loops = loops;
    c.paused = false;
    c.gainL = c.gainR = 256;
    plat_audio_unlock();
    return channel;
}

int Mix_HaltChannel(int channel)
{
    plat_audio_lock();
    for (int i = 0; i < s_numChannels; i++)
    {
        if (channel < 0 || channel == i)
            ResetChannel(s_channels[i]);
    }
    plat_audio_unlock();
    return 0;
}

void Mix_Pause(int channel)
{
    for (int i = 0; i < s_numChannels; i++)
    {
        if (channel < 0 || channel == i)
            s_channels[i].paused = true;
    }
}

void Mix_Resume(int channel)
{
    for (int i = 0; i < s_numChannels; i++)
    {
        if (channel < 0 || channel == i)
            s_channels[i].paused = false;
    }
}

int Mix_Playing(int channel)
{
    if (channel < 0)
    {
        int n = 0;
        for (int i = 0; i < s_numChannels; i++)
            n += s_channels[i].chunk ? 1 : 0;
        return n;
    }
    return (channel < s_numChannels && s_channels[channel].chunk) ? 1 : 0;
}

int Mix_SetPanning(int channel, Uint8 left, Uint8 right)
{
    if (channel < 0 || channel >= s_numChannels)
        return 0;
    s_channels[channel].gainL = left + (left >> 7);
    s_channels[channel].gainR = right + (right >> 7);
    return 1;
}

int Mix_SetPosition(int channel, Sint16 angle, Uint8 distance)
{
    if (channel < 0 || channel >= s_numChannels)
        return 0;
    angle %= 360;
    if (angle < 0)
        angle += 360;
    // 90 = right, 270 = left; full side only drops the far ear to a quarter
    int left = 256, right = 256;
    if (angle > 0 && angle < 180)
        left = 256 - ((angle < 90 ? angle : 180 - angle) * 192) / 90;
    else if (angle > 180)
        right = 256 - ((angle < 270 ? angle - 180 : 360 - angle) * 192) / 90;
    const int att = 256 - distance;
    s_channels[channel].gainL = (left * att) >> 8;
    s_channels[channel].gainR = (right * att) >> 8;
    return 1;
}

int Mix_SetDistance(int channel, Uint8 distance)
{
    return Mix_SetPosition(channel, 0, distance);
}

//-----------------------------------------------------------------------------
// Mixing (audio thread, lock held by the platform)
//-----------------------------------------------------------------------------

extern "C" typedef void (*MixHookFunction)(void*, Uint8*, int);
static MixHookFunction s_musicHook = NULL;
static void* s_musicHookArg = NULL;

extern "C" void Mix_HookMusic(void (*mix_func)(void* udata, Uint8* stream, int len), void* arg)
{
    plat_audio_lock();
    s_musicHook = mix_func;
    s_musicHookArg = arg;
    plat_audio_unlock();
}

void SDL_LockAudio(void)
{
    plat_audio_lock();
}

void SDL_UnlockAudio(void)
{
    plat_audio_unlock();
}

void sdl2lite_mix_audio(Sint16* out, int frames)
{
    // the hooked stream first, the effects are added on top
    static Sint16 hookBuffer[2048 * 2];
    static int acc[4096];
    while (frames > 0)
    {
        const int n = frames > 2048 ? 2048 : frames;
        memset(acc, 0, n * 2 * sizeof(int));
        if (s_musicHook)
        {
            memset(hookBuffer, 0, n * 4);
            s_musicHook(s_musicHookArg, (Uint8*)hookBuffer, n * 4);
            for (int k = 0; k < n * 2; k++)
                acc[k] = hookBuffer[k];
        }
        for (int ci = 0; ci < s_numChannels; ci++)
        {
            Channel& c = s_channels[ci];
            if (!c.chunk || c.paused)
                continue;
            const Uint32 total = c.chunk->alen / c.chunk->sampleBytes;
            const Uint32 step = (Uint32)(((Uint64)c.chunk->freq << 16) / s_rate);
            // volume 0..128 * 0..128 * gain 0..256 -> shift by 22
            const int vol = c.chunk->volume * c.volume;
            const int gl = (vol * c.gainL) >> 8;
            const int gr = (vol * c.gainR) >> 8;
            int* a = acc;
            for (int k = 0; k < n; k++)
            {
                if (c.pos >= total)
                {
                    if (c.loops != 0 && total > 0)
                    {
                        if (c.loops > 0)
                            c.loops--;
                        c.pos = 0;
                        c.frac = 0;
                    }
                    else
                    {
                        ResetChannel(c);
                        break;
                    }
                }
                const int v = c.chunk->sampleBytes == 1
                    ? ((const Sint8*)c.chunk->abuf)[c.pos] << 8
                    : ((const Sint16*)c.chunk->abuf)[c.pos];
                a[k * 2] += (v * gl) >> 14;
                a[k * 2 + 1] += (v * gr) >> 14;
                c.frac += step;
                c.pos += c.frac >> 16;
                c.frac &= 0xFFFF;
            }
        }
        for (int k = 0; k < n * 2; k++)
        {
            int v = acc[k];
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            out[k] = (Sint16)v;
        }
        out += n * 2;
        frames -= n;
    }
}

//-----------------------------------------------------------------------------
// Music
//-----------------------------------------------------------------------------

struct Mix_Music
{
    Uint8* data;
    int size;
};

static int s_musicVolume = MIX_MAX_VOLUME;

Mix_Music* Mix_LoadMUS_RW(SDL_RWops* src, int freesrc)
{
    if (!src)
        return NULL;
    const int size = (int)(src->stop - src->here);
    Mix_Music* m = (Mix_Music*)malloc(sizeof(Mix_Music));
    if (m)
    {
        m->data = (Uint8*)malloc(size);
        m->size = size;
        if (m->data)
        {
            memcpy(m->data, src->here, size);
        }
        else
        {
            free(m);
            m = NULL;
        }
    }
    if (freesrc)
        SDL_RWclose(src);
    return m;
}

static Mix_Music* s_currentMusic = NULL;

void Mix_FreeMusic(Mix_Music* music)
{
    if (!music)
        return;
    if (music == s_currentMusic)
    {
        plat_music_stop();
        s_currentMusic = NULL;
    }
    free(music->data);
    free(music);
}

int Mix_PlayMusic(Mix_Music* music, int loops)
{
    if (!music)
        return -1;
    // the previous track is released here: callers never free it
    if (s_currentMusic && s_currentMusic != music)
    {
        plat_music_stop();
        Mix_Music* old = s_currentMusic;
        s_currentMusic = NULL;
        Mix_FreeMusic(old);
    }
    s_currentMusic = music;
    plat_music_volume(s_musicVolume);
    plat_music_play(music->data, music->size, loops);
    return 0;
}

void Mix_PauseMusic(void)
{
    plat_music_pause();
}

void Mix_ResumeMusic(void)
{
    plat_music_resume();
}

int Mix_HaltMusic(void)
{
    plat_music_stop();
    return 0;
}

int Mix_VolumeMusic(int volume)
{
    const int prev = s_musicVolume;
    if (volume >= 0)
    {
        s_musicVolume = volume > MIX_MAX_VOLUME ? MIX_MAX_VOLUME : volume;
        plat_music_volume(s_musicVolume);
    }
    return prev;
}

int Mix_PlayingMusic(void)
{
    return plat_music_playing();
}
