// Glue between the S60 container, the sdl2lite platform layer and the game loop.
#ifndef OPENCLAWSYMBIAN_H_
#define OPENCLAWSYMBIAN_H_

#include <e32std.h>

class COpenClawContainer;

void SymbianPlatformInit(COpenClawContainer* aContainer);
void SymbianPlatformShutdown();
void SymbianPlatformForeground(TBool aForeground);
void SymbianPlatformResized(TInt aWidth, TInt aHeight);
// once per frame (held volume keys)
void SymbianPlatformFrame();
// iScanCode of a key press/release; false when it is not a game key
TBool SymbianPlatformKey(TInt aScanCode, TBool aDown);
TBool SymbianPlatformIsGameKey(TInt aScanCode);

// The game loop (OpenClaw/Engine/GameApp/MainLoop.cpp), run one frame at a time.
int InitGameEngine(int argc, char** argv);
bool StepGameEngine();
void ShutdownGameEngine();

#endif
