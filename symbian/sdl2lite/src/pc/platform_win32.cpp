// sdl2lite platform for Windows: a Win32 window with a WGL (GL 1.1) context, waveOut audio on
// a thread and MIDI through MCI. Used to run the Symbian build of the game on the PC.
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../sdl2lite_platform.h"

static HWND s_hwnd = NULL;
static HDC s_hdc = NULL;
static HGLRC s_glrc = NULL;
static int s_width = 640, s_height = 480;
static LARGE_INTEGER s_freq;
static FILE* s_logFile = NULL;

static SDL_Scancode VkToScancode(WPARAM vk, LPARAM lp)
{
    if (vk >= 'A' && vk <= 'Z')
        return (SDL_Scancode)(SDL_SCANCODE_A + (vk - 'A'));
    if (vk >= '1' && vk <= '9')
        return (SDL_Scancode)(SDL_SCANCODE_1 + (vk - '1'));
    const bool extended = (lp & (1 << 24)) != 0;
    switch (vk)
    {
        case '0': return SDL_SCANCODE_0;
        case VK_RETURN: return extended ? SDL_SCANCODE_KP_ENTER : SDL_SCANCODE_RETURN;
        case VK_ESCAPE: return SDL_SCANCODE_ESCAPE;
        case VK_BACK: return SDL_SCANCODE_BACKSPACE;
        case VK_TAB: return SDL_SCANCODE_TAB;
        case VK_SPACE: return SDL_SCANCODE_SPACE;
        case VK_OEM_3: return SDL_SCANCODE_GRAVE;
        case VK_F1: return SDL_SCANCODE_F1;
        case VK_PRIOR: return SDL_SCANCODE_PAGEUP;
        case VK_NEXT: return SDL_SCANCODE_PAGEDOWN;
        case VK_LEFT: return SDL_SCANCODE_LEFT;
        case VK_RIGHT: return SDL_SCANCODE_RIGHT;
        case VK_UP: return SDL_SCANCODE_UP;
        case VK_DOWN: return SDL_SCANCODE_DOWN;
        case VK_CONTROL: return extended ? SDL_SCANCODE_RCTRL : SDL_SCANCODE_LCTRL;
        case VK_MENU: return extended ? SDL_SCANCODE_RALT : SDL_SCANCODE_LALT;
        case VK_SHIFT: return (((lp >> 16) & 0xFF) == 0x36) ? SDL_SCANCODE_RSHIFT : SDL_SCANCODE_LSHIFT;
        default: return SDL_SCANCODE_UNKNOWN;
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_CLOSE:
            sdl2lite_on_quit();
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            SDL_Scancode sc = VkToScancode(wp, lp);
            if (sc != SDL_SCANCODE_UNKNOWN)
            {
                const bool down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
                const bool repeat = down && (lp & (1 << 30)) != 0;
                sdl2lite_on_key(sc, SDL_GetKeyFromScancode(sc), down, repeat);
            }
            return 0;
        }
        case WM_ACTIVATE:
            sdl2lite_on_focus(LOWORD(wp) != WA_INACTIVE);
            break;
        case WM_SIZE:
            if (LOWORD(lp) > 0 && HIWORD(lp) > 0 && (LOWORD(lp) != s_width || HIWORD(lp) != s_height))
            {
                s_width = LOWORD(lp);
                s_height = HIWORD(lp);
                sdl2lite_on_resize(s_width, s_height);
            }
            break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int plat_video_init(int requestedW, int requestedH, int* outW, int* outH)
{
    const char* env = getenv("SDL2LITE_SIZE");
    if (env)
        sscanf(env, "%dx%d", &requestedW, &requestedH);
    s_width = requestedW;
    s_height = requestedH;

    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "sdl2lite";
    RegisterClassA(&wc);

    RECT r = { 0, 0, requestedW, requestedH };
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    s_hwnd = CreateWindowA("sdl2lite", "OpenClaw", style | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    if (!s_hwnd)
        return 0;
    s_hdc = GetDC(s_hwnd);

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    const int format = ChoosePixelFormat(s_hdc, &pfd);
    SetPixelFormat(s_hdc, format, &pfd);
    s_glrc = wglCreateContext(s_hdc);
    if (!s_glrc || !wglMakeCurrent(s_hdc, s_glrc))
        return 0;

    *outW = s_width;
    *outH = s_height;
    return 1;
}

void plat_video_size(int* w, int* h)
{
    *w = s_width;
    *h = s_height;
}

void plat_video_swap(void)
{
    SwapBuffers(s_hdc);
}

void plat_video_shutdown(void)
{
    wglMakeCurrent(NULL, NULL);
    if (s_glrc)
        wglDeleteContext(s_glrc);
    if (s_hwnd)
        DestroyWindow(s_hwnd);
    s_glrc = NULL;
    s_hwnd = NULL;
}

static void MusicCheckLoop();

void plat_pump_events(void)
{
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    MusicCheckLoop();

    // test runs: SDL2LITE_QUIT_AFTER=<seconds> ends the process (profiling)
    static int quitAfter = -1;
    static Uint64 startUs = 0;
    if (quitAfter < 0)
    {
        const char* env = getenv("SDL2LITE_QUIT_AFTER");
        quitAfter = env ? atoi(env) : 0;
        startUs = plat_ticks_us();
    }
    static bool quitSent = false;
    if (quitAfter > 0 && !quitSent && plat_ticks_us() - startUs > (Uint64)quitAfter * 1000000)
    {
        quitSent = true;
        exit(0);
    }
}

Uint64 plat_ticks_us(void)
{
    if (!s_freq.QuadPart)
        QueryPerformanceFrequency(&s_freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (Uint64)(now.QuadPart * 1000000.0 / s_freq.QuadPart);
}

void plat_delay_ms(Uint32 ms)
{
    Sleep(ms);
}

// Heap accounting for the PC test build (the phone build logs its RHeap size instead)
static size_t s_heapBytes = 0;
static size_t s_heapPeak = 0;

void* operator new(size_t size)
{
    size_t* p = (size_t*)malloc(size + sizeof(size_t) * 2);
    if (!p)
        return NULL;
    p[0] = size;
    s_heapBytes += size;
    if (s_heapBytes > s_heapPeak)
        s_heapPeak = s_heapBytes;
    return p + 2;
}

void* operator new[](size_t size)
{
    return operator new(size);
}

void operator delete(void* ptr)
{
    if (!ptr)
        return;
    size_t* p = (size_t*)ptr - 2;
    s_heapBytes -= p[0];
    free(p);
}

void operator delete[](void* ptr)
{
    operator delete(ptr);
}

void plat_log(const char* text)
{
    char stamp[48];
    sprintf(stamp, "%6u %6uK %6uK ", (unsigned)(plat_ticks_us() / 1000), (unsigned)(s_heapBytes / 1024),
        (unsigned)(s_heapPeak / 1024));
    if (!s_logFile)
        s_logFile = fopen("openclaw.log", "w");
    if (s_logFile)
        fputs(stamp, s_logFile);
    fputs(text, stdout);
    fputc('\n', stdout);
    fflush(stdout);
    if (!s_logFile)
        s_logFile = fopen("openclaw.log", "w");
    if (s_logFile)
    {
        fputs(text, s_logFile);
        fputc('\n', s_logFile);
        fflush(s_logFile);
    }
}

const char* plat_data_dir(void)
{
    return "";
}

//-----------------------------------------------------------------------------
// Audio
//-----------------------------------------------------------------------------

enum { AUDIO_BUFFERS = 4, AUDIO_FRAMES = 1024 };

static HWAVEOUT s_waveOut = NULL;
static WAVEHDR s_headers[AUDIO_BUFFERS];
static Sint16 s_buffers[AUDIO_BUFFERS][AUDIO_FRAMES * 2];
static HANDLE s_audioThread = NULL;
static HANDLE s_audioEvent = NULL;
static volatile LONG s_audioStop = 0;
static CRITICAL_SECTION s_audioLock;

static DWORD WINAPI AudioThread(LPVOID)
{
    while (!s_audioStop)
    {
        WaitForSingleObject(s_audioEvent, 50);
        for (int i = 0; i < AUDIO_BUFFERS; i++)
        {
            if (!(s_headers[i].dwFlags & WHDR_DONE))
                continue;
            EnterCriticalSection(&s_audioLock);
            sdl2lite_mix_audio(s_buffers[i], AUDIO_FRAMES);
            LeaveCriticalSection(&s_audioLock);
            s_headers[i].dwFlags &= ~WHDR_DONE;
            waveOutWrite(s_waveOut, &s_headers[i], sizeof(WAVEHDR));
        }
    }
    return 0;
}

int plat_audio_open(int rate)
{
    InitializeCriticalSection(&s_audioLock);
    WAVEFORMATEX wf;
    memset(&wf, 0, sizeof(wf));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.nSamplesPerSec = rate;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 4;
    wf.nAvgBytesPerSec = rate * 4;
    s_audioEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (waveOutOpen(&s_waveOut, WAVE_MAPPER, &wf, (DWORD_PTR)s_audioEvent, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR)
        return 0;
    for (int i = 0; i < AUDIO_BUFFERS; i++)
    {
        memset(&s_headers[i], 0, sizeof(WAVEHDR));
        memset(s_buffers[i], 0, sizeof(s_buffers[i]));
        s_headers[i].lpData = (LPSTR)s_buffers[i];
        s_headers[i].dwBufferLength = sizeof(s_buffers[i]);
        waveOutPrepareHeader(s_waveOut, &s_headers[i], sizeof(WAVEHDR));
        waveOutWrite(s_waveOut, &s_headers[i], sizeof(WAVEHDR));
    }
    s_audioStop = 0;
    s_audioThread = CreateThread(NULL, 0, AudioThread, NULL, 0, NULL);
    return 1;
}

void plat_audio_close(void)
{
    if (!s_waveOut)
        return;
    s_audioStop = 1;
    WaitForSingleObject(s_audioThread, INFINITE);
    waveOutReset(s_waveOut);
    for (int i = 0; i < AUDIO_BUFFERS; i++)
        waveOutUnprepareHeader(s_waveOut, &s_headers[i], sizeof(WAVEHDR));
    waveOutClose(s_waveOut);
    s_waveOut = NULL;
}

void plat_audio_lock(void)
{
    if (s_waveOut)
        EnterCriticalSection(&s_audioLock);
}

void plat_audio_unlock(void)
{
    if (s_waveOut)
        LeaveCriticalSection(&s_audioLock);
}

//-----------------------------------------------------------------------------
// Music (MCI sequencer on a temp file)
//-----------------------------------------------------------------------------

static bool s_musicOpen = false;
static int s_musicLoops = 0;
static bool s_musicPaused = false;
static int s_musicVolume = 128;

void plat_music_stop(void)
{
    if (s_musicOpen)
    {
        mciSendStringA("close clawmusic", NULL, 0, NULL);
        s_musicOpen = false;
    }
}

void plat_music_play(const void* midi, int size, int loops)
{
    plat_music_stop();
    char path[MAX_PATH];
    GetTempPathA(MAX_PATH, path);
    strcat(path, "openclaw_music.mid");
    FILE* f = fopen(path, "wb");
    if (!f)
        return;
    fwrite(midi, 1, size, f);
    fclose(f);
    char cmd[MAX_PATH + 64];
    sprintf(cmd, "open \"%s\" type sequencer alias clawmusic", path);
    if (mciSendStringA(cmd, NULL, 0, NULL) != 0)
        return;
    s_musicOpen = true;
    s_musicLoops = loops;
    s_musicPaused = false;
    if (s_musicVolume > 0)
        mciSendStringA("play clawmusic from 0", NULL, 0, NULL);
}

static void MusicCheckLoop()
{
    if (!s_musicOpen || s_musicPaused || s_musicLoops == 0 || s_musicVolume == 0)
        return;
    static Uint32 last = 0;
    const Uint32 now = GetTickCount();
    if (now - last < 500)
        return;
    last = now;
    char mode[32] = "";
    mciSendStringA("status clawmusic mode", mode, sizeof(mode), NULL);
    if (strcmp(mode, "stopped") == 0)
    {
        if (s_musicLoops > 0)
            s_musicLoops--;
        mciSendStringA("play clawmusic from 0", NULL, 0, NULL);
    }
}

void plat_music_pause(void)
{
    if (s_musicOpen)
        mciSendStringA("pause clawmusic", NULL, 0, NULL);
    s_musicPaused = true;
}

void plat_music_resume(void)
{
    if (s_musicOpen && s_musicVolume > 0)
        mciSendStringA("resume clawmusic", NULL, 0, NULL);
    s_musicPaused = false;
}

void plat_music_volume(int volume)
{
    s_musicVolume = volume;
}

int plat_music_playing(void)
{
    return s_musicOpen && !s_musicPaused;
}
