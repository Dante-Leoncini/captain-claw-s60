// S60 3rd Edition application around the game: application, document, AppUi and the
// full-screen container that owns the OpenGL ES window and drives one game frame per CIdle tick.
#ifndef OPENCLAWAPP_H_
#define OPENCLAWAPP_H_

#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <coecntrl.h>
#include <GLES/egl.h>

const TUid KUidOpenClaw = { 0xE0C1A75A };

class COpenClawApplication : public CAknApplication
{
private:
    CApaDocument* CreateDocumentL();
    TUid AppDllUid() const;
};

class COpenClawDocument : public CAknDocument
{
public:
    COpenClawDocument(CEikApplication& aApp) : CAknDocument(aApp) {}

private:
    CEikAppUi* CreateAppUiL();
};

class CIdle;

class COpenClawContainer : public CCoeControl
{
public:
    void ConstructL();
    ~COpenClawContainer();

    TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
    void SetForeground(TBool aForeground);

    void SwapBuffers();
    void Quit();
    TSize ScreenSize() const { return Size(); }

private:
    void SizeChanged();
    void HandleResourceChange(TInt aType);
    void Draw(const TRect& aRect) const;
    void InitEglL();
    static TInt FrameCallback(TAny* aContainer);
    void Frame();

    EGLDisplay iEglDisplay;
    EGLSurface iEglSurface;
    EGLContext iEglContext;
    CIdle* iLoop;
    TBool iGameStarted;
    TBool iBackground;
    TInt iFrames;
};

class COpenClawAppUi : public CAknAppUi
{
public:
    void ConstructL();
    ~COpenClawAppUi();
    void HandleCommandL(TInt aCommand);

private:
    void HandleForegroundEventL(TBool aForeground);

    COpenClawContainer* iContainer;
};

#endif
