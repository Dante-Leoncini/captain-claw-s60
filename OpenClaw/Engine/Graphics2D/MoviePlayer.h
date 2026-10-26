#ifndef MOVIEPLAYER_H_
#define MOVIEPLAYER_H_

#include "../SharedDefines.h"
#include <SDL2/SDL.h>
#include <stdio.h>

struct smk_t;

// Plays the Smacker movies stored in the original CLAW.FEC archive:
// 0 = Monolith logo, 1 = introduction, 2..9 = story scenes between levels and ending, 10 = credits
class MoviePlayer
{
public:
    MoviePlayer();
    ~MoviePlayer();

    bool Open(const std::string& fecPath, int movieIndex, SDL_Renderer* pRenderer);
    void Close();

    bool IsPlaying() const { return m_pSmk != NULL; }

    // Advances the movie by the elapsed time; closes it after the last frame
    void Update(uint32 msDiff);
    void Render(SDL_Renderer* pRenderer, int windowWidth, int windowHeight);

private:
    bool DecodeNextFrame();
    void ConvertFrame();
    void QueueFrameAudio();

    static bool FindMovieOffset(FILE* pFile, int movieIndex, long& offset);

public:
    // (called from the mixer)
    void MixAudio(Uint8* pStream, int len);

private:

    FILE* m_pFile;
    struct smk_t* m_pSmk;
    SDL_Texture* m_pTexture;

    unsigned long m_Width;
    unsigned long m_Height;
    unsigned long m_FrameCount;
    unsigned long m_CurrentFrame;
    double m_FrameDurationUs;
    double m_ElapsedUs;
    std::vector<uint16_t> m_Pixels;

    // Movie sound waiting for the mixer, stereo 16 bit at the mixer rate
    bool m_HasAudio;
    int m_AudioChannels;
    std::vector<int16_t> m_AudioRing;
    size_t m_AudioRead;
    size_t m_AudioQueued;
};

#endif
