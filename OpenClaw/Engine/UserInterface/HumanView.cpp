#include "HumanView.h"
#include "../GameApp/BaseGameApp.h"
#include "../GameApp/BaseGameLogic.h"
#include "../Events/EventMgr.h"
#include "../Events/Events.h"
#include "../Audio/Audio.h"
#include "../Resource/Loaders/MidiLoader.h"
#include "../Resource/Loaders/WavLoader.h"
#include "../Resource/Loaders/XmlLoader.h"
#include "../Util/PrimeSearch.h"
#include "../Actor/Components/PositionComponent.h"
#include "../Actor/Components/RenderComponent.h"
#include "ScoreScreen/EndLevelScoreScreen.h"

const uint32 g_InvalidGameViewId = 0xFFFFFFFF;

HumanView::HumanView(SDL_Renderer* renderer)
    :
    m_bRendering(true),
    m_bPostponeRenderPresent(false)
{
    m_pProcessMgr = new ProcessMgr();

    m_ViewId = INVALID_GAME_VIEW_ID;

    RegisterAllDelegates();

    if (renderer)
    {
        m_pScene.reset(new ScreenElementScene(renderer));
        m_pCamera.reset(new CameraNode(Point(0, 0), 0, 0));
        m_pHUD.reset(new ScreenElementHUD());

        m_pScene->AddChild(INVALID_ACTOR_ID, m_pCamera);
        m_pScene->SetCamera(m_pCamera);

        //m_pConsole = unique_ptr<Console>(new Console(g_pApp->GetWindowSize().x, g_pApp->GetWindowSize().y / 2,
            //g_pApp->GetConsoleFont(), renderer, "console02.tga"));

        m_pConsole.reset(new Console(g_pApp->GetConsoleConfig(), renderer, g_pApp->GetWindow()));
    }
}

HumanView::~HumanView()
{
    RemoveAllDelegates();

    while (!m_ScreenElements.empty())
    {
        m_ScreenElements.pop_front();
    }

    SAFE_DELETE(m_pProcessMgr);
}

void HumanView::RegisterConsoleCommandHandler(void(*handler)(const char*, void*), void* userdata)
{
    if (m_pConsole)
    {
        m_pConsole->SetCommandHandler(handler, (void*)m_pConsole.get());
    }
}

void HumanView::VOnRender(uint32 msDiff)
{
    //PROFILE_CPU("HumanView Render");

    m_CurrentTick = SDL_GetTicks();
    if (m_CurrentTick == m_LastDraw)
    {
        return;
    }

    if (!m_bRendering)
    {
        return;
    }

    SDL_Renderer* renderer = g_pApp->GetRenderer();

    if (m_RunFullSpeed || true)
    {
        //PROFILE_CPU(".");

        // If we render out of bounds, render it as black color
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

        SDL_RenderClear(renderer);

        // Sort screen elements
        m_ScreenElements.sort(SortBy_SharedPtr_Content<IScreenElement>());

        for (ScreenElementList::iterator it = m_ScreenElements.begin(); it != m_ScreenElements.end(); ++it)
        {
            shared_ptr<IScreenElement>& screenElement = *it;
            if (screenElement->VIsVisible())
            {
                screenElement->VOnRender(msDiff);
            }
        }
        //LOG("SCREEN ELEMENTS: " + ToStr(m_ScreenElements.size()))

        g_pApp->GetGameLogic()->VRenderDiagnostics(renderer, m_pCamera);

        m_pConsole->OnRender(renderer);

        if (!m_bPostponeRenderPresent)
        {
            PROFILE_FRAME_SECTION(FrameSection_Present);
            SDL_RenderPresent(renderer);
        }
    }
}

void HumanView::VOnUpdate(uint32 msDiff)
{
    m_pProcessMgr->UpdateProcesses(msDiff);

    m_pConsole->OnUpdate(msDiff);

    for (ScreenElementList::iterator it = m_ScreenElements.begin(); it != m_ScreenElements.end(); ++it)
    {
        shared_ptr<IScreenElement> &element = *it;
        element->VOnUpdate(msDiff);
    }
}

bool HumanView::VOnEvent(SDL_Event& evt)
{
    // First let console try to eat this event
    if (m_pConsole->OnEvent(evt))
    {
        //LOG("command eaten !");
        return true;
    }

    // Then screen layers in reverse order
    for (ScreenElementList::reverse_iterator i = m_ScreenElements.rbegin(); i != m_ScreenElements.rend(); ++i)
    {
        if ((*i)->VIsVisible())
        {
            if ((*i)->VOnEvent(evt))
            {
                return true;
            }
        }
    }

    

    switch (evt.type)
    {
        case SDL_KEYDOWN:
        {
            if (evt.key.repeat == 0)
            {
                if (SDL_GetScancodeFromKey(evt.key.keysym.sym) == SDL_SCANCODE_ESCAPE &&
                    g_pApp->GetGameLogic()->GetGameState() == GameState_IngameRunning &&
                    m_pIngameMenu &&
                    !m_pIngameMenu->VIsVisible())
                {
                    m_pIngameMenu->VSetVisible(true);
                    return true;
                }

                return m_pKeyboardHandler->VOnKeyDown(evt.key.keysym.sym);
            }
            break;
        }
        case SDL_KEYUP:
        {
            if (evt.key.repeat == 0)
            {
                return m_pKeyboardHandler->VOnKeyUp(evt.key.keysym.sym);
            }
            break;
        }

        case SDL_UserTouchEvent:
        {
            if (m_pTouchHandler) {
                const Touch_Event &touchEvent = *((Touch_Event *)evt.user.data1);
                return m_pTouchHandler->VOnTouch(touchEvent);
            }
            break;
        }

        case SDL_MOUSEMOTION:
        {
            return m_pPointerHandler->VOnPointerMove(evt.motion);
        }
        case SDL_MOUSEBUTTONDOWN:
        {
            return m_pPointerHandler->VOnPointerButtonDown(evt.button);
        }
        case SDL_MOUSEBUTTONUP:
        {
            return m_pPointerHandler->VOnPointerButtonUp(evt.button);
        }
        default:
            return false;
    }

    return false;
}

void HumanView::VOnLostDevice()
{

}

bool HumanView::EnterMenu(TiXmlElement* pMenuData)
{
    if (m_pMenu == nullptr)
    {
        m_pMenu.reset(new ScreenElementMenu(g_pApp->GetRenderer()));
        if (!m_pMenu->Initialize(pMenuData))
        {
            LOG_ERROR("Could not initialize ScreenElementMenu");
            return false;
        }
    }

    m_ScreenElements.clear();
    VPushElement(m_pMenu);

    return true;
}

void HumanView::LoadScoreScreen(TiXmlElement* pScoreScreenRootElem)
{
    m_ScreenElements.clear();
    m_pScene.reset();
    m_pHUD.reset();

    m_pCamera->SetParent(nullptr);
    Point defaultCameraPos(0, 0);
    m_pCamera->VSetPosition(defaultCameraPos);
    m_pCamera->SetTarget(nullptr);

    g_pApp->GetAudio()->StopAllSounds();

    LOG("Initializing score screen ...");
    shared_ptr<ScreenElementScoreScreen> pScoreScreen(new ScreenElementScoreScreen(g_pApp->GetRenderer()));
    if (pScoreScreen->Initialize(pScoreScreenRootElem))
    {
        LOG("Score screen initialized OK");
        pScoreScreen->SetCamera(m_pCamera);
        VPushElement(pScoreScreen);
        assert(m_ScreenElements.size() == 1);
    }
    else
    {
        LOG_ERROR("Failed to inizialize Score Screen !");
    }
}

bool HumanView::LoadGame(TiXmlElement* pLevelXmlElem, LevelData* pLevelData)
{
    m_pScene->SortSceneNodesByZCoord();

    // Start playing background music
    m_CurrentLevelMusic = "/LEVEL" + ToStr(pLevelData->GetLevelNumber()) +
        "/MUSIC/PLAY.XMI";

    SoundInfo soundInfo(m_CurrentLevelMusic);
    soundInfo.isMusic = true;
    soundInfo.loops = -1;
    soundInfo.soundVolume = g_pApp->GetGameConfig()->musicVolume;

    IEventMgr::Get()->VQueueEvent(IEventDataPtr(new EventData_Request_Play_Sound(soundInfo)));

    return VLoadGameDelegate(pLevelXmlElem, pLevelData);
}

void HumanView::VPushElement(shared_ptr<IScreenElement> element)
{
    m_ScreenElements.push_front(element);
}

void HumanView::VRemoveElement(shared_ptr<IScreenElement> element)
{
    m_ScreenElements.remove(element);
}

void HumanView::VSetCameraOffset(int32 offsetX, int32 offsetY)
{
    if (m_pCamera)
    {
        m_pCamera->SetCameraOffset(offsetX, offsetY);
    }
    else
    {
        LOG_WARNING("Trying to set offset to nonexistant camera");
    }
}

void HumanView::RegisterAllDelegates()
{
    IEventMgr* pEventMgr = IEventMgr::Get();
    pEventMgr->VAddListener(MakeDelegate(this, &HumanView::NewHUDElementDelegate), EventData_New_HUD_Element::sk_EventType);
    pEventMgr->VAddListener(MakeDelegate(this, &HumanView::ScoreUpdatedDelegate), EventData_Updated_Score::sk_EventType);
    pEventMgr->VAddListener(MakeDelegate(this, &HumanView::LivesUpdatedDelegate), EventData_Updated_Lives::sk_EventType);
    pEventMgr->VAddListener(MakeDelegate(this, &HumanView::HealthUpdatedDelegate), EventData_Updated_Health::sk_EventType);
    pEventMgr->VAddListener(MakeDelegate(this, &HumanView::AmmoUpdatedDelegate), EventData_Updated_Ammo::sk_EventType);
    pEventMgr->VAddListener(MakeDelegate(this, &HumanView::AmmoTypeUpdatedDelegate), EventData_Updated_Ammo_Type::sk_EventType);
    pEventMgr->VAddListener(MakeDelegate(this, &HumanView::PowerupUpdatedTimeDelegate), EventData_Updated_Powerup_Time::sk_EventType);
    pEventMgr->VAddListener(MakeDelegate(this, &HumanView::PowerupUpdatedStatusDelegate), EventData_Updated_Powerup_Status::sk_EventType);
    pEventMgr->VAddListener(MakeDelegate(this, &HumanView::RequestPlaySoundDelegate), EventData_Request_Play_Sound::sk_EventType);
    pEventMgr->VAddListener(MakeDelegate(this, &HumanView::RequestResetLevelDelegate), EventData_Request_Reset_Level::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(
        this, &HumanView::LoadGameDelegate), EventData_Menu_LoadGame::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(
        this, &HumanView::SetVolumeDelegate), EventData_Set_Volume::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(
        this, &HumanView::SoundEnabledChangedDelegate), EventData_Sound_Enabled_Changed::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(
        this, &HumanView::ClawDiedDelegate), EventData_Claw_Died::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(
        this, &HumanView::TeleportActorDelegate), EventData_Teleport_Actor::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(
        this, &HumanView::EnterMenuDelegate), EventData_Enter_Menu::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(
        this, &HumanView::FinishedLevelDelegate), EventData_Finished_Level::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(
        this, &HumanView::ActorEnteredBossAreaDelegate), EventData_Entered_Boss_Area::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(
        this, &HumanView::BossFightEndedDelegate), EventData_Boss_Fight_Ended::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(
        this, &HumanView::IngameMenuEndGameDelegate), EventData_IngameMenu_End_Game::sk_EventType);
    
}

void HumanView::RemoveAllDelegates()
{
    IEventMgr* pEventMgr = IEventMgr::Get();
    pEventMgr->VRemoveListener(MakeDelegate(this, &HumanView::NewHUDElementDelegate), EventData_New_HUD_Element::sk_EventType);
    pEventMgr->VRemoveListener(MakeDelegate(this, &HumanView::ScoreUpdatedDelegate), EventData_Updated_Score::sk_EventType);
    pEventMgr->VRemoveListener(MakeDelegate(this, &HumanView::LivesUpdatedDelegate), EventData_Updated_Lives::sk_EventType);
    pEventMgr->VRemoveListener(MakeDelegate(this, &HumanView::HealthUpdatedDelegate), EventData_Updated_Health::sk_EventType);
    pEventMgr->VRemoveListener(MakeDelegate(this, &HumanView::AmmoUpdatedDelegate), EventData_Updated_Ammo::sk_EventType);
    pEventMgr->VRemoveListener(MakeDelegate(this, &HumanView::AmmoTypeUpdatedDelegate), EventData_Updated_Ammo_Type::sk_EventType);
    pEventMgr->VRemoveListener(MakeDelegate(this, &HumanView::PowerupUpdatedTimeDelegate), EventData_Updated_Powerup_Time::sk_EventType);
    pEventMgr->VRemoveListener(MakeDelegate(this, &HumanView::PowerupUpdatedStatusDelegate), EventData_Updated_Powerup_Status::sk_EventType);
    pEventMgr->VRemoveListener(MakeDelegate(this, &HumanView::RequestPlaySoundDelegate), EventData_Request_Play_Sound::sk_EventType);
    pEventMgr->VRemoveListener(MakeDelegate(this, &HumanView::RequestResetLevelDelegate), EventData_Request_Reset_Level::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(
        this, &HumanView::LoadGameDelegate), EventData_Menu_LoadGame::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(
        this, &HumanView::SetVolumeDelegate), EventData_Set_Volume::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(
        this, &HumanView::SoundEnabledChangedDelegate), EventData_Sound_Enabled_Changed::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(
        this, &HumanView::ClawDiedDelegate), EventData_Claw_Died::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(
        this, &HumanView::TeleportActorDelegate), EventData_Teleport_Actor::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(
        this, &HumanView::EnterMenuDelegate), EventData_Enter_Menu::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(
        this, &HumanView::FinishedLevelDelegate), EventData_Finished_Level::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(
        this, &HumanView::ActorEnteredBossAreaDelegate), EventData_Entered_Boss_Area::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(
        this, &HumanView::BossFightEndedDelegate), EventData_Boss_Fight_Ended::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(
        this, &HumanView::BossFightEndedDelegate), EventData_Boss_Fight_Ended::sk_EventType);
}

//=====================================================================================================================
// Delegates
//=====================================================================================================================

void HumanView::NewHUDElementDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_New_HUD_Element> pCastEventData = static_pointer_cast<EventData_New_HUD_Element>(pEventData);
    if (m_pHUD)
    {
        m_pHUD->AddHUDElement(pCastEventData->GetHUDKey(), pCastEventData->GetHUDElement());
    }
    else
    {
        LOG_ERROR("HUD is unitialized");
    }
}

void HumanView::ScoreUpdatedDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Updated_Score> pCastEventData = static_pointer_cast<EventData_Updated_Score>(pEventData);
    if (m_pHUD)
    {
        m_pHUD->UpdateScore(pCastEventData->GetNewScore());
        if (!pCastEventData->IsInitialScore())
        {
            if ((pCastEventData->GetOldScore() / 1000000) != (pCastEventData->GetNewScore() / 1000000))
            {
                shared_ptr<EventData_New_Life> pEvent(new EventData_New_Life(pCastEventData->GetActorId(), 1));
                IEventMgr::Get()->VQueueEvent(pEvent);

                // Play new life sound
                SoundInfo soundInfo(SOUND_GAME_EXTRA_LIFE);
                IEventMgr::Get()->VTriggerEvent(IEventDataPtr(
                    new EventData_Request_Play_Sound(soundInfo)));
            }
        }
    }
    else
    {
        LOG_ERROR("HUD is unitialized");
    }
}

void HumanView::LivesUpdatedDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Updated_Lives> pCastEventData = static_pointer_cast<EventData_Updated_Lives>(pEventData);
    if (m_pHUD)
    {
        m_pHUD->UpdateLives(pCastEventData->GetNewLivesCount());
    }
    else
    {
        LOG_ERROR("HUD is unitialized");
    }
}

void HumanView::HealthUpdatedDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Updated_Health> pCastEventData = static_pointer_cast<EventData_Updated_Health>(pEventData);
    if (m_pHUD)
    {
        m_pHUD->UpdateHealth(max(0, pCastEventData->GetNewHealth()));
    }
    else
    {
        LOG_ERROR("HUD is unitialized");
    }
}

void HumanView::AmmoUpdatedDelegate(IEventDataPtr pEventData)
{
    if (m_pHUD)
    {
        shared_ptr<EventData_Updated_Ammo> pCastEventData = static_pointer_cast<EventData_Updated_Ammo>(pEventData);

        // TODO: This should be more generic
        if ((pCastEventData->GetAmmoType() == AmmoType_Pistol && m_pHUD->IsElementVisible("pistol")) ||
            (pCastEventData->GetAmmoType() == AmmoType_Magic && m_pHUD->IsElementVisible("magic")) ||
            (pCastEventData->GetAmmoType() == AmmoType_Dynamite && m_pHUD->IsElementVisible("dynamite")))
        { 
            m_pHUD->UpdateAmmo(pCastEventData->GetAmmoCount());
        }
        else if (pCastEventData->GetAmmoType() <= AmmoType_None || pCastEventData->GetAmmoType() >= AmmoType_Max)
        {
            LOG_ERROR("Unknown ammo type: " + ToStr(pCastEventData->GetAmmoType()));
        }
    }
    else
    {
        LOG_ERROR("HUD is unitialized");
    }
}

void HumanView::AmmoTypeUpdatedDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Updated_Ammo_Type> pCastEventData = static_pointer_cast<EventData_Updated_Ammo_Type>(pEventData);
    if (pCastEventData->GetAmmoType() >= AmmoType_Max)
    {
        return;
    }

    if (m_pHUD)
    {
        // TODO: Redo this to be more general
        m_pHUD->SetElementVisible("pistol", false);
        m_pHUD->SetElementVisible("magic", false);
        m_pHUD->SetElementVisible("dynamite", false);

        if (pCastEventData->GetAmmoType() == AmmoType_Pistol) { m_pHUD->SetElementVisible("pistol", true); }
        else if (pCastEventData->GetAmmoType() == AmmoType_Magic) { m_pHUD->SetElementVisible("magic", true); }
        else if (pCastEventData->GetAmmoType() == AmmoType_Dynamite) { m_pHUD->SetElementVisible("dynamite", true); }
        else
        {
            LOG_ERROR("Unknown ammo type: " + ToStr(pCastEventData->GetAmmoType()));
        }
    }
    else
    {
        LOG_ERROR("HUD is unitialized");
    }
}

void HumanView::PowerupUpdatedTimeDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Updated_Powerup_Time> pCastEventData = static_pointer_cast<EventData_Updated_Powerup_Time>(pEventData);
    if (m_pHUD)
    {
        m_pHUD->UpdateStopwatchTime(pCastEventData->GetSecondsRemaining());
    }
    else
    {
        LOG_ERROR("HUD is unitialized");
    }
}


void HumanView::PowerupUpdatedStatusDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Updated_Powerup_Status> pCastEventData = static_pointer_cast<EventData_Updated_Powerup_Status>(pEventData);
    if (m_pHUD)
    {
        bool hadPowerup = m_pHUD->IsElementVisible("stopwatch");

        if (!m_pHUD->SetElementVisible("stopwatch", !pCastEventData->IsPowerupFinished()))
        {
            LOG_ERROR("Could not set visibility to HUD element \"stopwatch\"");
        }

        if (!hadPowerup && m_pHUD->IsElementVisible("stopwatch"))
        {
            // Acquired powerup
            SoundInfo soundInfo("/GAME/MUSIC/POWERUP.XMI");
            soundInfo.isMusic = true;
            soundInfo.loops = -1;
            IEventMgr::Get()->VTriggerEvent(IEventDataPtr(
                new EventData_Request_Play_Sound(soundInfo)));

            if (pCastEventData->GetPowerupType() == PowerupType_Invisibility)
            {

            }
        }
        else if (hadPowerup && !m_pHUD->IsElementVisible("stopwatch"))
        {
            // Lost powerup
            assert(!m_CurrentLevelMusic.empty());
            SoundInfo soundInfo(m_CurrentLevelMusic);
            soundInfo.isMusic = true;
            soundInfo.loops = -1;
            IEventMgr::Get()->VTriggerEvent(IEventDataPtr(
                new EventData_Request_Play_Sound(soundInfo)));
        }
    }
    else
    {
        LOG_ERROR("HUD is unitialized");
    }
}

#include <cmath>
// TODO: Handle somehow volume of specific track
// Mix_VolumeChunk for sound
// Music has only 1 channel as far as I know so setting volume for music globally should be fine
void HumanView::RequestPlaySoundDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Request_Play_Sound> pCastEventData = static_pointer_cast<EventData_Request_Play_Sound>(pEventData);
    if (pCastEventData)
    {
        const SoundInfo* pSoundInfo = pCastEventData->GetSoundInfo();

        if ((pSoundInfo->soundToPlay.empty()) || (pSoundInfo->soundToPlay == "/GAME/SOUNDS/NULL.WAV"))
        {
            return;
        }

        if (pSoundInfo->isMusic) // Background music - instrumental
        {
#ifdef __EMSCRIPTEN__
            // TODO: [EMSCRIPTEN] Disable midi sounds for now.
            // All midi must be converted to MP3 or another web browser compatible formats
            return;
#endif
            shared_ptr<MidiFile> pMidiFile = MidiResourceLoader::LoadAndReturnMidiFile(pSoundInfo->soundToPlay.c_str());
            assert(pMidiFile != nullptr);

            g_pApp->GetAudio()->PlayMusic(pMidiFile->data, pMidiFile->size, pSoundInfo->loops != 0);
        }
        else // Effect / Speech etc. - WAV
        {
            SoundProperties soundProperties;
            soundProperties.volume = pSoundInfo->soundVolume;
            soundProperties.loops = pSoundInfo->loops;

            Point soundSourcePos = pSoundInfo->soundSourcePosition;
            if (pSoundInfo->setPositionEffect)
            {
                assert(!soundSourcePos.IsZeroXY());
            }

            if (pSoundInfo->setDistanceEffect)
            {
                /*assert(!soundSourcePos.IsZeroXY());
                Point soundDistanceDelta = m_pCamera->GetCenterPosition() - soundSourcePos;
                float length = soundDistanceDelta.Length();
                float distanceRatio = length / (m_pCamera->GetWidth() / 2);
                int sdlDistance = std::min(distanceRatio * 255, (float)255);
                LOG("SDL DISTANCE: " + ToStr(sdlDistance));*/
            }

            bool play = true;
            if (!soundSourcePos.IsZeroXY())
            {
                const float paddingPx = 150.0f;
                const float paddingRatio = paddingPx / (float)m_pCamera->GetWidth();
                if (m_pCamera->IntersectsWithPoint(soundSourcePos, 1.0f + paddingRatio))
                {
                    if (pSoundInfo->setDistanceEffect)
                    {
                        Point soundDistanceDelta = m_pCamera->GetCenterPosition() - soundSourcePos;
                        double length = soundDistanceDelta.Length();

                        float distanceRatio = length / ((m_pCamera->GetWidth() / 2) * (1.0f + paddingRatio));
                        //float distanceRatio = length / pSoundInfo->maxHearDistance;
                        int sdlDistance = std::min(distanceRatio * 150, (float)150);
                        /*LOG("SDL DISTANCE: " + ToStr(sdlDistance));
                        LOG("Length: " + ToStr(length));*/
                        soundProperties.distance = sdlDistance;

                        if (pSoundInfo->setPositionEffect)
                        {
                            double dot = soundDistanceDelta.y;
                            double det = soundDistanceDelta.x;
                            double angle = std::atan2(det, dot);
                            angle *= 180 / M_PI;
                            angle -= 180;

                            if (angle < 0) angle = fabs(angle) + 180;

                            soundProperties.angle = angle;
                        }
                    }

                    /*LOG("CenterPosition: " + m_pCamera->GetCenterPosition().ToString());
                    LOG("SoundSourcePos: " + soundSourcePos.ToString());*/
                } else {
                    play = false;
                }
            }

            if (play)
            {
                shared_ptr<Mix_Chunk> pSound = WavResourceLoader::LoadAndReturnSound(pSoundInfo->soundToPlay.c_str());
                assert(pSound != nullptr);
                g_pApp->GetAudio()->PlaySound(pSound.get(), soundProperties);
            }
        }
    }
}

void HumanView::RequestResetLevelDelegate(IEventDataPtr pEventData)
{
    // This event actually has nothing but the information that someone requested level reset
    shared_ptr<EventData_Request_Reset_Level> pCastEventData = 
        static_pointer_cast<EventData_Request_Reset_Level>(pEventData);

    // Reset Graphical representation of level
    m_ScreenElements.clear();

    m_pScene.reset(new ScreenElementScene(g_pApp->GetRenderer()));
    //m_pCamera.reset(new CameraNode(Point(0, 0), 0, 0));
    m_pHUD.reset(new ScreenElementHUD());
    m_pScene->AddChild(INVALID_ACTOR_ID, m_pCamera);
    m_pScene->SetCamera(m_pCamera);
    //m_pCamera->SetSize(g_pApp->GetWindowSize().x, g_pApp->GetWindowSize().y);

    g_pApp->GetGameLogic()->VResetLevel();
}

void HumanView::LoadGameDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Menu_LoadGame> pCastEventData =
        static_pointer_cast<EventData_Menu_LoadGame>(pEventData);
    
    if (pCastEventData)
    {
        bool isNewGame = pCastEventData->GetIsNewGame();
        int levelNumber = pCastEventData->GetLevelNumber();
        int checkpointNumber = pCastEventData->GetCheckpointNumber();

        // The menu goes to black first
        Util::FadeScreen(&HumanView::DrawFrameWithoutPresent, this, 0, 255, 400);

        // Reset Graphical representation of level
        m_ScreenElements.clear();
        m_pMenu.reset();
        m_pScene.reset(new ScreenElementScene(g_pApp->GetRenderer()));
        m_pHUD.reset(new ScreenElementHUD());
        m_pScene->AddChild(INVALID_ACTOR_ID, m_pCamera);
        m_pScene->SetCamera(m_pCamera);

        // Go to menu after finishing last implemented level
        if (levelNumber > g_pApp->GetDebugOptions()->lastImplementedLevel)
        {
            IEventMgr::Get()->VQueueEvent(IEventDataPtr(new EventData_Enter_Menu()));
        }
        else
        {
            shared_ptr<LevelData> pLevelData(new LevelData(levelNumber, isNewGame, checkpointNumber));
            g_pApp->GetGameLogic()->SetLevelData(pLevelData);

            g_pApp->GetGameLogic()->VChangeState(GameState_LoadingLevel);
        }
    }
}

void HumanView::EnterMenuDelegate(IEventDataPtr pEventData)
{
    if (g_pApp->GetGameLogic()->GetGameState() == GameState_Menu)
    {
        LOG_WARNING("Already in menu state. Skipping.");
        return;
    }

    m_ScreenElements.clear();
    m_pScene.reset(new ScreenElementScene(g_pApp->GetRenderer()));
    m_pHUD.reset();
    m_pIngameMenu.reset();
    m_pCamera->SetParent(nullptr);

    g_pApp->GetAudio()->StopAllSounds();
    //g_pApp->GetGameLogic()->UnloadLevel();

    g_pApp->GetGameLogic()->VChangeState(GameState_LoadingMenu);
}

void HumanView::FinishedLevelDelegate(IEventDataPtr pEventData)
{
    
}

void HumanView::ActorEnteredBossAreaDelegate(IEventDataPtr pEventData)
{
    m_pHUD->SetElementVisible("bossbar", true);

    assert(g_pApp->GetGameLogic()->GetCurrentLevelData() != nullptr);
    int currentLevel = g_pApp->GetGameLogic()->GetCurrentLevelData()->GetLevelNumber();

    std::string bossMusicPath = "LEVEL" + ToStr(currentLevel) + "/MUSIC/BOSS.XMI";

    SoundInfo soundInfo(bossMusicPath);
    soundInfo.isMusic = true;
    soundInfo.loops = -1;
    //soundInfo.soundVolume = g_pApp->GetGameConfig()->musicVolume * 3;
    g_pApp->GetAudio()->SetMusicVolume(g_pApp->GetGameConfig()->musicVolume);
    IEventMgr::Get()->VTriggerEvent(IEventDataPtr(new EventData_Request_Play_Sound(soundInfo)));
}

void HumanView::BossFightEndedDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Boss_Fight_Ended> pCastEventData =
        static_pointer_cast<EventData_Boss_Fight_Ended>(pEventData);

    if (!pCastEventData->GetIsBossDead())
    {
        SoundInfo soundInfo(m_CurrentLevelMusic);
        soundInfo.isMusic = true;
        soundInfo.loops = -1;
        //soundInfo.soundVolume = g_pApp->GetGameConfig()->musicVolume;
        g_pApp->GetAudio()->SetMusicVolume(g_pApp->GetGameConfig()->musicVolume);
        IEventMgr::Get()->VTriggerEvent(IEventDataPtr(new EventData_Request_Play_Sound(soundInfo)));
    }

    m_pHUD->SetElementVisible("bossbar", false);

    // Does music go back to original level one once the boss is defeated ?
    /*SoundInfo soundInfo(m_CurrentLevelMusic);
    soundInfo.isMusic = true;
    soundInfo.loops = -1;
    //soundInfo.soundVolume = g_pApp->GetGameConfig()->musicVolume;
    g_pApp->GetAudio()->SetMusicVolume(g_pApp->GetGameConfig()->musicVolume / 3);
    IEventMgr::Get()->VTriggerEvent(IEventDataPtr(new EventData_Request_Play_Sound(soundInfo)));*/
}

void HumanView::SetVolumeDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Set_Volume> pCastEventData =
        static_pointer_cast<EventData_Set_Volume>(pEventData);

    if (pCastEventData)
    {
        if (pCastEventData->GetIsMusicVolume())
        {
            if (pCastEventData->GetIsDelta())
            {
                int currentVolume = g_pApp->GetAudio()->GetMusicVolume();
                g_pApp->GetAudio()->SetMusicVolume(currentVolume + pCastEventData->GetVolume());
            }
            else
            {
                g_pApp->GetAudio()->SetMusicVolume(pCastEventData->GetVolume());
            }
        }
        else
        {
            if (pCastEventData->GetIsDelta())
            {
                int currentVolume = g_pApp->GetAudio()->GetSoundVolume();
                g_pApp->GetAudio()->SetSoundVolume(currentVolume + pCastEventData->GetVolume());
            }
            else
            {
                g_pApp->GetAudio()->SetSoundVolume(pCastEventData->GetVolume());
            }
        }
    }
}

void HumanView::SoundEnabledChangedDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Sound_Enabled_Changed> pCastEventData =
        static_pointer_cast<EventData_Sound_Enabled_Changed>(pEventData);

    if (pCastEventData)
    {
        if (pCastEventData->GetIsMusic())
        {
            g_pApp->GetAudio()->SetMusicActive(pCastEventData->GetIsEnabled());
        }
        else
        {
            g_pApp->GetAudio()->SetSoundActive(pCastEventData->GetIsEnabled());
        }
    }
}

void HumanView::ClawDiedDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Claw_Died> pCastEventData =
        static_pointer_cast<EventData_Claw_Died>(pEventData);

    if (pCastEventData->GetRemainingLives() < 0)
    {
        TiXmlElement* pXmlGameOverMenuRoot = XmlResourceLoader::LoadAndReturnRootXmlElement("GAME_OVER_MENU.XML");
        assert(pXmlGameOverMenuRoot != NULL);

        shared_ptr<ScreenElementMenu> pGameOverMenu(new ScreenElementMenu(g_pApp->GetRenderer()));
        DO_AND_CHECK(pGameOverMenu->Initialize(pXmlGameOverMenuRoot));

        m_ScreenElements.push_back(pGameOverMenu);
        pGameOverMenu->VSetVisible(true);

        IEventMgr::Get()->VAbortAllEvents();
    }
    else
    {
        StrongProcessPtr pDeathProcess(
            new DeathFadeInOutProcess(pCastEventData->GetDeathPosition(), 1450, 1450, 0, 0));
        m_pProcessMgr->AttachProcess(pDeathProcess);
        // Needs to be called here
        pDeathProcess->VOnInit();
    }
}

void HumanView::TeleportActorDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Teleport_Actor> pCastEventData =
        static_pointer_cast<EventData_Teleport_Actor>(pEventData);

    if (pCastEventData->GetHasScreenSfx())
    {
        StrongProcessPtr pTeleportSfxProcess(
            new TeleportFadeInOutProcess(1150, 1150));
        m_pProcessMgr->AttachProcess(pTeleportSfxProcess);
        // Needs to be called here
        pTeleportSfxProcess->VOnInit();
    }
}

void HumanView::IngameMenuEndGameDelegate(IEventDataPtr pEventData)
{
    IEventMgr::Get()->VQueueEvent(IEventDataPtr(new EventData_Enter_Menu()));
}

//=================================================================================================
// 
// class SpecialEffectProcess
//
//=================================================================================================

void SpecialEffectProcess::VRestoreStates()
{
    if (g_pApp && g_pApp->GetGameLogic())
    {
        g_pApp->GetGameLogic()->SetRunning(true);
    }
    if (g_pApp && g_pApp->GetHumanView())
    {
        g_pApp->GetHumanView()->SetPostponeRenderPresent(false);
        g_pApp->GetHumanView()->SetRendering(true);
    }
}

//=================================================================================================
// 
// class DeathFadeInOutProcess
//
//=================================================================================================

DeathFadeInOutProcess::DeathFadeInOutProcess(Point epicenter, int fadeInDuration, int fadeOutDuration, int startDelay, int endDelay)
    :
    SpecialEffectProcess(),
    m_Epicenter(epicenter),
    m_FadeInDuration(fadeInDuration),
    m_FadeOutDuration(fadeOutDuration),
    m_StartDelay(startDelay),
    m_EndDelay(endDelay),
    m_DeathFadeState(DeathFadeState_Started),
    m_CurrentTime(0)
{
    m_Epicenter.Set(epicenter.x, epicenter.y);
}

DeathFadeInOutProcess::~DeathFadeInOutProcess()
{

}

void DeathFadeInOutProcess::VOnInit()
{
    Process::VOnInit();

    assert(g_pApp);
    assert(g_pApp->GetGameLogic());
    assert(g_pApp->GetHumanView());
    assert(g_pApp->GetHumanView()->GetCamera());

    // Recalc epicenter to screen-local coordinates
    Point windowSize = g_pApp->GetWindowSizeScaled();
    shared_ptr<CameraNode> pCamera = g_pApp->GetHumanView()->GetCamera();
    Point cameraPos = pCamera->GetPosition();

    // (camera and drawing both in game pixels: no scale to apply)
    m_Epicenter.Set(m_Epicenter.x - cameraPos.x, m_Epicenter.y - cameraPos.y);

    // Stop game logic update during fade in/out
    g_pApp->GetGameLogic()->SetRunning(false);

    // Also we will render as we want to
    g_pApp->GetHumanView()->SetRendering(false);
    g_pApp->GetHumanView()->SetPostponeRenderPresent(true);

    // While closing, the place where he died stays on the screen, without him (he is already back at
    // the checkpoint): the camera stops following him until the circle opens again
    m_pCameraTarget = pCamera->GetTarget();
    pCamera->ClearTarget();
    SetClawVisible(false);

    // Calculate graphics fade speed
    m_FadeInSpeed.x = (windowSize.x / (double)m_FadeInDuration) / 2.0;
    m_FadeInSpeed.y = (windowSize.y / (double)m_FadeInDuration) / 2.0;

    m_FadeOutSpeed.x = (windowSize.x / (double)m_FadeOutDuration) / 2.0;
    m_FadeOutSpeed.y = (windowSize.y / (double)m_FadeOutDuration) / 2.0;
}

void DeathFadeInOutProcess::VOnUpdate(uint32 msDiff)
{
    m_CurrentTime += msDiff;

    switch (m_DeathFadeState)
    {
        case DeathFadeState_Started:
        {
            if (m_CurrentTime >= m_StartDelay)
            {
                SoundInfo soundInfo(SOUND_GAME_DEATH_FADE_IN_SOUND);
                IEventMgr::Get()->VTriggerEvent(IEventDataPtr(
                    new EventData_Request_Play_Sound(soundInfo)));

                m_CurrentTime = 0;
                m_DeathFadeState = DeathFadeState_FadingIn;
            }
            break;
        }

        case DeathFadeState_FadingIn:
        {
            if (m_CurrentTime >= m_FadeInDuration)
            {
                SoundInfo soundInfo(SOUND_GAME_DEATH_FADE_OUT_SOUND);
                IEventMgr::Get()->VTriggerEvent(IEventDataPtr(
                    new EventData_Request_Play_Sound(soundInfo)));

                if (m_pCameraTarget)
                {
                    g_pApp->GetHumanView()->GetCamera()->SetTarget(m_pCameraTarget);
                    m_pCameraTarget.reset();
                }
                SetClawVisible(true);
                m_CurrentTime = 0;
                m_DeathFadeState = DeathFadeState_FadingOut;
            }
            break;
        }

        case DeathFadeState_FadingOut:
        {
            if (m_CurrentTime >= m_FadeOutDuration)
            {
                m_CurrentTime = 0;
                m_DeathFadeState = DeathFadeState_Ended;
            }
            break;
        }

        case DeathFadeState_Ended:
        {
            if (m_CurrentTime >= m_EndDelay)
            {
                Succeed();

                // This should not be here, since it does not know that Claw died here...
                StrongActorPtr pClaw = g_pApp->GetGameLogic()->GetClawActor();
                assert(pClaw != nullptr);

                IEventMgr::Get()->VQueueEvent(IEventDataPtr(new EventData_Claw_Respawned(pClaw->GetGUID())));
            }
            break;
        }
        default:
            LOG_ERROR("Unknown DeathFadeState: " + ToStr((int)m_DeathFadeState));
            break;
    }

    //LOG("State: " + ToStr((int)m_DeathFadeState));

    VRender(msDiff);
}

void HumanView::DrawFrameWithoutPresent(void* pHumanView)
{
    HumanView* pThis = static_cast<HumanView*>(pHumanView);
    const bool postponed = pThis->m_bPostponeRenderPresent;
    pThis->m_bPostponeRenderPresent = true;
    pThis->m_LastDraw = 0;
    pThis->VOnRender(0);
    pThis->m_bPostponeRenderPresent = postponed;
}

void HumanView::PlayLevelStartReveal()
{
    m_pProcessMgr->AttachProcess(StrongProcessPtr(new LevelRevealProcess(900)));
}

//=================================================================================================
//
// class LevelRevealProcess - the level unrolls out of black from left to right, a still picture,
// before the game starts (the original game)
//
//=================================================================================================

LevelRevealProcess::LevelRevealProcess(int duration)
    :
    SpecialEffectProcess(),
    m_Duration(duration),
    m_CurrentTime(0)
{
}

void LevelRevealProcess::VOnInit()
{
    Process::VOnInit();
    g_pApp->GetGameLogic()->SetRunning(false);
    g_pApp->GetHumanView()->SetRendering(false);
    g_pApp->GetHumanView()->SetPostponeRenderPresent(true);
}

void LevelRevealProcess::VOnUpdate(uint32 msDiff)
{
    m_CurrentTime += msDiff;
    VRender(msDiff);
    if (m_CurrentTime >= m_Duration)
    {
        Succeed();
    }
}

void LevelRevealProcess::VRender(uint32 msDiff)
{
    SDL_Renderer* pRenderer = g_pApp->GetRenderer();
    HumanView* pHumanView = g_pApp->GetHumanView();
    pHumanView->SetRendering(true);
    pHumanView->VOnRender(msDiff);
    pHumanView->SetRendering(false);

    const double revealed = m_CurrentTime >= m_Duration ? 1.0 : (double)m_CurrentTime / m_Duration;
    Point windowSize = g_pApp->GetWindowSize();
    float scaleX, scaleY;
    SDL_RenderGetScale(pRenderer, &scaleX, &scaleY);
    SDL_RenderSetScale(pRenderer, 1.0f, 1.0f);
    const int edge = (int)(windowSize.x * revealed);
    SDL_Rect hidden = { edge, 0, (int)windowSize.x - edge, (int)windowSize.y };
    SDL_SetRenderDrawColor(pRenderer, 0, 0, 0, 255);
    if (hidden.w > 0)
    {
        SDL_RenderFillRect(pRenderer, &hidden);
    }
    SDL_RenderSetScale(pRenderer, scaleX, scaleY);

    Util::RenderForcePresent(pRenderer);
}

void DeathFadeInOutProcess::SetClawVisible(bool visible)
{
    if (StrongActorPtr pClaw = g_pApp->GetGameLogic()->GetClawActor())
    {
        if (shared_ptr<ActorRenderComponent> pRenderComponent = MakeStrongPtr(pClaw->GetComponent<ActorRenderComponent>()))
        {
            pRenderComponent->SetVisible(visible);
        }
    }
}

// Claw on the screen, in the coordinates the effects draw with
Point DeathFadeInOutProcess::GetClawScreenPosition()
{
    Point position = m_Epicenter;
    StrongActorPtr pClaw = g_pApp->GetGameLogic()->GetClawActor();
    shared_ptr<CameraNode> pCamera = g_pApp->GetHumanView()->GetCamera();
    if (pClaw && pCamera)
    {
        Point clawPosition = pClaw->GetPositionComponent()->GetPosition();
        position.Set(clawPosition.x - pCamera->GetPosition().x, clawPosition.y - pCamera->GetPosition().y);
    }
    return position;
}

// Black square with a round transparent hole, stretched over the iris
static SDL_Texture* GetIrisTexture(SDL_Renderer* pRenderer)
{
    static SDL_Texture* s_pIrisTexture = NULL;
    if (s_pIrisTexture == NULL)
    {
        const int size = 256;
        SDL_Surface* pSurface = Util::CreateRGBSurface(0, size, size, 32);
        Uint32* pPixels = (Uint32*)pSurface->pixels;
        const double radius = size / 2.0;
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                const double dx = x + 0.5 - radius, dy = y + 0.5 - radius;
                const double distance = sqrt(dx * dx + dy * dy);
                // (a soft pixel at the edge)
                double alpha = (distance - (radius - 1.5)) / 1.5;
                alpha = alpha < 0.0 ? 0.0 : (alpha > 1.0 ? 1.0 : alpha);
                pPixels[y * (pSurface->pitch / 4) + x] = SDL_MapRGBA(pSurface->format, 0, 0, 0, (Uint8)(alpha * 255));
            }
        }
        s_pIrisTexture = SDL_CreateTextureFromSurface(pRenderer, pSurface);
        SDL_FreeSurface(pSurface);
    }
    return s_pIrisTexture;
}

void DeathFadeInOutProcess::VRender(uint32 msDiff)
{
    SDL_Renderer* pRenderer = g_pApp->GetRenderer();
    Point windowSize = g_pApp->GetWindowSizeScaled();

    // The scene as it stands (the game is stopped), with the iris of the original game over it:
    // a circle closing on where Claw died and opening again where he comes back
    HumanView* pHumanView = g_pApp->GetHumanView();
    SetClawVisible(m_DeathFadeState == DeathFadeState_FadingOut || m_DeathFadeState == DeathFadeState_Ended);
    pHumanView->SetRendering(true);
    pHumanView->VOnRender(msDiff);
    pHumanView->SetRendering(false);

    // Opening on Claw where he comes back (the camera has caught up with him once the scene is drawn)
    if (m_DeathFadeState == DeathFadeState_FadingOut)
    {
        m_Epicenter = GetClawScreenPosition();
    }

    double openRatio = 1.0;
    if (m_DeathFadeState == DeathFadeState_FadingIn)
    {
        openRatio = 1.0 - (double)m_CurrentTime / m_FadeInDuration;
    }
    else if (m_DeathFadeState == DeathFadeState_FadingOut)
    {
        openRatio = (double)m_CurrentTime / m_FadeOutDuration;
    }
    openRatio = openRatio < 0.0 ? 0.0 : (openRatio > 1.0 ? 1.0 : openRatio);

    // (on the middle of the screen when Claw is off it: he fell)
    const bool onScreen = m_Epicenter.x >= 0 && m_Epicenter.x <= windowSize.x && m_Epicenter.y >= 0 && m_Epicenter.y <= windowSize.y;
    const double centerX = onScreen ? m_Epicenter.x : windowSize.x / 2;
    const double centerY = onScreen ? m_Epicenter.y : windowSize.y / 2;
    const double farX = (std::max)(centerX, windowSize.x - centerX);
    const double farY = (std::max)(centerY, windowSize.y - centerY);
    const int radius = (int)(sqrt(farX * farX + farY * farY) * openRatio);

    SDL_Rect hole = { (int)centerX - radius, (int)centerY - radius, radius * 2, radius * 2 };
    SDL_SetRenderDrawColor(pRenderer, 0, 0, 0, 255);
    SDL_Rect above = { 0, 0, (int)windowSize.x, hole.y };
    SDL_Rect below = { 0, hole.y + hole.h, (int)windowSize.x, (int)windowSize.y - (hole.y + hole.h) };
    SDL_Rect left = { 0, hole.y, hole.x, hole.h };
    SDL_Rect right = { hole.x + hole.w, hole.y, (int)windowSize.x - (hole.x + hole.w), hole.h };
    SDL_Rect* pBlackRects[] = { &above, &below, &left, &right };
    for (int i = 0; i < 4; i++)
    {
        if (pBlackRects[i]->w > 0 && pBlackRects[i]->h > 0)
        {
            SDL_RenderFillRect(pRenderer, pBlackRects[i]);
        }
    }
    if (radius > 0)
    {
        SDL_RenderCopy(pRenderer, GetIrisTexture(pRenderer), NULL, &hole);
    }

    Util::RenderForcePresent(pRenderer);
}

//=================================================================================================
// 
// class FadingLine - helper that represents line on the screen that is fading in or out
//
//=================================================================================================

FadingLine::FadingLine(int length, Point fragmentSize, int fadeDelay, int fadeDuration, bool isFadingIn)
    :
    m_Length(length),
    m_FragmentSize(fragmentSize),
    m_FadeDelay(fadeDelay),
    m_FadeDuration(fadeDuration),
    m_bIsFadingIn(isFadingIn),
    m_CurrentTime(0),
    m_SingleFragmentFadeTime(0),
    m_bIsActive(false),
    m_bIsDone(false),
    m_FragmentCount(0)
{
    m_FragmentCount = (length / (int)fragmentSize.x) + 1;
    m_SingleFragmentFadeTime = fadeDuration / m_FragmentCount;
    m_pPrimeSearch.reset(new PrimeSearch(m_FragmentCount));

    for (int fragIdx = 0; fragIdx < m_FragmentCount; fragIdx++)
    {
        m_FadedFragments.push_back(!m_bIsFadingIn);
    }

    LOG("length: " + ToStr(m_Length) + ", fadeDelay: " + ToStr(m_FadeDelay) + 
        ", fadeDuration: " + ToStr(m_FadeDuration) + ", fragmentCount: " + ToStr(m_FragmentCount) + 
        ", isFadingIn: " + ToStr(m_bIsFadingIn));
}

FadingLine::~FadingLine()
{

}

void FadingLine::Update(uint32 msDiff)
{
    if (m_bIsDone)
    {
        return;
    }

    m_CurrentTime += msDiff;
    if (!m_bIsActive)
    {
        if (m_CurrentTime >= m_FadeDelay)
        {
            m_bIsActive = true;
            //m_CurrentTime = m_CurrentTime - m_FadeDelay;
            LOG("Activated");
            m_CurrentTime = 0;
        }
        else
        {
            return;
        }
    }

    if (m_CurrentTime >= m_SingleFragmentFadeTime)
    {
        // TODO: Handle possible overflow
        //m_CurrentTime -= m_SingleFragmentFadeTime;
        m_CurrentTime = 0;
        int fadedElemIdx = m_pPrimeSearch->GetNext();
        if (fadedElemIdx == -1)
        {
            m_bIsDone = true;
            return;
        }

        m_FadedFragments[fadedElemIdx] = m_bIsFadingIn;
    }
}

void FadingLine::Reset(int fadeDelay, bool isFadingIn)
{
    m_bIsActive = false;
    m_bIsDone = false;
    m_bIsFadingIn = isFadingIn;
    m_FadeDelay = fadeDelay;

    m_FadedFragments.clear();
    for (int fragIdx = 0; fragIdx < m_FragmentCount; fragIdx++)
    {
        m_FadedFragments.push_back(!m_bIsFadingIn);
    }

    m_pPrimeSearch.reset(new PrimeSearch(m_FragmentCount));
}

void FadingLine::Render(SDL_Renderer* pRenderer, SDL_Texture* pFragmentTexture, Point& lineOffset, bool asRow)
{
    assert(pRenderer != NULL);
    assert(pFragmentTexture != NULL);

    int fragIdx = 0;
    for (size_t i = 0; i < m_FadedFragments.size(); i++)
    {
        bool shouldRender = m_FadedFragments[i];
        if (!shouldRender)
        {
            fragIdx++;
            //LOG("Should NOT render");
            continue;
        }
        else
        {
            //LOG("Should render")
        }

        SDL_Rect renderRect;
        renderRect.w = (int)m_FragmentSize.x;
        renderRect.y = (int)m_FragmentSize.y;
        if (asRow)
        {
            renderRect.x = (int)lineOffset.x + fragIdx * (int)m_FragmentSize.x;
            renderRect.y = (int)lineOffset.y;
        }
        else
        {
            renderRect.x = (int)lineOffset.x;
            renderRect.y = (int)lineOffset.y + fragIdx * (int)m_FragmentSize.y;
        }

        SDL_RenderCopy(pRenderer, pFragmentTexture, NULL, &renderRect);

        fragIdx++;
    }
}

//=================================================================================================
// 
// class TeleportFadeInOutProcess
//
//=================================================================================================

TeleportFadeInOutProcess::TeleportFadeInOutProcess(int fadeInDuration, int fadeOutDuration)
    :
    SpecialEffectProcess(),
    m_FadeInDuration(fadeInDuration),
    m_FadeOutDuration(fadeOutDuration),
    m_TeleportState(TeleportState_FadingIn),
    m_CurrentTime(0)
{
}

TeleportFadeInOutProcess::~TeleportFadeInOutProcess()
{

}

void TeleportFadeInOutProcess::VOnInit()
{
    SpecialEffectProcess::VOnInit();

    assert(g_pApp);
    assert(g_pApp->GetGameLogic());
    assert(g_pApp->GetHumanView());
    assert(g_pApp->GetHumanView()->GetCamera());

    // Stop game logic update during fade in/out
    g_pApp->GetGameLogic()->SetRunning(false);
    g_pApp->GetHumanView()->SetPostponeRenderPresent(true);
    g_pApp->GetHumanView()->SetRendering(false);

    Point windowSize = g_pApp->GetWindowSizeScaled();

    // Calculate graphics fade speed
    m_FadeInSpeed.x = (windowSize.x / (double)m_FadeInDuration) / 2.0;
    m_FadeInSpeed.y = (windowSize.y / (double)m_FadeInDuration) / 2.0;

    m_FadeOutSpeed.x = (windowSize.x / (double)m_FadeOutDuration) / 2.0;
    m_FadeOutSpeed.y = (windowSize.y / (double)m_FadeOutDuration) / 2.0;

    /*m_FragmentSize.Set(8, 8);

    m_pFadingTexture = Util::CreateSDLTextureRect(
        (int)m_FragmentSize.x, (int)m_FragmentSize.y, COLOR_BLACK, g_pApp->GetRenderer());
    assert(m_pFadingTexture != NULL);
    if (m_pFadingTexture == NULL)
    {
        LOG_ERROR("Could not create fading texture for TeleportFadeInOutProcess");
       Fail();
    }

    int numLines = (int)(windowSize.y / m_FragmentSize.y);
    int msPerLine = (m_FadeInDuration / numLines) + 1;
    for (int lineIdx = 0; lineIdx < numLines; lineIdx++)
    {
        int fadeDelay = (m_FadeInDuration - (lineIdx * msPerLine)) / 2;
        LOG("FadeDelay: " + ToStr(fadeDelay));
        shared_ptr<FadingLine> pLine(new FadingLine((int)windowSize.x, m_FragmentSize, fadeDelay, msPerLine, true));
        m_Lines.push_back(pLine);
    }*/
}

void TeleportFadeInOutProcess::VOnUpdate(uint32 msDiff)
{
    m_CurrentTime += msDiff;

    switch (m_TeleportState)
    {
        case TeleportState_FadingIn:
            /*for (shared_ptr<FadingLine> pLine : m_Lines)
            {
                pLine->Update(msDiff);
            }*/

            if (m_CurrentTime >= m_FadeInDuration)
            {
                m_CurrentTime = 0;
                m_TeleportState = TeleportState_FadingOut;
                g_pApp->GetHumanView()->SetRendering(true);

                /*for (shared_ptr<FadingLine> pLine : m_Lines)
                {
                    pLine->Reset(false);
                }*/

                //Succeed();
            }
            break;

        case TeleportState_FadingOut:
            if (m_CurrentTime >= m_FadeOutDuration)
            {
                Succeed();
            }
            break;

        default:
            LOG_ERROR("Unknown TeleportState: " + ToStr((int)m_TeleportState));
            break;
    }

    VRender(msDiff);
}

void TeleportFadeInOutProcess::VRender(uint32 msDiff)
{
    SDL_Renderer* pRenderer = g_pApp->GetRenderer();

    Point windowSize = g_pApp->GetWindowSizeScaled();

    // Render fade in/outs according to current state
    int referenceTime = 0;
    Point *fadeSpeed = &m_FadeInSpeed;
    if (m_TeleportState == TeleportState_FadingIn)
    {
        referenceTime = m_CurrentTime;
    }
    else if (m_TeleportState == TeleportState_FadingOut)
    {
        referenceTime = m_FadeOutDuration - m_CurrentTime;
        fadeSpeed = &m_FadeOutSpeed;
    }

    referenceTime = max(0, referenceTime);

    // Left -> right rect
    int currentWidth = (int)((double)referenceTime * fadeSpeed->x);
    //LOG("Current width: " + ToStr(currentWidth));
    SDL_Rect leftRect = { 0, 0, currentWidth, (int)windowSize.y };
    SDL_Texture* pLeftRectTexture = Util::CreateSDLTextureRect(leftRect.w, leftRect.h, COLOR_BLACK, pRenderer);
    SDL_RenderCopy(pRenderer, pLeftRectTexture, NULL, &leftRect);

    // Right -> left rect
    SDL_Rect rightRect = { (int)windowSize.x - currentWidth, 0, currentWidth, (int)windowSize.y };
    SDL_Texture* pRightRectTexture = Util::CreateSDLTextureRect(rightRect.w, rightRect.h, COLOR_BLACK, pRenderer);
    SDL_RenderCopy(pRenderer, pRightRectTexture, NULL, &rightRect);

    SDL_DestroyTexture(pLeftRectTexture);
    SDL_DestroyTexture(pRightRectTexture);

    Util::RenderForcePresent(pRenderer);
}