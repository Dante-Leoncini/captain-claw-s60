#include "OpenClawApp.h"
#include "OpenClawSymbian.h"

#include <eikstart.h>
#include <eikenv.h>
#include <eikspane.h>
#include <eikbtgpc.h>
#include <avkon.hrh>
#include <akndef.h>
#include <e32base.h>
#include <e32keys.h>
#include <GLES/gl.h>

extern "C" void* OpenClawGameApp();

//-----------------------------------------------------------------------------
// Application / document
//-----------------------------------------------------------------------------

TUid COpenClawApplication::AppDllUid() const
{
    return KUidOpenClaw;
}

CApaDocument* COpenClawApplication::CreateDocumentL()
{
    return new (ELeave) COpenClawDocument(*this);
}

LOCAL_C CApaApplication* NewApplication()
{
    return new COpenClawApplication;
}

GLDEF_C TInt E32Main()
{
    return EikStart::RunApplication(NewApplication);
}

CEikAppUi* COpenClawDocument::CreateAppUiL()
{
    return new (ELeave) COpenClawAppUi;
}

//-----------------------------------------------------------------------------
// AppUi
//-----------------------------------------------------------------------------

void COpenClawAppUi::ConstructL()
{
    // Landscape (320x240) or portrait (240x320), following the phone
    BaseConstructL(EAknEnableSkin | EAppOrientationAutomatic);
    if (StatusPane())
        StatusPane()->MakeVisible(EFalse);
    if (Cba())
        Cba()->MakeVisible(EFalse);
    iContainer = new (ELeave) COpenClawContainer;
    iContainer->SetMopParent(this);
    iContainer->ConstructL();
    AddToStackL(iContainer);
}

COpenClawAppUi::~COpenClawAppUi()
{
    if (iContainer)
    {
        RemoveFromStack(iContainer);
        delete iContainer;
    }
}

void COpenClawAppUi::HandleCommandL(TInt aCommand)
{
    switch (aCommand)
    {
        case EEikCmdExit:
        case EAknSoftkeyExit:
        case EAknCmdExit:
            if (iContainer)
                iContainer->Quit();
            Exit();
            break;
        default:
            break;
    }
}

void COpenClawAppUi::HandleForegroundEventL(TBool aForeground)
{
    CAknAppUi::HandleForegroundEventL(aForeground);
    if (iContainer)
        iContainer->SetForeground(aForeground);
}

//-----------------------------------------------------------------------------
// Container
//-----------------------------------------------------------------------------

void COpenClawContainer::ConstructL()
{
    CreateWindowL();
    SetExtentToWholeScreen();
    ActivateL();
    InitEglL();
    SymbianPlatformInit(this);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    SwapBuffers();

    // The game starts on the first idle tick, once the application is up.
    iLoop = CIdle::NewL(CActive::EPriorityIdle);
    iLoop->Start(TCallBack(COpenClawContainer::FrameCallback, this));
}

void COpenClawContainer::InitEglL()
{
    iEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (iEglDisplay == EGL_NO_DISPLAY)
        User::Panic(_L("eglGetDisplay"), 0);
    if (eglInitialize(iEglDisplay, NULL, NULL) == EGL_FALSE)
        User::Panic(_L("eglInitialize"), 0);

    TInt bits = 16;
    switch (Window().DisplayMode())
    {
        case EColor4K: bits = 12; break;
        case EColor64K: bits = 16; break;
        case EColor16M: bits = 24; break;
        case EColor16MU:
        case EColor16MA: bits = 32; break;
        default: break;
    }
    const EGLint attribs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BUFFER_SIZE, bits, EGL_DEPTH_SIZE, 0, EGL_NONE };
    EGLConfig config;
    EGLint count = 0;
    if (eglChooseConfig(iEglDisplay, attribs, &config, 1, &count) == EGL_FALSE || count == 0)
        User::Panic(_L("eglChooseConfig"), 0);
    iEglSurface = eglCreateWindowSurface(iEglDisplay, config, &Window(), NULL);
    if (iEglSurface == EGL_NO_SURFACE)
        User::Panic(_L("eglCreateWindowSurface"), 0);
    iEglContext = eglCreateContext(iEglDisplay, config, EGL_NO_CONTEXT, NULL);
    if (iEglContext == EGL_NO_CONTEXT)
        User::Panic(_L("eglCreateContext"), 0);
    if (eglMakeCurrent(iEglDisplay, iEglSurface, iEglSurface, iEglContext) == EGL_FALSE)
        User::Panic(_L("eglMakeCurrent"), 0);
    // (no waiting for the display refresh: the game rarely makes 60 fps here)
    eglSwapInterval(iEglDisplay, 0);
}

COpenClawContainer::~COpenClawContainer()
{
    Quit();
}

// Leaves at once: audio stopped, log written, and the process ends without tearing the game
// down object by object (that takes long on the phone and saves nothing: the game saves its
// progress at each checkpoint).
void COpenClawContainer::Quit()
{
    if (iLoop)
        iLoop->Cancel();
    SymbianPlatformShutdown();
    User::Exit(KErrNone);
}

void COpenClawContainer::SwapBuffers()
{
    eglSwapBuffers(iEglDisplay, iEglSurface);
}

TInt COpenClawContainer::FrameCallback(TAny* aContainer)
{
    static_cast<COpenClawContainer*>(aContainer)->Frame();
    return 1;
}

void COpenClawContainer::Frame()
{
    TBool running = ETrue;
    if (!iGameStarted)
    {
        iGameStarted = ETrue;
        static char arg0[] = "OpenClaw";
        static char* argv[] = { arg0, NULL };
        running = OpenClawGameApp() && InitGameEngine(1, argv) == 0;
    }
    else
    {
        SymbianPlatformFrame();
        running = StepGameEngine();
    }

    if (++iFrames % 100 == 0)
        User::ResetInactivityTime();

    if (!running)
    {
        Quit();
    }
}

void COpenClawContainer::SetForeground(TBool aForeground)
{
    if (!iLoop || aForeground == !iBackground)
        return;
    iBackground = !aForeground;
    SymbianPlatformForeground(aForeground);
    if (aForeground)
    {
        if (!iLoop->IsActive())
            iLoop->Start(TCallBack(COpenClawContainer::FrameCallback, this));
    }
    else
    {
        iLoop->Cancel();
    }
}

TKeyResponse COpenClawContainer::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
{
    if (aType == EEventKeyDown)
        return SymbianPlatformKey(aKeyEvent.iScanCode, ETrue) ? EKeyWasConsumed : EKeyWasNotConsumed;
    if (aType == EEventKeyUp)
        return SymbianPlatformKey(aKeyEvent.iScanCode, EFalse) ? EKeyWasConsumed : EKeyWasNotConsumed;
    return SymbianPlatformIsGameKey(aKeyEvent.iScanCode) ? EKeyWasConsumed : EKeyWasNotConsumed;
}

void COpenClawContainer::SizeChanged()
{
    if (iGameStarted)
        SymbianPlatformResized(Size().iWidth, Size().iHeight);
}

void COpenClawContainer::HandleResourceChange(TInt aType)
{
    CCoeControl::HandleResourceChange(aType);
    if (aType == KEikDynamicLayoutVariantSwitch)
        SetExtentToWholeScreen();
}

void COpenClawContainer::Draw(const TRect&) const
{
}
