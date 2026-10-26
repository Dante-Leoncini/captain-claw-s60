#include "Profilers.h"
#include "../SharedDefines.h"

// For memory profiler
#if defined(_WIN32) && !defined(SDL2LITE)
#include <Windows.h>
#include <Psapi.h>
#endif

#include <string>
#include <stdint.h>
#include <iostream>
#include <SDL2/SDL.h>

CPU_PROFILER::CPU_PROFILER(std::string tag)
{
    m_Tag = tag;

    m_StartingTime = SDL_GetPerformanceCounter();
}

CPU_PROFILER::~CPU_PROFILER()
{
    uint64_t now = SDL_GetPerformanceCounter();

    if (!m_Tag.empty())
    {
        std::string s("[" + m_Tag + "]: Elapsed microseconds: " + ToStr((unsigned long)(((now - m_StartingTime) * 1000000) / SDL_GetPerformanceFrequency())));
        std::cout << s << std::endl;
    }
    else
    {
        std::string s("Elapsed microseconds: " + ToStr((unsigned long)(((now - m_StartingTime) * 1000000) / SDL_GetPerformanceFrequency())));
        std::cout << s << std::endl;
    }
}


MEMORY_PROFILER::MEMORY_PROFILER(std::string tag)
{
    m_Tag = tag;
#if defined(_WIN32) && !defined(SDL2LITE)
    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    m_StartingMemory = pmc.WorkingSetSize;
#else
    LOG_ERROR("Memory profiler not supported on this platform !");
#endif
}

MEMORY_PROFILER::~MEMORY_PROFILER()
{
#if defined(_WIN32) && !defined(SDL2LITE)
    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    SIZE_T currentMemory = pmc.WorkingSetSize;

    int32_t memoryDiff = currentMemory - m_StartingMemory;

    if (!m_Tag.empty())
    {
        std::string s("[" + m_Tag + "]: Memory difference: " + ToStr(memoryDiff));
        std::cout << s << std::endl;
    }
    else
    {
        std::string s("Memory difference: " + ToStr(memoryDiff));
        std::cout << s << std::endl;
    }
#endif
}
static uint64_t s_FrameSectionTime[FrameSection_Count];
static uint64_t s_ThisFrameSectionTime[FrameSection_Count];
static uint32_t s_ProfiledFrames = 0;
static uint64_t s_ProfilePeriodStart = 0;
static uint64_t s_LastFrameEnd = 0;

void FrameProfiler::Add(FrameSection section, uint64_t microseconds)
{
    s_FrameSectionTime[section] += microseconds;
    s_ThisFrameSectionTime[section] += microseconds;
}

void FrameProfiler::EndFrame()
{
    static const char* sectionNames[FrameSection_Count] =
    {
        "events", "processes", "physics", "sync", "views", "actors", "render", "present", "movie", "upload"
    };

    uint64_t now = (SDL_GetPerformanceCounter() * 1000000) / SDL_GetPerformanceFrequency();

    // A frame much longer than the others (a hitch) is logged on its own
    const uint64_t frameTime = s_LastFrameEnd ? now - s_LastFrameEnd : 0;
    s_LastFrameEnd = now;
    if (frameTime > 45000 && frameTime < 1000000)
    {
        uint64_t measured = 0;
        std::string line = "[slow] " + ToStr((int)(frameTime / 1000)) + " ms:";
        for (int i = 0; i < FrameSection_Count; i++)
        {
            if (s_ThisFrameSectionTime[i] >= 1000)
            {
                char value[32];
                sprintf(value, " %s %.1f", sectionNames[i], s_ThisFrameSectionTime[i] / 1000.0);
                line += value;
            }
            measured += s_ThisFrameSectionTime[i];
        }
        char rest[32];
        sprintf(rest, " (outside the sections %.1f)", (frameTime - (measured < frameTime ? measured : frameTime)) / 1000.0);
        LOG(line + rest);
    }
    for (int i = 0; i < FrameSection_Count; i++)
    {
        s_ThisFrameSectionTime[i] = 0;
    }

    if (s_ProfilePeriodStart == 0)
    {
        s_ProfilePeriodStart = now;
    }
    s_ProfiledFrames++;
    if (now - s_ProfilePeriodStart < 2000000)
    {
        return;
    }

    std::string line = "[frame] " + ToStr((int)s_ProfiledFrames) + " frames, ms per frame:";
    for (int i = 0; i < FrameSection_Count; i++)
    {
        char value[32];
        sprintf(value, " %s %.1f", sectionNames[i], s_FrameSectionTime[i] / 1000.0 / s_ProfiledFrames);
        line += value;
        s_FrameSectionTime[i] = 0;
    }
    LOG(line);
    s_ProfiledFrames = 0;
    s_ProfilePeriodStart = now;
}

FRAME_SECTION_PROFILER::FRAME_SECTION_PROFILER(FrameSection section)
    :
    m_Section(section),
    m_StartingTime(SDL_GetPerformanceCounter())
{
}

FRAME_SECTION_PROFILER::~FRAME_SECTION_PROFILER()
{
    FrameProfiler::Add(m_Section,
        ((SDL_GetPerformanceCounter() - m_StartingTime) * 1000000) / SDL_GetPerformanceFrequency());
}
