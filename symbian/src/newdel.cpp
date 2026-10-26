// Global new/delete on the thread heap. The Symbian^3 SDK otherwise imports them from
// scppnwdl.dll, which S60 3rd Edition phones (N95) don't have. Returns NULL on failure.
#ifndef __WINS__

#include <e32std.h>
#include <stdio.h>
#include "../sdl2lite/src/sdl2lite_platform.h"

static void* Alloc(unsigned int aSize)
{
    void* p = User::Alloc(aSize);
    if (!p)
    {
        TInt biggest = 0;
        const TInt avail = User::Available(biggest);
        char msg[96];
        sprintf(msg, "out of memory: %u bytes (heap free %d, biggest block %d)", aSize, (int)avail, (int)biggest);
        plat_log(msg);
    }
    return p;
}

void* operator new(unsigned int aSize)
{
    return Alloc(aSize);
}

void* operator new[](unsigned int aSize)
{
    return Alloc(aSize);
}

void operator delete(void* aPtr)
{
    User::Free(aPtr);
}

void operator delete[](void* aPtr)
{
    User::Free(aPtr);
}

#endif
