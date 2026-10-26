#include "MoviePlayer.h"
#include "../Util/Profilers.h"

#include <SDL2/SDL_mixer.h>
#include <libsmacker/smacker.h>

#include <string.h>

extern "C" void MoviePlayerMixAudio(void* pThis, Uint8* pStream, int len)
{
    static_cast<MoviePlayer*>(pThis)->MixAudio(pStream, len);
}

// One second of stereo sound at 22 kHz between the decoder and the mixer
static const size_t AUDIO_RING_SAMPLES = 2 * 22050 * 2;

MoviePlayer::MoviePlayer()
    :
    m_pFile(NULL),
    m_pSmk(NULL),
    m_pTexture(NULL),
    m_Width(0),
    m_Height(0),
    m_FrameCount(0),
    m_CurrentFrame(0),
    m_FrameDurationUs(66666.0),
    m_ElapsedUs(0.0),
    m_HasAudio(false),
    m_AudioChannels(0),
    m_AudioRead(0),
    m_AudioQueued(0)
{
}

MoviePlayer::~MoviePlayer()
{
    Close();
}

static uint32_t ReadU32(const unsigned char* p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

// CLAW.FEC is a container whose directory is encrypted; the movies inside are stored one after
// the other, so each one is found right after the end of the previous one (a few bytes of padding).
bool MoviePlayer::FindMovieOffset(FILE* pFile, int movieIndex, long& offset)
{
    const size_t windowSize = 4096;
    std::vector<unsigned char> window(windowSize);
    long searchFrom = 0;
    for (int index = 0; index <= movieIndex; index++)
    {
        if (fseek(pFile, searchFrom, SEEK_SET) != 0)
        {
            return false;
        }
        size_t got = fread(&window[0], 1, windowSize, pFile);
        long found = -1;
        for (size_t i = 0; i + 4 <= got; i++)
        {
            if (memcmp(&window[i], "SMK2", 4) == 0 || memcmp(&window[i], "SMK4", 4) == 0)
            {
                found = searchFrom + (long)i;
                break;
            }
        }
        if (found < 0)
        {
            return false;
        }
        if (index == movieIndex)
        {
            offset = found;
            return true;
        }

        // Header: frame count at 12, flags at 20 (bit 0: ring frame), trees size at 52, 104 bytes in all;
        // then one size and one type byte per frame, the trees and the frames
        unsigned char header[104];
        fseek(pFile, found, SEEK_SET);
        if (fread(header, 1, sizeof(header), pFile) != sizeof(header))
        {
            return false;
        }
        uint32_t frames = ReadU32(header + 12) + ((ReadU32(header + 20) & 1) ? 1 : 0);
        uint32_t treesSize = ReadU32(header + 52);
        long total = 104 + (long)frames * 5 + (long)treesSize;
        for (uint32_t frame = 0; frame < frames; frame++)
        {
            unsigned char size[4];
            if (fread(size, 1, 4, pFile) != 4)
            {
                return false;
            }
            total += (long)(ReadU32(size) & ~3u);
        }
        searchFrom = found + total;
    }
    return false;
}

bool MoviePlayer::Open(const std::string& fecPath, int movieIndex, SDL_Renderer* pRenderer)
{
    Close();

    m_pFile = fopen(fecPath.c_str(), "rb");
    if (!m_pFile)
    {
        LOG_WARNING("Movies not available: " + fecPath);
        return false;
    }

    long offset = 0;
    if (!FindMovieOffset(m_pFile, movieIndex, offset) || fseek(m_pFile, offset, SEEK_SET) != 0)
    {
        LOG_ERROR("Movie " + ToStr(movieIndex) + " not found in " + fecPath);
        Close();
        return false;
    }

    // (from here libsmacker owns the file and reads each frame from the disk)
    m_pSmk = smk_open_filepointer(m_pFile, SMK_MODE_DISK);
    m_pFile = NULL;
    if (!m_pSmk)
    {
        LOG_ERROR("Could not open movie " + ToStr(movieIndex));
        return false;
    }

    unsigned char yScale = 0;
    smk_info_all(m_pSmk, NULL, &m_FrameCount, &m_FrameDurationUs);
    smk_info_video(m_pSmk, &m_Width, &m_Height, &yScale);

    unsigned char trackMask = 0;
    unsigned char channels[7], bitDepth[7];
    unsigned long audioRate[7];
    smk_info_audio(m_pSmk, &trackMask, channels, bitDepth, audioRate);

    int mixerRate = 0, mixerChannels = 0;
    Uint16 mixerFormat = 0;
    m_HasAudio = (trackMask & SMK_AUDIO_TRACK_0) && bitDepth[0] == 16 && channels[0] >= 1 && channels[0] <= 2 &&
        Mix_QuerySpec(&mixerRate, &mixerFormat, &mixerChannels) && mixerRate == (int)audioRate[0];
    m_AudioChannels = channels[0];

    smk_enable_video(m_pSmk, 1);
    smk_enable_audio(m_pSmk, 0, m_HasAudio ? 1 : 0);

    m_pTexture = SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, (int)m_Width, (int)m_Height);
    m_Pixels.resize(m_Width * m_Height);

    if (m_HasAudio)
    {
        m_AudioRing.assign(AUDIO_RING_SAMPLES, 0);
        m_AudioRead = 0;
        m_AudioQueued = 0;
        Mix_HookMusic(&MoviePlayerMixAudio, this);
    }

    m_CurrentFrame = 0;
    m_ElapsedUs = 0.0;
    smk_first(m_pSmk);
    ConvertFrame();
    QueueFrameAudio();

    LOG("Playing movie " + ToStr(movieIndex) + ": " + ToStr((int)m_Width) + "x" + ToStr((int)m_Height) + ", " +
        ToStr((int)m_FrameCount) + " frames" + (m_HasAudio ? "" : ", no sound"));
    return true;
}

void MoviePlayer::Close()
{
    if (m_HasAudio)
    {
        Mix_HookMusic(NULL, NULL);
        m_HasAudio = false;
    }
    if (m_pSmk)
    {
        smk_close(m_pSmk);
        m_pSmk = NULL;
    }
    if (m_pFile)
    {
        fclose(m_pFile);
        m_pFile = NULL;
    }
    if (m_pTexture)
    {
        SDL_DestroyTexture(m_pTexture);
        m_pTexture = NULL;
    }
    m_Pixels.clear();
    m_AudioRing.clear();
}

void MoviePlayer::Update(uint32 msDiff)
{
    if (!m_pSmk)
    {
        return;
    }

    // A long hitch (loading, a call) would make the movie race to catch up: skip it instead
    m_ElapsedUs += msDiff < 500 ? msDiff * 1000.0 : m_FrameDurationUs;

    bool newFrame = false;
    while (m_ElapsedUs >= (m_CurrentFrame + 1) * m_FrameDurationUs)
    {
        if (!DecodeNextFrame())
        {
            Close();
            return;
        }
        newFrame = true;
    }
    if (newFrame)
    {
        ConvertFrame();
    }
}

bool MoviePlayer::DecodeNextFrame()
{
    if (m_CurrentFrame + 1 >= m_FrameCount)
    {
        return false;
    }
    {
        PROFILE_FRAME_SECTION(FrameSection_MovieDecode);
        if (smk_next(m_pSmk) == SMK_ERROR)
        {
            return false;
        }
    }
    m_CurrentFrame++;
    QueueFrameAudio();
    return true;
}

void MoviePlayer::ConvertFrame()
{
    const unsigned char* pPalette = smk_get_palette(m_pSmk);
    const unsigned char* pFrame = smk_get_video(m_pSmk);
    if (!pPalette || !pFrame || !m_pTexture)
    {
        return;
    }

    uint16_t palette565[256];
    for (int i = 0; i < 256; i++)
    {
        const unsigned char* c = pPalette + i * 3;
        palette565[i] = (uint16_t)(((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3));
    }
    PROFILE_FRAME_SECTION(FrameSection_MovieUpload);
    const size_t count = m_Width * m_Height;
    for (size_t i = 0; i < count; i++)
    {
        m_Pixels[i] = palette565[pFrame[i]];
    }
    SDL_UpdateTexture(m_pTexture, NULL, &m_Pixels[0], (int)m_Width * 2);
}

void MoviePlayer::QueueFrameAudio()
{
    if (!m_HasAudio)
    {
        return;
    }
    const int16_t* pSamples = (const int16_t*)smk_get_audio(m_pSmk, 0);
    unsigned long bytes = smk_get_audio_size(m_pSmk, 0);
    if (!pSamples || bytes == 0)
    {
        return;
    }

    const size_t frames = bytes / (2 * m_AudioChannels);
    const size_t ringSize = m_AudioRing.size();
    SDL_LockAudio();
    for (size_t i = 0; i < frames && m_AudioQueued + 2 <= ringSize; i++)
    {
        int16_t left = pSamples[i * m_AudioChannels];
        int16_t right = m_AudioChannels == 2 ? pSamples[i * 2 + 1] : left;
        size_t write = (m_AudioRead + m_AudioQueued) % ringSize;
        m_AudioRing[write] = left;
        m_AudioRing[(write + 1) % ringSize] = right;
        m_AudioQueued += 2;
    }
    SDL_UnlockAudio();
}

// Audio thread, with the audio lock held
void MoviePlayer::MixAudio(Uint8* pStream, int len)
{
    MoviePlayer* pPlayer = this;
    int16_t* pOut = (int16_t*)pStream;
    const size_t samples = (size_t)len / 2;
    const size_t ringSize = pPlayer->m_AudioRing.size();
    for (size_t i = 0; i < samples && pPlayer->m_AudioQueued > 0; i++)
    {
        pOut[i] = pPlayer->m_AudioRing[pPlayer->m_AudioRead];
        pPlayer->m_AudioRead = (pPlayer->m_AudioRead + 1) % ringSize;
        pPlayer->m_AudioQueued--;
    }
}

void MoviePlayer::Render(SDL_Renderer* pRenderer, int windowWidth, int windowHeight)
{
    if (!m_pTexture || m_Width == 0 || m_Height == 0)
    {
        return;
    }

    // Whole movie on screen, aspect kept. Movies twice as wide as high (the Monolith logo) are
    // stored with every other line, for a 4:3 screen
    const unsigned long displayHeight = m_Width >= m_Height * 2 ? m_Height * 2 : m_Height;
    float scaleX = (float)windowWidth / m_Width;
    float scaleY = (float)windowHeight / displayHeight;
    float scale = scaleX < scaleY ? scaleX : scaleY;
    SDL_Rect dst;
    dst.w = (int)(m_Width * scale);
    dst.h = (int)(displayHeight * scale);
    dst.x = (windowWidth - dst.w) / 2;
    dst.y = (windowHeight - dst.h) / 2;
    SDL_RenderCopy(pRenderer, m_pTexture, NULL, &dst);
}
