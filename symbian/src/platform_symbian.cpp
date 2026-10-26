// sdl2lite platform for S60 3rd Edition (Nokia N95): EGL window owned by the container, keys
// from the container, CMdaAudioOutputStream on its own thread for the mixer and
// CMidiClientUtility for the music.
#include "OpenClawApp.h"
#include "OpenClawSymbian.h"
#include "../sdl2lite/src/sdl2lite_platform.h"

#include <e32base.h>
#include <e32keys.h>
#include <hal.h>
#include <f32file.h>
#include <MdaAudioOutputStream.h>
#include <mda/common/audio.h>
#ifndef OPENCLAW_NO_MIDI
#include <midiclientutility.h>
#endif
#include <remconcoreapitarget.h>
#include <remconcoreapitargetobserver.h>
#include <remconinterfaceselector.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static COpenClawContainer* s_container = NULL;
static char s_dataDir[32] = "e:/data/claw/";
// native file in shared mode: the log can be read over the cable while the game runs
static RFs s_fs;
static RFile s_log;
static TBool s_logOpen = EFalse;
static TFileName s_logPath;
// until the game runs frames, every line is written at once (a crash at start leaves its trail)
static TBool s_logEveryLine = ETrue;

static TInt s_counterFreq = 0;
static TBool s_counterUp = ETrue;
static TUint32 s_lastCounter = 0;
static Uint64 s_ticks = 0;

static void MusicShutdown();
static void FlushLog();
static void MusicVolumeLevel(TInt aLevel);

// Phone volume, 0..10 steps: the side keys change it, the audio thread applies it to the stream
enum { KVolumeSteps = 10 };
static volatile TInt s_volumeLevel = 9;
// set while the game is in the background: the audio thread outputs silence
static volatile TInt s_audioSilent = 0;

//-----------------------------------------------------------------------------
// Volume keys: on S60 they arrive through RemCon, not as key events
//-----------------------------------------------------------------------------

class CVolumeKeys : public CBase, public MRemConCoreApiTargetObserver
{
public:
    static CVolumeKeys* NewL()
    {
        CVolumeKeys* self = new (ELeave) CVolumeKeys;
        CleanupStack::PushL(self);
        self->iSelector = CRemConInterfaceSelector::NewL();
        self->iTarget = CRemConCoreApiTarget::NewL(*self->iSelector, *self);
        self->iSelector->OpenTargetL();
        CleanupStack::Pop(self);
        return self;
    }

    // (the selector owns the target)
    ~CVolumeKeys()
    {
        delete iSelector;
    }

    void MrccatoCommand(TRemConCoreApiOperationId aOperationId, TRemConCoreApiButtonAction aButtonAct)
    {
        TInt step = 0;
        TRequestStatus status;
        if (aOperationId == ERemConCoreApiVolumeUp)
        {
            step = 1;
            iTarget->VolumeUpResponse(status, KErrNone);
            User::WaitForRequest(status);
        }
        else if (aOperationId == ERemConCoreApiVolumeDown)
        {
            step = -1;
            iTarget->VolumeDownResponse(status, KErrNone);
            User::WaitForRequest(status);
        }
        if (!step)
            return;
        if (aButtonAct == ERemConCoreApiButtonRelease)
        {
            iHeld = 0;
            return;
        }
        Change(step);
        iHeld = aButtonAct == ERemConCoreApiButtonPress ? step : 0;
        iNextRepeat = plat_ticks_us() + 500000;
    }

    // every frame: a held key repeats
    void Repeat()
    {
        if (!iHeld || plat_ticks_us() < iNextRepeat)
            return;
        Change(iHeld);
        iNextRepeat = plat_ticks_us() + 150000;
    }

private:
    CVolumeKeys() : iSelector(NULL), iTarget(NULL), iHeld(0), iNextRepeat(0) {}

    void Change(TInt aStep)
    {
        TInt level = s_volumeLevel + aStep;
        if (level < 0)
            level = 0;
        if (level > KVolumeSteps)
            level = KVolumeSteps;
        s_volumeLevel = level;
        MusicVolumeLevel(level);
    }

    CRemConInterfaceSelector* iSelector;
    CRemConCoreApiTarget* iTarget;
    TInt iHeld;
    Uint64 iNextRepeat;
};

static CVolumeKeys* s_volumeKeys = NULL;

#ifdef OPENCLAW_FRAME_PROFILE
// Logs the game clock against the real time clock and the kernel tick every 10 s
static void CheckClock()
{
    static Uint64 startUs = 0;
    static TTime startTime;
    static TUint32 startTick = 0;
    static Uint64 nextUs = 0;
    const Uint64 nowUs = plat_ticks_us();
    if (startUs == 0)
    {
        startUs = nowUs;
        startTime.UniversalTime();
        startTick = User::NTickCount();
        nextUs = nowUs + 10000000;
        return;
    }
    if (nowUs < nextUs)
        return;
    nextUs = nowUs + 10000000;
    TTime now;
    now.UniversalTime();
    const TInt64 rtcUs = now.MicroSecondsFrom(startTime).Int64();
    char msg[128];
    sprintf(msg, "[clock] game %u ms, rtc %u ms, ntick %u, counter %d Hz", (unsigned)((nowUs - startUs) / 1000),
        (unsigned)(rtcUs / 1000), (unsigned)(User::NTickCount() - startTick), (int)s_counterFreq);
    plat_log(msg);
}
#endif

void SymbianPlatformFrame()
{
#ifdef OPENCLAW_FRAME_PROFILE
    CheckClock();
#endif
    s_logEveryLine = EFalse;
    if (s_volumeKeys)
        s_volumeKeys->Repeat();
}

//-----------------------------------------------------------------------------
// Keys
//-----------------------------------------------------------------------------

struct KeyMap
{
    TInt scanCode;
    SDL_Scancode sdl;
};

// D-pad moves, centre jumps (and confirms in menus), 1 attacks, 3 fires, 7 switches ammo,
// softkeys / C open the in-game menu or go back.
static const KeyMap kKeys[] =
{
    { EStdKeyUpArrow, SDL_SCANCODE_UP },
    { EStdKeyDownArrow, SDL_SCANCODE_DOWN },
    { EStdKeyLeftArrow, SDL_SCANCODE_LEFT },
    { EStdKeyRightArrow, SDL_SCANCODE_RIGHT },
    { '2', SDL_SCANCODE_UP },
    { '8', SDL_SCANCODE_DOWN },
    { '4', SDL_SCANCODE_LEFT },
    { '6', SDL_SCANCODE_RIGHT },
    { EStdKeyDevice3, SDL_SCANCODE_SPACE },
    { EStdKeyEnter, SDL_SCANCODE_SPACE },
    { '5', SDL_SCANCODE_SPACE },
    { '1', SDL_SCANCODE_LCTRL },
    { '0', SDL_SCANCODE_LCTRL },
    { EStdKeyYes, SDL_SCANCODE_LCTRL },
    { '3', SDL_SCANCODE_LALT },
    { EStdKeyHash, SDL_SCANCODE_LALT },
    { '7', SDL_SCANCODE_LSHIFT },
    { '*', SDL_SCANCODE_LSHIFT },
    { EStdKeyNkpAsterisk, SDL_SCANCODE_LSHIFT },
    { EStdKeyDevice0, SDL_SCANCODE_ESCAPE },
    { EStdKeyDevice1, SDL_SCANCODE_ESCAPE },
    { EStdKeyBackspace, SDL_SCANCODE_ESCAPE },
};

static int FindKey(TInt aScanCode)
{
    for (unsigned i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); i++)
    {
        if (kKeys[i].scanCode == aScanCode)
            return (int)i;
    }
    return -1;
}

TBool SymbianPlatformIsGameKey(TInt aScanCode)
{
    return FindKey(aScanCode) >= 0;
}

TBool SymbianPlatformKey(TInt aScanCode, TBool aDown)
{
    const int i = FindKey(aScanCode);
    if (i < 0)
        return EFalse;
    const SDL_Scancode sc = kKeys[i].sdl;
    sdl2lite_on_key(sc, SDL_GetKeyFromScancode(sc), aDown ? 1 : 0, 0);
    return ETrue;
}

//-----------------------------------------------------------------------------
// Init
//-----------------------------------------------------------------------------

static bool FileExists(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

// RFs::Close() is an efsrv export (ordinal 390) only since Symbian^3: S60 3rd phones can't load an
// exe that imports it. The session close from euser does the same.
static void CloseFs(RFs& aFs)
{
    static_cast<RSessionBase&>(aFs).Close();
}

// Remote tests: a quit.txt dropped in the data folder over the cable ends the process, even
// while the game is busy loading.
static TInt QuitWatchThread(TAny*)
{
    RFs fs;
    if (fs.Connect() != KErrNone)
        return KErrNone;
    _LIT(KQuitFile, "E:\\data\\claw\\quit.txt");
    for (;;)
    {
        User::After(1000000);
        TEntry entry;
        if (fs.Entry(KQuitFile, entry) == KErrNone)
        {
            fs.Delete(KQuitFile);
            CloseFs(fs);
            RProcess().Kill(KErrNone);
        }
    }
}

static void StartQuitWatch()
{
    RThread thread;
    _LIT(KQuitThreadName, "OpenClawQuitWatch");
    if (thread.Create(KQuitThreadName, QuitWatchThread, 0x2000, NULL, NULL) == KErrNone)
    {
        thread.SetPriority(EPriorityLess);
        thread.Resume();
        thread.Close();
    }
}

void SymbianPlatformInit(COpenClawContainer* aContainer)
{
    s_container = aContainer;
    StartQuitWatch();
    TRAPD(volumeError, s_volumeKeys = CVolumeKeys::NewL());
    if (volumeError != KErrNone)
        s_volumeKeys = NULL;
    HAL::Get(HALData::EFastCounterFrequency, s_counterFreq);
    TInt up = 1;
    HAL::Get(HALData::EFastCounterCountsUp, up);
    s_counterUp = up != 0;
    s_lastCounter = User::FastCounter();

    // the original game data goes to <drive>:\data\claw\ (E: first)
    const char drives[] = { 'e', 'f', 'c' };
    for (unsigned i = 0; i < sizeof(drives); i++)
    {
        char path[48];
        sprintf(path, "%c:/data/claw/CLAW.REZ", drives[i]);
        if (FileExists(path))
        {
            sprintf(s_dataDir, "%c:/data/claw/", drives[i]);
            break;
        }
    }
    // config.xml, CLAW.REZ, ASSETS.ZIP and the saves are opened relative to the data folder
    chdir(s_dataDir);
    char logPath[48];
    sprintf(logPath, "%sopenclaw.log", s_dataDir);
    if (s_fs.Connect() == KErrNone)
    {
        TBuf<48> path;
        path.Copy(TPtrC8((const TUint8*)logPath));
        for (TInt i = 0; i < path.Length(); i++)
        {
            if (path[i] == '/')
                path[i] = '\\';
        }
        s_logPath = path;
        s_logOpen = s_log.Replace(s_fs, path, EFileWrite | EFileShareAny) == KErrNone;
        if (s_logOpen)
            s_log.Close();
    }
    plat_log("OpenClaw for S60 starting");
}

void SymbianPlatformShutdown()
{
    plat_audio_close();
    MusicShutdown();
    delete s_volumeKeys;
    s_volumeKeys = NULL;
    plat_log("OpenClaw exiting");
    FlushLog();
    s_logOpen = EFalse;
    CloseFs(s_fs);
}

void SymbianPlatformForeground(TBool aForeground)
{
    // in the background the game is paused: silence and the music on hold. The volume keys
    // are given back too: RemCon delivers them to one application only
    s_audioSilent = aForeground ? 0 : 1;
    if (aForeground && !s_volumeKeys)
    {
        TRAPD(volumeError, s_volumeKeys = CVolumeKeys::NewL());
        if (volumeError != KErrNone)
            s_volumeKeys = NULL;
    }
    else if (!aForeground)
    {
        delete s_volumeKeys;
        s_volumeKeys = NULL;
    }
    if (aForeground)
        plat_music_resume();
    else
        plat_music_pause();
    sdl2lite_on_focus(aForeground ? 1 : 0);
}

void SymbianPlatformResized(TInt aWidth, TInt aHeight)
{
    sdl2lite_on_resize(aWidth, aHeight);
}

//-----------------------------------------------------------------------------
// plat_* : video, time, log
//-----------------------------------------------------------------------------

extern "C" int plat_video_init(int, int, int* outW, int* outH)
{
    if (!s_container)
        return 0;
    plat_video_size(outW, outH);
    return 1;
}

extern "C" void plat_video_size(int* w, int* h)
{
    const TSize size = s_container ? s_container->ScreenSize() : TSize(320, 240);
    *w = size.iWidth;
    *h = size.iHeight;
}

extern "C" void plat_video_swap(void)
{
    if (s_container)
        s_container->SwapBuffers();
}

extern "C" void plat_video_shutdown(void)
{
}

extern "C" void plat_pump_events(void)
{
}

extern "C" Uint64 plat_ticks_us(void)
{
    if (s_counterFreq <= 0)
        return (Uint64)User::NTickCount() * 1000;
    const TUint32 now = User::FastCounter();
    s_ticks += s_counterUp ? (TUint32)(now - s_lastCounter) : (TUint32)(s_lastCounter - now);
    s_lastCounter = now;
    if (s_counterFreq == 32768)
        return (s_ticks * 15625) >> 9;
    return s_ticks * (Uint64)1000000 / (Uint64)s_counterFreq;
}

extern "C" void plat_delay_ms(Uint32 ms)
{
    User::AfterHighRes(ms * 1000);
}

// The log is written in batches and closed in between, so it can be copied off the phone
// while the game runs.
static char s_logBuffer[8192];
static TInt s_logUsed = 0;
static Uint64 s_logLastWrite = 0;


static void FlushLog()
{
    if (!s_logOpen || s_logUsed == 0)
        return;
    if (s_log.Open(s_fs, s_logPath, EFileWrite | EFileShareAny) == KErrNone)
    {
        TInt end = 0;
        s_log.Seek(ESeekEnd, end);
        s_log.Write(TPtrC8((const TUint8*)s_logBuffer, s_logUsed));
        s_log.Close();
    }
    s_logUsed = 0;
    s_logLastWrite = plat_ticks_us();
}

extern "C" void plat_log(const char* text)
{
    if (!s_logOpen)
        return;
    // time ms, heap KB, free RAM KB
    TInt ramFree = 0;
    HAL::Get(HALData::EMemoryRAMFree, ramFree);
    char stamp[48];
    const int n = sprintf(stamp, "%6u %5dK %5dK ", (unsigned)(plat_ticks_us() / 1000),
        (int)(User::Heap().Size() / 1024), (int)(ramFree / 1024));
    TInt len = (TInt)strlen(text);
    if (s_logUsed + n + len + 1 > (TInt)sizeof(s_logBuffer))
        FlushLog();
    if (n + len + 1 > (TInt)sizeof(s_logBuffer))
        len = sizeof(s_logBuffer) - n - 1;
    memcpy(s_logBuffer + s_logUsed, stamp, n);
    memcpy(s_logBuffer + s_logUsed + n, text, len);
    s_logBuffer[s_logUsed + n + len] = '\n';
    s_logUsed += n + len + 1;
    if (s_logEveryLine || plat_ticks_us() - s_logLastWrite > 1000000)
        FlushLog();
}

extern "C" const char* plat_data_dir(void)
{
    return s_dataDir;
}

//-----------------------------------------------------------------------------
// Audio: CMdaAudioOutputStream on its own thread, double buffered
//-----------------------------------------------------------------------------

static RThread s_audioThread;
static RMutex s_audioMutex;
static TBool s_audioRunning = EFalse;
static volatile TInt s_audioStop = 0;
static TInt s_audioRate = 22050;

enum { KAudioBufBytes = 4096 };
static TUint8 s_audioBuf[2][KAudioBufBytes];
static TInt s_audioBufIdx = 0;

class CAudioStream : public CBase, public MMdaAudioOutputStreamCallback
{
public:
    static CAudioStream* NewL(TInt aRate)
    {
        CAudioStream* self = new (ELeave) CAudioStream(aRate);
        CleanupStack::PushL(self);
        self->iStream = CMdaAudioOutputStream::NewL(*self);
        self->iStream->Open(&self->iSettings);
        CleanupStack::Pop(self);
        return self;
    }

    ~CAudioStream()
    {
        delete iStream;
    }

    void MaoscOpenComplete(TInt aError)
    {
        if (aError != KErrNone)
        {
            CActiveScheduler::Stop();
            return;
        }
        TInt rate = TMdaAudioDataSettings::ESampleRate22050Hz;
        if (iRate == 11025)
            rate = TMdaAudioDataSettings::ESampleRate11025Hz;
        else if (iRate == 16000)
            rate = TMdaAudioDataSettings::ESampleRate16000Hz;
        else if (iRate == 44100)
            rate = TMdaAudioDataSettings::ESampleRate44100Hz;
        TRAP_IGNORE(iStream->SetAudioPropertiesL(rate, TMdaAudioDataSettings::EChannelsStereo));
        ApplyVolume();
        iStream->SetPriority(EMdaPriorityMax, EMdaPriorityPreferenceTimeAndQuality);
        WriteNext();
        WriteNext();
    }

    void MaoscBufferCopied(TInt aError, const TDesC8&)
    {
        if (aError != KErrNone)
        {
            CActiveScheduler::Stop();
            return;
        }
        WriteNext();
    }

    void MaoscPlayComplete(TInt)
    {
    }

private:
    CAudioStream(TInt aRate) : iRate(aRate), iStream(NULL), iVolumeLevel(-1) {}

    void ApplyVolume()
    {
        iVolumeLevel = s_volumeLevel;
        iStream->SetVolume(iStream->MaxVolume() * iVolumeLevel / KVolumeSteps);
    }

    void WriteNext()
    {
        if (iVolumeLevel != s_volumeLevel)
            ApplyVolume();
        if (s_audioStop)
        {
            CActiveScheduler::Stop();
            return;
        }
        TUint8* buf = s_audioBuf[s_audioBufIdx];
        s_audioBufIdx ^= 1;
        if (s_audioSilent)
        {
            memset(buf, 0, KAudioBufBytes);
        }
        else
        {
            s_audioMutex.Wait();
            sdl2lite_mix_audio((Sint16*)buf, KAudioBufBytes / 4);
            s_audioMutex.Signal();
        }
        iDes.Set(buf, KAudioBufBytes);
        TRAP_IGNORE(iStream->WriteL(iDes));
    }

    TInt iRate;
    TInt iVolumeLevel;
    CMdaAudioOutputStream* iStream;
    TMdaAudioDataSettings iSettings;
    TPtrC8 iDes;
};

static TInt AudioThreadFunc(TAny*)
{
    CTrapCleanup* cleanup = CTrapCleanup::New();
    if (!cleanup)
        return KErrNoMemory;
    CActiveScheduler* scheduler = new CActiveScheduler;
    if (scheduler)
    {
        CActiveScheduler::Install(scheduler);
        CAudioStream* stream = NULL;
        TRAPD(err, stream = CAudioStream::NewL(s_audioRate));
        if (err == KErrNone)
            CActiveScheduler::Start();
        delete stream;
        CActiveScheduler::Install(NULL);
        delete scheduler;
    }
    delete cleanup;
    return KErrNone;
}

extern "C" int plat_audio_open(int rate)
{
    if (s_audioRunning)
        return 1;
    s_audioRate = rate;
    s_audioStop = 0;
    s_audioBufIdx = 0;
    if (s_audioMutex.CreateLocal() != KErrNone)
        return 0;
    _LIT(KAudioThreadName, "OpenClawAudio");
    if (s_audioThread.Create(KAudioThreadName, AudioThreadFunc, 0x8000, NULL, NULL) != KErrNone)
    {
        s_audioMutex.Close();
        return 0;
    }
    s_audioThread.SetPriority(EPriorityMore);
    s_audioThread.Resume();
    s_audioRunning = ETrue;
    return 1;
}

extern "C" void plat_audio_close(void)
{
    if (!s_audioRunning)
        return;
    s_audioStop = 1;
    // (the thread ends on its next buffer; if the stream is stuck, it is killed after half a second)
    TRequestStatus status;
    s_audioThread.Logon(status);
    RTimer timer;
    if (timer.CreateLocal() == KErrNone)
    {
        TRequestStatus timeout;
        timer.After(timeout, 500000);
        User::WaitForRequest(status, timeout);
        if (status == KRequestPending)
        {
            s_audioThread.Kill(KErrNone);
            User::WaitForRequest(status);
        }
        else
        {
            timer.Cancel();
            User::WaitForRequest(timeout);
        }
        timer.Close();
    }
    else
    {
        User::WaitForRequest(status);
    }
    s_audioThread.Close();
    s_audioMutex.Close();
    s_audioRunning = EFalse;
}

extern "C" void plat_audio_lock(void)
{
    if (s_audioRunning)
        s_audioMutex.Wait();
}

extern "C" void plat_audio_unlock(void)
{
    if (s_audioRunning)
        s_audioMutex.Signal();
}

//-----------------------------------------------------------------------------
// Music: the phone's MIDI synth
//-----------------------------------------------------------------------------

#ifndef OPENCLAW_NO_MIDI
class CMusicPlayer : public CBase, public MMidiClientUtilityObserver
{
public:
    CMusicPlayer() : iMidi(NULL), iData(NULL), iLoops(0), iVolume(128), iPaused(EFalse), iPlaying(EFalse) {}

    ~CMusicPlayer()
    {
        Stop();
        delete iMidi;
    }

    // One MIDI player for the whole game, closed and opened again for each song: a new player
    // per song played the songs after the first one silently
    void Play(const void* aData, int aSize, int aLoops)
    {
        Stop();
        iData = HBufC8::New(aSize);
        if (!iData)
            return;
        iData->Des().Copy((const TUint8*)aData, aSize);
        iLoops = aLoops;
        iPaused = EFalse;
        if (!iMidi)
        {
            TRAPD(err, iMidi = CMidiClientUtility::NewL(*this, EMdaPriorityNormal, EMdaPriorityPreferenceNone));
            if (err != KErrNone)
            {
                iMidi = NULL;
                plat_log("music: CMidiClientUtility::NewL failed");
                return;
            }
        }
        {
            char m[64];
            sprintf(m, "music: open %d bytes", aSize);
            plat_log(m);
        }
        iMidi->OpenDes(*iData);
    }

    void Stop()
    {
        if (iMidi)
        {
            iMidi->Close();
        }
        delete iData;
        iData = NULL;
        iPlaying = EFalse;
    }

    void Pause()
    {
        iPaused = ETrue;
        if (iMidi && iPlaying)
        {
            TRAP_IGNORE(iPosition = iMidi->PositionMicroSecondsL());
            iMidi->Stop(TTimeIntervalMicroSeconds(0));
        }
    }

    void Resume()
    {
        if (!iPaused)
            return;
        iPaused = EFalse;
        if (iMidi && iPlaying)
        {
            iMidi->Play();
            TRAP_IGNORE(iMidi->SetPositionMicroSecondsL(iPosition));
        }
    }

    void SetVolume(int aVolume)
    {
        iVolume = aVolume;
        ApplyVolume();
    }

    TBool IsPlaying() const
    {
        return iPlaying && !iPaused;
    }

    void Reapply()
    {
        ApplyVolume();
    }

    void MmcuoStateChanged(TMidiState aOldState, TMidiState aNewState, const TTimeIntervalMicroSeconds&, TInt aError)
    {
        {
            char m[80];
            sprintf(m, "music: state %d -> %d, error %d", (int)aOldState, (int)aNewState, (int)aError);
            plat_log(m);
        }
        if (aError != KErrNone)
        {
            char msg[64];
            sprintf(msg, "music: MIDI error %d", (int)aError);
            plat_log(msg);
            return;
        }
        if (aNewState == EMidiStateOpenDisengaged && !iPlaying)
        {
            iPlaying = ETrue;
            const TInt repeats = iLoops < 0 ? KMdaRepeatForever : iLoops;
            TRAP_IGNORE(iMidi->SetRepeatsL(repeats, TTimeIntervalMicroSeconds(0)));
            ApplyVolume();
            if (!iPaused)
                iMidi->Play();
        }
    }

    void MmcuoTempoChanged(TInt) {}
    void MmcuoVolumeChanged(TInt, TReal32) {}
    void MmcuoMuteChanged(TInt, TBool) {}
    void MmcuoSyncUpdate(const TTimeIntervalMicroSeconds&, TInt64) {}
    void MmcuoMetaDataEntryFound(const TInt, const TTimeIntervalMicroSeconds&) {}
    void MmcuoMipMessageReceived(const RArray<TMipMessageEntry>&) {}
    void MmcuoPolyphonyChanged(TInt) {}
    void MmcuoInstrumentChanged(TInt, TInt, TInt) {}

private:
    void ApplyVolume()
    {
        if (!iMidi || !iPlaying)
            return;
        TInt maxVolume = 0;
        TRAP_IGNORE(maxVolume = iMidi->MaxVolumeL());
        TRAP_IGNORE(iMidi->SetVolumeL(maxVolume * iVolume / 128 * s_volumeLevel / KVolumeSteps));
    }

    CMidiClientUtility* iMidi;
    HBufC8* iData;
    int iLoops;
    int iVolume;
    TBool iPaused;
    TBool iPlaying;
    TTimeIntervalMicroSeconds iPosition;
};

#else
class CMusicPlayer : public CBase
{
public:
    void Play(const void*, int, int) {}
    void Stop() {}
    void Pause() {}
    void Resume() {}
    void SetVolume(int) {}
    TBool IsPlaying() const { return EFalse; }
    void Reapply() {}
};
#endif

static CMusicPlayer* s_music = NULL;

static CMusicPlayer* Music()
{
    if (!s_music)
        s_music = new CMusicPlayer;
    return s_music;
}

static void MusicVolumeLevel(TInt)
{
    if (s_music)
        s_music->Reapply();
}

static void MusicShutdown()
{
    delete s_music;
    s_music = NULL;
}

extern "C" void plat_music_play(const void* midi, int size, int loops)
{
    if (Music())
        Music()->Play(midi, size, loops);
}

extern "C" void plat_music_stop(void)
{
    if (s_music)
        s_music->Stop();
}

extern "C" void plat_music_pause(void)
{
    if (Music())
        Music()->Pause();
}

extern "C" void plat_music_resume(void)
{
    if (Music())
        Music()->Resume();
}

extern "C" void plat_music_volume(int volume)
{
    if (Music())
        Music()->SetVolume(volume);
}

extern "C" int plat_music_playing(void)
{
    return s_music && s_music->IsPlaying() ? 1 : 0;
}
