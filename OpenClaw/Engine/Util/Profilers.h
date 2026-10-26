#ifndef __PROFILERS_H__
#define __PROFILERS_H__

#include <stdint.h>
#include <string>

class CPU_PROFILER
{
public:
    CPU_PROFILER(std::string tag = 0);
    ~CPU_PROFILER();

private:
    uint64_t m_StartingTime;
    std::string m_Tag;
};

class MEMORY_PROFILER
{
public:
    MEMORY_PROFILER(std::string tag = 0);
    ~MEMORY_PROFILER();

private:
    std::string m_Tag;
    int32_t m_StartingMemory;
};

// Time spent per part of the frame, averaged and logged every 2 seconds. Enabled with
// OPENCLAW_FRAME_PROFILE (used to tune slow devices).
enum FrameSection
{
    FrameSection_Events,
    FrameSection_Processes,
    FrameSection_PhysicsStep,
    FrameSection_PhysicsSync,
    FrameSection_Views,
    FrameSection_Actors,
    FrameSection_Render,
    FrameSection_Present,
    FrameSection_MovieDecode,
    FrameSection_MovieUpload,
    FrameSection_Count
};

class FrameProfiler
{
public:
    static void Add(FrameSection section, uint64_t microseconds);
    static void EndFrame();
};

class FRAME_SECTION_PROFILER
{
public:
    FRAME_SECTION_PROFILER(FrameSection section);
    ~FRAME_SECTION_PROFILER();

private:
    FrameSection m_Section;
    uint64_t m_StartingTime;
};

#ifdef OPENCLAW_FRAME_PROFILE
#define PROFILE_FRAME_SECTION(section) FRAME_SECTION_PROFILER _FRAME_SECTION_PROFILER_(section)
#define PROFILE_FRAME_END() FrameProfiler::EndFrame()
#else
#define PROFILE_FRAME_SECTION(section)
#define PROFILE_FRAME_END()
#endif

#endif