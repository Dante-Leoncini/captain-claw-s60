#ifndef __MAINLOOP_H__
#define __MAINLOOP_H__

extern int RunGameEngine(int argc, char** argv);

// For hosts that drive the loop themselves (one StepGameEngine() per frame)
extern int InitGameEngine(int argc, char** argv);
extern bool StepGameEngine();
extern void ShutdownGameEngine();

extern bool TryToFindFile(const char *path);

#endif