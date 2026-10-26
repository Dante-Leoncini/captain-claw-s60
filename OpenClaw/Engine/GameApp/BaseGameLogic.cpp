#include "../Actor/ActorFactory.h"
#include "../UserInterface/HumanView.h"
#include "../Events/Events.h"
#include "../Resource/Loaders/XmlLoader.h"
#include "../Resource/Loaders/PalLoader.h"
#include "../Resource/Loaders/WwdLoader.h"
#include "../Resource/Loaders/PcxLoader.h"
#include "../Events/EventMgr.h"
#include "../Graphics2D/Image.h"
#include "../Audio/Audio.h"

#include "../Actor/Components/PositionComponent.h"
#include "../Actor/Components/PickupComponents/PickupComponent.h"
#include "../Actor/Components/LootComponent.h"
#include "../Actor/Components/ControllerComponents/HealthComponent.h"
#include "../Actor/Components/ControllerComponents/ScoreComponent.h"
#include "../Actor/Components/ControllerComponents/LifeComponent.h"
#include "../Actor/Components/ControllerComponents/AmmoComponent.h"
#include "../Actor/Components/ControllableComponent.h"

#include "../Util/Converters.h"
#include "../Util/ClawLevelUtil.h"

#include "GameSaves.h"
#include "BaseGameLogic.h"
#include "GameSaves.h"

#include "../Physics/ClawPhysics.h"
#include "../Resource/Loaders/PidLoader.h"
#include "../Resource/ResourceMgr.h"

#include <algorithm>
#include <fstream>
//#include <thread>
#include <iostream>

//=================================================================================================
//
// BaseGameLogic implementation
//
//=================================================================================================

BaseGameLogic::BaseGameLogic()
{
    m_Lifetime = 0;
    m_bActiveAreaEnabled = false;
    m_pProcessMgr = NULL;
    m_LastActorId = 0;
    m_GameState = GameState_Initializing;
    m_HumanPlayersAttached = 0;
    m_AIPlayersAttached = 0;
    m_HumanGamesLoaded = 0;
    m_pActorFactory = NULL;
    m_Proxy = false;
    m_RenderDiagnostics = true;
    m_SelectedLevel = -1;
    m_bRunning = true;

    m_pGameSaveMgr.reset(new GameSaveMgr());

    //RegisterEngineScriptEvents();
    RegisterAllDelegates();
}

BaseGameLogic::~BaseGameLogic()
{
    while (!m_GameViews.empty())
    {
        m_GameViews.pop_front();
    }

    SAFE_DELETE(m_pProcessMgr);
    SAFE_DELETE(m_pActorFactory);

    // Destroy all actors
    for (ActorMap::iterator actorIter = m_ActorMap.begin(); actorIter != m_ActorMap.end(); ++actorIter)
    {
        actorIter->second->Destroy();
    }
    m_ActorMap.clear();

    RemoveAllDelegates();
}

bool BaseGameLogic::Initialize()
{
    m_pActorFactory = VCreateActorFactory();

    // TODO: Add saves to Emscripten indexed DB
    if (!m_pGameSaveMgr->IsSaveSupported()) {
        return m_pGameSaveMgr->Initialize(nullptr);
    }

    std::string savesFilePath = g_pApp->GetGameConfig()->userDirectory + g_pApp->GetGameConfig()->savesFile;

    TiXmlDocument gameSaves(savesFilePath.c_str());
    gameSaves.LoadFile();
    if (gameSaves.Error())
    {
        LOG_ERROR("Error while loading " + savesFilePath
             + ": " + std::string(gameSaves.ErrorDesc()));
        return false;
    }

    if (!m_pGameSaveMgr->Initialize(gameSaves.RootElement()))
    {
        return false;
    }

    return true;
}

std::string BaseGameLogic::GetActorXml(uint32 actorId)
{
    StrongActorPtr pActor = MakeStrongPtr(VGetActor(actorId));
    if (pActor)
    {
        return pActor->ToXML();
    }
    else
    {
        LOG_ERROR("Could not find actor: " + ToStr(actorId));
    }

    return "";
}

bool BaseGameLogic::VEnterMenu(const char* xmlMenuResource)
{
    TiXmlElement* pXmlLevelRoot = XmlResourceLoader::LoadAndReturnRootXmlElement(xmlMenuResource);
    if (pXmlLevelRoot == NULL)
    {
        LOG_ERROR("Could not load menu resource file: " + std::string(xmlMenuResource));
        return false;
    }

    for (GameViewList::iterator pGameView = m_GameViews.begin(); pGameView != m_GameViews.end(); ++pGameView)
    {
        if ((*pGameView)->VGetType() == GameView_Human)
        {
            shared_ptr<HumanView> pHumanView = static_pointer_cast<HumanView>(*pGameView);
            if (!pHumanView->EnterMenu(pXmlLevelRoot))
            {
                return false;
            }
        }
    }

    return true;
}

static void DrawLoadingScreen(shared_ptr<Image> pBackground, SDL_Rect& renderRect, Point& scale, float progress);

void RenderLoadingScreen(shared_ptr<Image> pBackground, SDL_Rect& renderRect, Point& scale, float progress)
{
    // While we are at it, eat incoming events
    SDL_Event evt;
    while (SDL_PollEvent(&evt))
    {
        g_pApp->OnEvent(evt);
    }

    DrawLoadingScreen(pBackground, renderRect, scale, progress);
    Util::RenderForcePresent(g_pApp->GetRenderer());
}

struct LoadingScreenFrame
{
    shared_ptr<Image> pBackground;
    SDL_Rect renderRect;
    Point scale;
    float progress;
};

static void DrawLoadingScreenFrame(void* pUserData)
{
    LoadingScreenFrame* pFrame = static_cast<LoadingScreenFrame*>(pUserData);
    DrawLoadingScreen(pFrame->pBackground, pFrame->renderRect, pFrame->scale, pFrame->progress);
}

// The loading screen comes out of black and goes back to it, as in the original game
static void FadeLoadingScreen(shared_ptr<Image> pBackground, SDL_Rect& renderRect, Point& scale, float progress, bool fadeIn)
{
    LoadingScreenFrame frame;
    frame.pBackground = pBackground;
    frame.renderRect = renderRect;
    frame.scale = scale;
    frame.progress = progress;
    Util::FadeScreen(&DrawLoadingScreenFrame, &frame, fadeIn ? 255 : 0, fadeIn ? 0 : 255, 400);
}

static void DrawLoadingScreen(shared_ptr<Image> pBackground, SDL_Rect& renderRect, Point& scale, float progress)
{
    SDL_Renderer* pRenderer = g_pApp->GetRenderer();
    SDL_RenderClear(pRenderer);

    SDL_RenderCopy(pRenderer, pBackground->GetTexture(), &renderRect, NULL);

    // Progress bar
    int progressFullLength = renderRect.w / 2;
    int progressCurrLength = (int)((progressFullLength * progress) / 100.0f);
    int progressHeight = (int)(30 * scale.x);
    SDL_Rect totalProgressBarRect = { renderRect.w / 4, (int)(renderRect.h * 0.75), progressFullLength, progressHeight };
    SDL_Rect remainingProgressBarRect = { renderRect.w / 4, (int)(renderRect.h * 0.75), progressCurrLength, progressHeight };

    SDL_Texture* pTotalProgressBar = Util::CreateSDLTextureRect(
        progressFullLength, progressHeight, COLOR_BLACK, pRenderer);
    SDL_Texture* pRemainingProgressBar = Util::CreateSDLTextureRect(
        remainingProgressBarRect.w, remainingProgressBarRect.h, COLOR_RED, pRenderer);

    SDL_RenderCopy(pRenderer, pTotalProgressBar, NULL, &totalProgressBarRect);
    SDL_RenderCopy(pRenderer, pRemainingProgressBar, NULL, &remainingProgressBarRect);

    SDL_DestroyTexture(pTotalProgressBar);
    SDL_DestroyTexture(pRemainingProgressBar);
}

// Images of what appears during play (score popups, loot, glitter, explosions, hits, HUD digits): loaded with
// the level and kept, so the first coin or the first explosion doesn't stall the game loading them
static std::vector<shared_ptr<Image> > s_GameplayImages;

static void LoadGameplayImages(WapPal* pPalette)
{
    static const char* const directories[] =
    {
        "/GAME/IMAGES/POINTS", "/GAME/IMAGES/TREASURE", "/GAME/IMAGES/GLITTER", "/GAME/IMAGES/GREENGLITTER",
        "/GAME/IMAGES/GLITTERRED", "/GAME/IMAGES/WARPGLITTER", "/GAME/IMAGES/EXPLOS", "/GAME/IMAGES/DYNAMITEEXPLO",
        "/GAME/IMAGES/DYNAMITELIT", "/GAME/IMAGES/DYNAMITE", "/GAME/IMAGES/PROJECTILES", "/GAME/IMAGES/BULLETS",
        "/GAME/IMAGES/ENEMYHIT", "/GAME/IMAGES/CLAWHIT", "/GAME/IMAGES/SPARKLE", "/GAME/IMAGES/SPARKLE2",
        "/GAME/IMAGES/RICOCHET", "/GAME/IMAGES/MAGIC", "/GAME/IMAGES/MAGICCLAW", "/GAME/IMAGES/HEALTH",
        "/GAME/IMAGES/AMMO", "/GAME/IMAGES/CATNIPS", "/GAME/IMAGES/POWERUPS", "/GAME/IMAGES/EXCLAMATION",
        "/GAME/IMAGES/CHECKPOINTFLAG", "/GAME/IMAGES/SUPERCHECKPOINT", "/GAME/IMAGES/LIGHTFX",
        // HUD digits: a new score or health shows digits nobody else keeps alive
        "/GAME/IMAGES/INTERFACE/SCORENUMBERS", "/GAME/IMAGES/INTERFACE/HEALTHNUMBERS",
        "/GAME/IMAGES/INTERFACE/SMALLNUMBERS", "/GAME/IMAGES/INTERFACE/STOPWATCH", "/GAME/IMAGES/INTERFACE/WEAPONS",
        "/GAME/IMAGES/INTERFACE/POWERUPS"
    };

    s_GameplayImages.clear();
    for (size_t dirIdx = 0; dirIdx < sizeof(directories) / sizeof(directories[0]); dirIdx++)
    {
        std::vector<std::string> files = g_pApp->GetResourceCache()->GetAllFilesInDirectory(directories[dirIdx]);
        for (size_t fileIdx = 0; fileIdx < files.size(); fileIdx++)
        {
            if (!WildcardMatch("*.pid", files[fileIdx].c_str()))
            {
                continue;
            }
            if (shared_ptr<Image> pImage = PidResourceLoader::LoadAndReturnImage(files[fileIdx].c_str(), pPalette))
            {
                s_GameplayImages.push_back(pImage);
            }
        }
    }
}

bool BaseGameLogic::VLoadGame(const char* xmlLevelResource)
{
    s_GameplayImages.clear();

    // The menu and the previous level are gone: their images (and textures) go with them, and so do
    // the pictures of OpenClaw's own archive (credits, touch controls), loaded again if ever shown
    g_pApp->GetResourceCache()->FlushImages("/states/");
    g_pApp->GetResourceCache()->FlushImages("/level");
    if (shared_ptr<ResourceCache> pCustomCache = g_pApp->GetResourceMgr()->VGetResourceCacheFromName(CUSTOM_RESOURCE))
    {
        pCustomCache->FlushImages("");
    }

    PROFILE_CPU("GAME LOADING");
    PROFILE_MEMORY("GAME LOADING");

    // Stop all audio
    g_pApp->GetAudio()->StopAllSounds();

    m_pPhysics.reset(CreateClawPhysics());

    float loadingProgress = 0.0f;
    float lastProgress = 0.0f;

    // Preload level resources
    std::string levelPath = "/LEVEL" + ToStr(m_pCurrentLevel->GetLevelNumber()) + "/*";
    g_pApp->GetResourceCache()->Preload(levelPath, NULL);
#ifdef OPENCLAW_FRAME_PROFILE
    LOG("[load] level resources preloaded");
#endif

    // ============== LOADING SCREEN RENDERING ==============

    // Start rendering the loading screen
    Point windowSize = g_pApp->GetWindowSize();
    Point scale = g_pApp->GetScale();
    int targetWidth = (int)(windowSize.x / scale.x);
    int targetHeight = (int)(windowSize.y / scale.y);
    SDL_Rect backgroundRect = { 0, 0, targetWidth, targetHeight };
    // Background loading image
    std::string backgroundPath = "/LEVEL" + ToStr(m_pCurrentLevel->m_LeveNumber) + "/SCREENS/LOADING.PCX";
    shared_ptr<Image> pBackgroundImage = PcxResourceLoader::LoadAndReturnImage(backgroundPath.c_str());
    assert(pBackgroundImage != nullptr);
    assert(pBackgroundImage->GetTexture() != NULL);

    FadeLoadingScreen(pBackgroundImage, backgroundRect, scale, loadingProgress, true);

    // ============== LEVEL LOADING ==============

    WapWwd* pWwd = WwdResourceLoader::LoadAndReturnWwd(xmlLevelResource);
    assert(pWwd != NULL);

    TiXmlElement* pXmlLevelRoot = WwdToXml(pWwd, m_pCurrentLevel->GetLevelNumber());
#ifdef OPENCLAW_FRAME_PROFILE
    LOG("[load] level converted to xml");
#endif
    if (pXmlLevelRoot == NULL)
    {
        LOG_ERROR("Could not load level resource file: " + std::string(xmlLevelResource));
        return false;
    }

    // Get level palette
    TiXmlElement* pLevelProperties = pXmlLevelRoot->FirstChildElement("LevelProperties");
    if (!pLevelProperties)
    {
        LOG_ERROR("Level does not have level properties node.");
        return false;
    }

    if (TiXmlElement* pLevelNameElem = pLevelProperties->FirstChildElement("LevelName"))
    {
        m_pCurrentLevel->m_LevelName = pLevelNameElem->GetText();
    }
    if (TiXmlElement* pLevelAuthorElem = pLevelProperties->FirstChildElement("Author"))
    {
        m_pCurrentLevel->m_LevelAuthor = pLevelAuthorElem->GetText();
    }
    if (TiXmlElement* pLevelCreatedDateElem = pLevelProperties->FirstChildElement("Created"))
    {
        m_pCurrentLevel->m_LevelCreatedDate = pLevelCreatedDateElem->GetText();
    }

    // Tile descriptions
    if (TiXmlElement* pTileDescRootElem = pLevelProperties->FirstChildElement("TileDescriptions"))
    {
        for (TiXmlElement* pTileDescElem = pTileDescRootElem->FirstChildElement("TileDescription");
            pTileDescElem; pTileDescElem = pTileDescElem->NextSiblingElement("TileDescription"))
        {

            // Maybe code more deffensively here, revisit in future probably.. right now I dont want to
            // add 10000 conditions to assert correct xml format

            // TileDescription will maybe be used by editor, it is not used directly by game
            //    but only to parse TileCollisionPrototype from it which is used by physics subsystem
            TileDescription tileDesc;
            tileDesc.tileId = std::stoi(pTileDescElem->FirstChildElement("TileId")->GetText());

            TiXmlElement* pTileSizeElem = pTileDescElem->FirstChildElement("Size");
            tileDesc.width = std::stoi(pTileSizeElem->Attribute("width"));
            tileDesc.height = std::stoi(pTileSizeElem->Attribute("height"));

            if (std::string(pTileDescElem->FirstChildElement("Type")->GetText()) == "single")
            {
                tileDesc.type = WAP_TILE_TYPE_SINGLE;
            }
            else
            {
                tileDesc.type = WAP_TILE_TYPE_DOUBLE;
                tileDesc.outsideAttrib = std::stoi(pTileDescElem->FirstChildElement("OutsideAttrib")->GetText());
            }
            tileDesc.insideAttrib = std::stoi(pTileDescElem->FirstChildElement("InsideAttrib")->GetText());

            TiXmlElement* pTileRectElem = pTileDescElem->FirstChildElement("TileRect");
            tileDesc.rect.left = std::stoi(pTileRectElem->Attribute("left"));
            tileDesc.rect.top = std::stoi(pTileRectElem->Attribute("top"));
            tileDesc.rect.right = std::stoi(pTileRectElem->Attribute("right"));
            tileDesc.rect.bottom = std::stoi(pTileRectElem->Attribute("bottom"));

            m_pCurrentLevel->m_TileDescriptionMap.insert(std::make_pair(tileDesc.tileId, tileDesc));

            // This structure is actually used in game in order to prevent recalculating the collision rects
            //    over and over again
            TileCollisionPrototype tileProto;
            tileProto.id = tileDesc.tileId;
            tileProto.width = tileDesc.width;
            tileProto.height = tileDesc.height;
            Util::ParseCollisionRectanglesFromTile(&tileProto, &tileDesc);

            m_pCurrentLevel->m_TileCollisionPrototypeMap.insert(std::make_pair(tileProto.id, tileProto));
        }
    }
    else
    {
        assert(false && "Tile descriptions element not found.");
    }

    loadingProgress = 10.0f;
    RenderLoadingScreen(pBackgroundImage, backgroundRect, scale, loadingProgress);

    // Get number of actors to estimate loading progress
    int numActors = 0;
    for (TiXmlElement* pActorElem = pXmlLevelRoot->FirstChildElement("Actor");
        pActorElem != NULL;
        pActorElem = pActorElem->NextSiblingElement("Actor"), numActors++);

    // Leave 90% for actor's processing
    float actorToPercent = (100.0f - loadingProgress - 5.0f) / (float)numActors;

    std::string palettePath = pLevelProperties->FirstChildElement("Palette")->GetText();
    std::replace(palettePath.begin(), palettePath.end(), '\\', '/');
    g_pApp->SetCurrentPalette(PalResourceLoader::LoadAndReturnPal(palettePath.c_str()));

    uint32 clawId = -1;
    // Each actor's XML is released once the actor exists: on big levels the whole tree is
    // tens of megabytes, which is too much for phones
    TiXmlElement* pNextActorElem = NULL;
    for (TiXmlElement* pActorElem = pXmlLevelRoot->FirstChildElement("Actor"); 
        pActorElem != NULL;
        pActorElem = pNextActorElem)
    {
        pNextActorElem = pActorElem->NextSiblingElement("Actor");
        //LOG("Creating actor: " + std::string(pActorElem->Attribute("Type")));
        //if (std::string(pActorElem->Attribute("Type")) != "Plane") break;
        StrongActorPtr pActor = VCreateActor(pActorElem, NULL);
        if (pActor)
        {
            shared_ptr<EventData_New_Actor> pNewActorEvent(new EventData_New_Actor(pActor->GetGUID()));
            IEventMgr::Get()->VQueueEvent(pNewActorEvent);

            // Get Claw's GUID
            if (pActor->GetName() == "Claw")
            {
                assert(clawId == -1 && "Multiple Captain Claws in this level - not supported at this time !");
                clawId = pActor->GetGUID();
            }
        }
        else
        {
            return false;
        }

        pXmlLevelRoot->RemoveChild(pActorElem);

        loadingProgress += actorToPercent;
        if ((loadingProgress - lastProgress) > 1.0f)
        {
            RenderLoadingScreen(pBackgroundImage, backgroundRect, scale, loadingProgress);
            lastProgress = loadingProgress;
        }
    }

#ifdef OPENCLAW_FRAME_PROFILE
    LOG("[load] actors created");
    ActorFactory::LogCreationProfile();
#endif

    LoadGameplayImages(g_pApp->GetCurrentPalette());
#ifdef OPENCLAW_FRAME_PROFILE
    LOG("[load] gameplay images: " + ToStr((int)s_GameplayImages.size()));
#endif

    // Loading the level pushed sounds and music out of the cache (the images took the room, which
    // they gave back once on the GPU): read them again now, not at the first coin or death
    const std::string levelDir = "/LEVEL" + ToStr(m_pCurrentLevel->m_LeveNumber);
    g_pApp->GetResourceCache()->Preload("/CLAW/SOUNDS/*", NULL);
    g_pApp->GetResourceCache()->Preload("/GAME/SOUNDS/*", NULL);
    g_pApp->GetResourceCache()->Preload("/GAME/MUSIC/*", NULL);
    g_pApp->GetResourceCache()->Preload(levelDir + "/SOUNDS/*", NULL);
    g_pApp->GetResourceCache()->Preload(levelDir + "/MUSIC/*", NULL);
#ifdef OPENCLAW_FRAME_PROFILE
    LOG("[load] sounds preloaded");
#endif

    // Load game save data
    const CheckpointSave* pCheckpointSave = m_pGameSaveMgr->GetCheckpointSave(
        m_pCurrentLevel->m_LeveNumber, m_pCurrentLevel->m_LoadedCheckpoint);
    assert(pCheckpointSave != NULL);

    loadingProgress = 95.0f;
    RenderLoadingScreen(pBackgroundImage, backgroundRect, scale, loadingProgress);

    // Load claw stats: Score, Health, Lives, Ammo: Bullets, Magic, Dynamite
    IEventMgr* pEventMgr = IEventMgr::Get();
    pEventMgr->VQueueEvent(IEventDataPtr(new EventData_Modify_Player_Stat(clawId, PlayerStat_Score, pCheckpointSave->score, false)));
    pEventMgr->VQueueEvent(IEventDataPtr(new EventData_Modify_Player_Stat(clawId, PlayerStat_Health, pCheckpointSave->health, false)));
    pEventMgr->VQueueEvent(IEventDataPtr(new EventData_Modify_Player_Stat(clawId, PlayerStat_Lives, pCheckpointSave->lives, false)));
    pEventMgr->VQueueEvent(IEventDataPtr(new EventData_Modify_Player_Stat(clawId, PlayerStat_Bullets, pCheckpointSave->bulletCount, false)));
    pEventMgr->VQueueEvent(IEventDataPtr(new EventData_Modify_Player_Stat(clawId, PlayerStat_Magic, pCheckpointSave->magicCount, false)));
    pEventMgr->VQueueEvent(IEventDataPtr(new EventData_Modify_Player_Stat(clawId, PlayerStat_Dynamite, pCheckpointSave->dynamiteCount, false)));

    // Set claw to spawn location
    m_CurrentSpawnPosition = GetSpawnPosition(m_pCurrentLevel->m_LeveNumber, m_pCurrentLevel->m_LoadedCheckpoint);
    pEventMgr->VTriggerEvent(IEventDataPtr(new EventData_Teleport_Actor(clawId, m_CurrentSpawnPosition)));

    // Save possible pickup items
    for (ActorMap::iterator actorIter = m_ActorMap.begin(); actorIter != m_ActorMap.end(); ++actorIter)
    {
        shared_ptr<TreasurePickupComponent> pTreasurePickupComponent =
            MakeStrongPtr(actorIter->second->GetComponent<TreasurePickupComponent>());
        if (pTreasurePickupComponent != nullptr)
        {
            PickupType treasurePickupType = pTreasurePickupComponent->GetPickupType();
            assert(treasurePickupType != PickupType_None);

            m_pCurrentLevel->m_TotalPickupsMap[treasurePickupType]++;
        }

        shared_ptr<LootComponent> pLootComponent =
            MakeStrongPtr(actorIter->second->GetComponent<LootComponent>());
        if (pLootComponent)
        {
            const std::vector<PickupType>* pLootList = pLootComponent->GetLoot();
            for (size_t pickupIdx = 0; pickupIdx < (*pLootList).size(); pickupIdx++)
            {
                PickupType pickup = (*pLootList)[pickupIdx];
                assert(pickup != PickupType_None);

                m_pCurrentLevel->m_TotalPickupsMap[pickup]++;
            }
        }
    }

    // Notify all human views
    for (GameViewList::iterator pGameView = m_GameViews.begin(); pGameView != m_GameViews.end(); ++pGameView)
    {
        if ((*pGameView)->VGetType() == GameView_Human)
        {
            shared_ptr<HumanView> pHumanView = static_pointer_cast<HumanView>(*pGameView);
            pHumanView->LoadGame(pXmlLevelRoot, m_pCurrentLevel.get());
        }
    }

    loadingProgress = 100.0f;
    FadeLoadingScreen(pBackgroundImage, backgroundRect, scale, loadingProgress, false);

    LOG("Level loaded !");
    LOG("Level name: " + m_pCurrentLevel->m_LevelName);
    LOG("Level author: " + m_pCurrentLevel->m_LevelAuthor);
    LOG("Level created date: " + m_pCurrentLevel->m_LevelCreatedDate);

    // TODO: This is a bit hacky but it helps with development (prevents entering cheats over and over again). It can be data driven.
    LOG("Loading startup ingame commands...");
    ExecuteStartupCommands(g_pApp->GetGameConfig()->startupCommandsFile);

    /*int skullsCount = m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Skull_Blue] +
        m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Skull_Red] +
        m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Skull_Purple] +
        m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Skull_Green];
    LOG("Skull count: " + ToStr(skullsCount));

    int crownsCount = m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Crowns_Blue] +
        m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Crowns_Red] +
        m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Crowns_Purple] +
        m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Crowns_Green];
    LOG("crownsCount count: " + ToStr(crownsCount));

    int geckoCount = m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Geckos_Blue] +
        m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Geckos_Red] +
        m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Geckos_Purple] +
        m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Geckos_Green];
    LOG("geckoCount count: " + ToStr(geckoCount));

    int coinsCount = m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Coins] +
        m_pCurrentLevel->m_TotalPickupsMap[PickupType_Default];
    LOG("coinsCount count: " + ToStr(coinsCount));

    int Rings = m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Rings_Blue] +
        m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Rings_Red] +
        m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Rings_Purple] +
        m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Rings_Green];
    LOG("Rings count: " + ToStr(Rings));*/

    pEventMgr->VTriggerEvent(IEventDataPtr(new EventData_World_Finished_Loading()));

    SAFE_DELETE(pXmlLevelRoot);

    return true;
}

bool BaseGameLogic::VLoadScoreScreen(const char* xmlScoreScreenResource)
{
    TiXmlElement* pScoreScreenRootElem = XmlResourceLoader::LoadAndReturnRootXmlElement(xmlScoreScreenResource);
    if (pScoreScreenRootElem == NULL)
    {
        LOG_ERROR("Failed to load score screen XML resource: " + std::string(xmlScoreScreenResource));
        return false;
    }

    // Save game progress
    // When Claw finishes level, the saved checkpoint is (CurrentLevel + 1) level start

    // For convinience
    StrongActorPtr pClaw = GetClawActor();
    assert(pClaw != nullptr);

    shared_ptr<ScoreComponent> pScoreComponent = MakeStrongPtr(pClaw->GetComponent<ScoreComponent>(ScoreComponent::g_Name));
    shared_ptr<HealthComponent> pHealthComponent = MakeStrongPtr(pClaw->GetComponent<HealthComponent>(HealthComponent::g_Name));
    shared_ptr<LifeComponent> pLifeComponent = MakeStrongPtr(pClaw->GetComponent<LifeComponent>(LifeComponent::g_Name));
    shared_ptr<AmmoComponent> pAmmoComponent = MakeStrongPtr(pClaw->GetComponent<AmmoComponent>(AmmoComponent::g_Name));
    assert(pScoreComponent);
    assert(pHealthComponent);
    assert(pLifeComponent);
    assert(pAmmoComponent);

    CheckpointSave nextLevelCheckpoint;

    nextLevelCheckpoint.checkpointIdx = 0;
    nextLevelCheckpoint.score = pScoreComponent->GetScore();
    nextLevelCheckpoint.health = pHealthComponent->GetHealth();
    nextLevelCheckpoint.lives = pLifeComponent->GetLives();
    nextLevelCheckpoint.bulletCount = pAmmoComponent->GetRemainingAmmo(AmmoType_Pistol);
    nextLevelCheckpoint.magicCount = pAmmoComponent->GetRemainingAmmo(AmmoType_Magic);
    nextLevelCheckpoint.dynamiteCount = pAmmoComponent->GetRemainingAmmo(AmmoType_Dynamite);

    int finishedLevelNumber = m_pCurrentLevel->GetLevelNumber();
    int nextLevelNumber = m_pCurrentLevel->GetLevelNumber() + 1;

    // Level 11 is the last implemented level
    if (nextLevelNumber <= 11)
    {
        m_pGameSaveMgr->AddCheckpointSave(nextLevelNumber, nextLevelCheckpoint);

        // If not in testing mode
        if (m_pGameSaveMgr->IsSaveSupported() && !g_pApp->GetGlobalOptions()->loadAllLevelSaves)
        {
            TiXmlDocument saveGamesDoc;
            saveGamesDoc.LinkEndChild(m_pGameSaveMgr->ToXml());
            std::string savesFilePath = g_pApp->GetGameConfig()->userDirectory + g_pApp->GetGameConfig()->savesFile;
            saveGamesDoc.SaveFile(savesFilePath.c_str());
        }
    }

    // Unload the finished level
    UnloadLevel();

    // Gather information about collected treasure items
    for (TiXmlElement* pScoreRowElem = pScoreScreenRootElem->FirstChildElement("ScoreRow");
        pScoreRowElem != NULL;
        pScoreRowElem = pScoreRowElem->NextSiblingElement("ScoreRow"))
    {
        std::string treasureTypeStr;
        DO_AND_CHECK(ParseAttributeFromXmlElem(&treasureTypeStr, "Treasure", pScoreRowElem));

        int pickedUpCount = 0;
        int totalCount = 0;
        // Better solution ?
        if (treasureTypeStr == "Skull")
        {
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Skull_Red];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Skull_Green];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Skull_Blue];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Skull_Purple];

            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Skull_Red];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Skull_Green];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Skull_Blue];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Skull_Purple];


        }
        else if (treasureTypeStr == "Crown")
        {
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Crowns_Red];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Crowns_Green];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Crowns_Blue];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Crowns_Purple];

            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Crowns_Red];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Crowns_Green];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Crowns_Blue];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Crowns_Purple];
        }
        else if (treasureTypeStr == "Gecko")
        {
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Geckos_Red];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Geckos_Green];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Geckos_Blue];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Geckos_Purple];

            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Geckos_Red];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Geckos_Green];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Geckos_Blue];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Geckos_Purple];
        }
        else if (treasureTypeStr == "Scepter")
        {
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Scepters_Red];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Scepters_Green];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Scepters_Blue];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Scepters_Purple];

            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Scepters_Red];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Scepters_Green];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Scepters_Blue];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Scepters_Purple];
        }
        else if (treasureTypeStr == "Cross")
        {
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Crosses_Red];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Crosses_Green];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Crosses_Blue];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Crosses_Purple];

            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Crosses_Red];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Crosses_Green];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Crosses_Blue];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Crosses_Purple];
        }
        else if (treasureTypeStr == "Chalice")
        {
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Chalices_Red];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Chalices_Green];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Chalices_Blue];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Chalices_Purple];

            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Chalices_Red];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Chalices_Green];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Chalices_Blue];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Chalices_Purple];
        }
        else if (treasureTypeStr == "Ring")
        {
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Rings_Red];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Rings_Green];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Rings_Blue];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Rings_Purple];

            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Rings_Red];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Rings_Green];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Rings_Purple];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Rings_Purple];
        }
        else if (treasureTypeStr == "Goldbar")
        {
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Goldbars];

            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Goldbars];
        }
        else if (treasureTypeStr == "Coin")
        {
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Default];
            pickedUpCount += m_pCurrentLevel->m_LootedPickupsMap[PickupType_Treasure_Coins];

            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Default];
            totalCount += m_pCurrentLevel->m_TotalPickupsMap[PickupType_Treasure_Coins];
        }
        else
        {
            LOG_ERROR("Offending treasure type: " + treasureTypeStr);
            assert(false && "Unknown treasuretype");
        }

        /*LOG("[TOTAL] " + treasureTypeStr + ": " + ToStr(totalCount));
        LOG("[PICKED UP] " + treasureTypeStr + ": " + ToStr(pickedUpCount));*/

        DO_AND_CHECK(SetTiXmlNodeValue(pScoreRowElem, "ScoreRow.CountOfPickedUpScoreItems", pickedUpCount));
        DO_AND_CHECK(SetTiXmlNodeValue(pScoreRowElem, "ScoreRow.CountOfTotalScoreItemsInLevel", totalCount));
    }

    const CheckpointSave* pStartLevelSave = m_pGameSaveMgr->GetCheckpointSave(finishedLevelNumber, 0);
    assert(pStartLevelSave != NULL);

    int startLevelScore = pStartLevelSave->score;
    int levelScoreCollected = nextLevelCheckpoint.score - startLevelScore;

    //<ScorePointsCollectedInLevel>0<ScorePointsCollectedInLevel>
    DO_AND_CHECK(SetTiXmlNodeValue(pScoreScreenRootElem, "FinishedLevelScreen.NextLevelNumber", nextLevelNumber));
    DO_AND_CHECK(SetTiXmlNodeValue(pScoreScreenRootElem, "FinishedLevelScreen.ScorePointsOnLevelStart", startLevelScore));
    DO_AND_CHECK(SetTiXmlNodeValue(pScoreScreenRootElem, "FinishedLevelScreen.ScorePointsCollectedInLevel", levelScoreCollected));

    // Load the ScoreScreen
    g_pApp->GetHumanView()->LoadScoreScreen(pScoreScreenRootElem);

    return true;
}

void BaseGameLogic::VSetProxy()
{
    m_Proxy = true;
}

StrongActorPtr BaseGameLogic::VCreateActor(const std::string& xmlActorResource, TiXmlElement* overrides)
{
    assert(m_pActorFactory);
    
    StrongActorPtr pActor = m_pActorFactory->CreateActor(xmlActorResource.c_str(), overrides);
    if (pActor)
    {
        m_ActorMap.insert(std::make_pair(pActor->GetGUID(), pActor));
        if (m_GameState == GameState_IngameRunning)
        {
            // Create event that actor was created
        }
        return pActor;
    }
    else
    {
        return StrongActorPtr();
    }
}

StrongActorPtr BaseGameLogic::VCreateActor(TiXmlElement* pActorRoot, TiXmlElement* overrides)
{
    assert(m_pActorFactory);

    StrongActorPtr pActor = m_pActorFactory->CreateActor(pActorRoot, overrides);
    if (pActor)
    {
        m_ActorMap.insert(std::make_pair(pActor->GetGUID(), pActor));
        if (m_GameState == GameState_IngameRunning)
        {
            // Create event that actor was created
        }
        return pActor;
    }
    else
    {
        return StrongActorPtr();
    }
}

void BaseGameLogic::VDestroyActor(const uint32 actorId)
{
    // Trigger actor destroyed event prior removing it here

    ActorMap::iterator findIter = m_ActorMap.find(actorId);
    if (findIter != m_ActorMap.end())
    {
        //LOG("Destroying: " + ToStr(actorId));
        findIter->second->Destroy();
        m_ActorMap.erase(findIter);
    }
}

WeakActorPtr BaseGameLogic::VGetActor(const uint32 actorId)
{
    ActorMap::iterator findIter = m_ActorMap.find(actorId);
    if (findIter != m_ActorMap.end())
    {
        return findIter->second;
    }

    return WeakActorPtr();
}

void BaseGameLogic::VModifyActor(const uint32 actorId, TiXmlElement* overrides)
{
    assert(m_pActorFactory);

    ActorMap::iterator findIter = m_ActorMap.begin();
    if (findIter != m_ActorMap.end())
    {
        m_pActorFactory->ModifyActor(findIter->second, overrides);
    }
}

void BaseGameLogic::UpdateActiveArea()
{
    m_bActiveAreaEnabled = false;
    const int margin = g_pApp->GetGlobalOptions()->activeAreaMargin;
    if (margin <= 0)
    {
        return;
    }

    for (GameViewList::iterator pGameView = m_GameViews.begin(); pGameView != m_GameViews.end(); ++pGameView)
    {
        if ((*pGameView)->VGetType() == GameView_Human)
        {
            shared_ptr<CameraNode> pCamera = static_cast<HumanView*>((*pGameView).get())->GetCamera();
            if (pCamera)
            {
                SDL_Rect cameraRect = pCamera->GetCameraRect();
                m_ActiveArea.x = cameraRect.x - margin;
                m_ActiveArea.y = cameraRect.y - margin;
                m_ActiveArea.w = cameraRect.w + margin * 2;
                m_ActiveArea.h = cameraRect.h + margin * 2;
                m_bActiveAreaEnabled = true;
            }
            break;
        }
    }
}

bool BaseGameLogic::IsInActiveArea(const Point& position) const
{
    if (!m_bActiveAreaEnabled)
    {
        return true;
    }

    return position.x >= m_ActiveArea.x && position.x < m_ActiveArea.x + m_ActiveArea.w &&
        position.y >= m_ActiveArea.y && position.y < m_ActiveArea.y + m_ActiveArea.h;
}

void BaseGameLogic::VOnUpdate(uint32 msDiff)
{
    m_Lifetime += msDiff;

    // TODO: This is code duplication, should think of better way
    if (!m_bRunning)
    {
        for (GameViewList::iterator pGameView = m_GameViews.begin(); pGameView != m_GameViews.end(); ++pGameView)
        {
            (*pGameView)->VOnUpdate(msDiff);
        }

        return;
    }

    switch (m_GameState)
    {
        case GameState_Initializing:
        {
#ifdef ANDROID
            VChangeState(GameState_LoadingLevel);
#else
            if (g_pApp->GetDebugOptions()->bSkipMenu)
            {
                VChangeState(GameState_LoadingLevel);
            }
            else
            {
                VChangeState(GameState_Menu);
            }
#endif
            break;
        }

        case GameState_LoadingMenu:
        {
            VChangeState(GameState_Menu);
            break;
        }

        case GameState_Menu:
        {
            break;
        }

        case GameState_LoadingLevel:
        {
            VChangeState(GameState_IngameRunning);

            // The level data places Claw and the objects a few pixels above the ground: let them
            // land before the first frame is shown, like the original game, where everything starts
            // standing in place
            for (int settleStep = 0; settleStep < 10; settleStep++)
            {
                IEventMgr::Get()->VUpdate(IEventMgr::kINFINITE);
                VOnUpdate(16);
            }

            // Then the level unrolls out of black, as a still picture, before the game starts
            if (HumanView* pHumanView = g_pApp->GetHumanView())
            {
                pHumanView->PlayLevelStartReveal();
            }
            break;
        }

        case GameState_LoadingScoreScreen:
        {
            VChangeState(GameState_ScoreScreen);
            break;
        }

        case GameState_ScoreScreen:
        {
            break;
        }

        case GameState_IngameRunning:
        {
            UpdateActiveArea();

            if (m_pProcessMgr)
            {
                PROFILE_FRAME_SECTION(FrameSection_Processes);
                m_pProcessMgr->UpdateProcesses(msDiff);
            }
            
            if (m_pPhysics)
            {
                //PROFILE_CPU("PHYSICS");
                // TODO: Add config to choose between fixed physics timestep and variable
                if (true)
                {
                    {
                        PROFILE_FRAME_SECTION(FrameSection_PhysicsStep);
                        m_pPhysics->VOnUpdate(msDiff);
                    }
                    PROFILE_FRAME_SECTION(FrameSection_PhysicsSync);
                    m_pPhysics->VSyncVisibleScene();
                }
                else
                {
                    static uint32 timeSinceLastUpdate = 0;
                    const uint32 updateInterval = 1000 / 120;

                    timeSinceLastUpdate += msDiff;
                    if (timeSinceLastUpdate >= updateInterval)
                    {
                        //PROFILE_CPU("PHYSICS");
                        //LOG(ToStr(timeSinceLastUpdate));
                        m_pPhysics->VOnUpdate(timeSinceLastUpdate);
                        m_pPhysics->VSyncVisibleScene();

                        timeSinceLastUpdate = 0;
                    }
                }
                break;
            }

            break;
        }

        default:
        {
            LOG_ERROR("Unknown state: " + ToStr(m_GameState));
            break;
        }
    }

    // Update all game views
    {
        PROFILE_FRAME_SECTION(FrameSection_Views);
        for (GameViewList::iterator pGameView = m_GameViews.begin(); pGameView != m_GameViews.end(); ++pGameView)
        {
            (*pGameView)->VOnUpdate(msDiff);
        }
    }

    // Limit update to max 100 times / second
    static int msAccumulation = 0;
    msAccumulation += msDiff;
    if (msAccumulation >= 5)
    {
        // Update all game actors
        PROFILE_FRAME_SECTION(FrameSection_Actors);
        for (ActorMap::iterator actorIter = m_ActorMap.begin(); actorIter != m_ActorMap.end(); ++actorIter)
        {
            if (m_bActiveAreaEnabled)
            {
                shared_ptr<PositionComponent> pPositionComponent = actorIter->second->GetPositionComponent();
                if (pPositionComponent && !IsInActiveArea(pPositionComponent->GetPosition()))
                {
                    continue;
                }
            }
            actorIter->second->Update(msAccumulation);
        }
        msAccumulation = 0;
    }
}

void BaseGameLogic::VChangeState(GameState newState)
{
    if (newState == GameState_Menu)
    {
        // Unload level if applicable
        UnloadLevel();
        if (!VEnterMenu("MENU.XML"))
        {
            LOG_ERROR("Failed to enter menu");
            g_pApp->Terminate();
            exit(1);
        }
    }
    else if (newState == GameState_LoadingLevel)
    {
        // In case of debugging
        if (m_pCurrentLevel == nullptr)
        {
            m_pCurrentLevel = ClawLevelUtil::GetDebugLoadLevelData();
        }

        int levelNumber = m_pCurrentLevel->GetLevelNumber();
        assert(levelNumber >= 0 && levelNumber <= 14);

        std::string levelName = "LEVEL" + ToStr(levelNumber);
        std::string wwdLevelPath = "/" + levelName + "/WORLDS/WORLD.WWD";

        // Load saved level file, e.g. LEVEL1.xml
        if (!VLoadGame(wwdLevelPath.c_str()))
        {
            LOG_ERROR("Could not load level");
            exit(1);
        }
    }
    else if (newState == GameState_ScoreScreen)
    {
        // Dummy for testing
        if (m_pCurrentLevel == nullptr)
        {
            m_pCurrentLevel.reset(new LevelData(1, false, 2));
        }

        std::string finishedLevelXmlDescPath = "/FINISHED_LEVEL_SCENES/LEVEL" + ToStr(m_pCurrentLevel->m_LeveNumber) + ".XML";

        if (!VLoadScoreScreen(finishedLevelXmlDescPath.c_str()))
        {
            LOG_ERROR("Could not load score screen");
            exit(1);
        }
    }

    LOG("Changing to: " + ToStr(newState));
    m_GameState = newState;
}

void BaseGameLogic::VRenderDiagnostics(SDL_Renderer* pRenderer, shared_ptr<CameraNode> pCamera)
{
    if (m_RenderDiagnostics && m_pPhysics)
    {
        m_pPhysics->VRenderDiagnostics(pRenderer, pCamera);
    }
}

void BaseGameLogic::VAddView(shared_ptr<IGameView> pView, uint32 actorId)
{
    int viewId = static_cast<int>(m_GameViews.size());
    m_GameViews.push_back(pView);
    pView->VOnAttach(viewId, actorId);
}

void BaseGameLogic::VRemoveView(shared_ptr<IGameView> pView)
{
    m_GameViews.remove(pView);
}

ActorFactory* BaseGameLogic::VCreateActorFactory()
{
    return new ActorFactory();
}

void BaseGameLogic::RequestDestroyActorDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Destroy_Actor> pCastEventData = static_pointer_cast<EventData_Destroy_Actor>(pEventData);
    VDestroyActor(pCastEventData->GetActorId());
    if (m_pPhysics != nullptr)
    {
        m_pPhysics->VRemoveActor(pCastEventData->GetActorId());
    }
}

void BaseGameLogic::MoveActorDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Move_Actor> pCastEventData = static_pointer_cast<EventData_Move_Actor>(pEventData);

    StrongActorPtr pActor = MakeStrongPtr(VGetActor(pCastEventData->GetActorId()));
    if (pActor != nullptr)
    {
        shared_ptr<PositionComponent> pPositionComponent = pActor->GetPositionComponent();
        assert(pPositionComponent != nullptr);

        pPositionComponent->SetPosition(pCastEventData->GetMove());
    }
}

void BaseGameLogic::RequestNewActorDelegate(IEventDataPtr pEventData)
{

}

static std::map<CollisionType, FixtureType> CreateCollisionToFixtureTypeMap()
{
    std::map<CollisionType, FixtureType> collisionToFixtureTypeMap;
    collisionToFixtureTypeMap[CollisionType_None] = FixtureType_None;
    collisionToFixtureTypeMap[CollisionType_Solid] = FixtureType_Solid;
    collisionToFixtureTypeMap[CollisionType_Ground] = FixtureType_Ground;
    collisionToFixtureTypeMap[CollisionType_Climb] = FixtureType_Climb;
    collisionToFixtureTypeMap[CollisionType_Death] = FixtureType_Death;
    return collisionToFixtureTypeMap;
}

std::map<CollisionType, FixtureType> g_CollisionToFixtureTypeMap = CreateCollisionToFixtureTypeMap();

static FixtureType CollisonToFixtureType(CollisionType collisionType)
{
    std::map<CollisionType, FixtureType>::iterator findIt = g_CollisionToFixtureTypeMap.find(collisionType);
    assert(findIt != g_CollisionToFixtureTypeMap.end());

    return findIt->second;
}

// Helper function
void BaseGameLogic::CreateSinglePhysicsTile(int x, int y, const TileCollisionPrototype& proto)
{
    for (std::vector<TileCollisionRectangle>::const_iterator tileCollisionRect = proto.collisionRectangles.begin(); tileCollisionRect != proto.collisionRectangles.end(); ++tileCollisionRect)
    {
        Point position = Point(x + tileCollisionRect->collisionRect.x,
            y + tileCollisionRect->collisionRect.y);
        Point size = Point(tileCollisionRect->collisionRect.w, tileCollisionRect->collisionRect.h);

        m_pPhysics->VAddStaticGeometry(
            position, 
            size, 
            tileCollisionRect->collisionType, 
            CollisonToFixtureType(tileCollisionRect->collisionType));
    }

    Point topLadderOffset;
    if (ClawLevelUtil::TryGetTopLadderInfo(m_pCurrentLevel->GetLevelNumber(), proto.id, topLadderOffset))
    {
        Point position = Point(x, y) + topLadderOffset;
        Point size(64, 10);

        m_pPhysics->VAddStaticGeometry(position, size, CollisionType_Ground, FixtureType_TopLadderGround);
    }
}

void BaseGameLogic::CollideableTileCreatedDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Collideable_Tile_Created> pCastEventData = static_pointer_cast<EventData_Collideable_Tile_Created>(pEventData);

    //-------------------------------------------------------------------------
    // (1) First get all info about the tile, its position etc we want to create
    //-------------------------------------------------------------------------

    int tileId = pCastEventData->GetTileId();
    int tileX = pCastEventData->GetPositionX();
    int tileY = pCastEventData->GetPositionY();
    int numTiles = pCastEventData->GetTilesCount();
    assert(numTiles >= 1);

    TileCollisionPrototypeMap::iterator findIt = m_pCurrentLevel->m_TileCollisionPrototypeMap.find(tileId);
    if (findIt == m_pCurrentLevel->m_TileCollisionPrototypeMap.end())
    {
        LOG_ERROR("Unknown tile! Id = " + ToStr(tileId));
        assert(false && "Unknown tile");
    }

    const TileCollisionPrototype& tileProto = findIt->second;

    //-------------------------------------------------------------------------
    // (2) Create the tile in the physics world appropriately
    //    - If its single tile, create it simply from prototype
    //    - If its multiple same tiles, check if we can merge them, if yes, 
    //      merge and create, if not, create them separately
    //-------------------------------------------------------------------------

    if (numTiles == 1)
    {
        CreateSinglePhysicsTile(tileX, tileY, tileProto);
    }
    else
    {
        // Multiple tiles in a row, check if we can marge them
        bool bCanBeMerged = true;

        int countAttrTiles = 0;
        TileCollisionRectangle mergedRect;
        for (size_t colRectIdx = 0; colRectIdx < tileProto.collisionRectangles.size(); colRectIdx++)
        {
            const TileCollisionRectangle& colRect = tileProto.collisionRectangles[colRectIdx];
            if (colRect.collisionType != CollisionType_None)
            {
                countAttrTiles++;
                mergedRect = colRect;

                if (colRect.collisionRect.w != tileProto.width)
                {
                    bCanBeMerged = false;
                }
            }
        }

        if (countAttrTiles != 1)
        {
            bCanBeMerged = false;
        }

        // If they can be merged, merge them
        if (bCanBeMerged)
        {
            assert(mergedRect.collisionRect.x == 0);
            assert(mergedRect.collisionRect.w == tileProto.width);
            assert(mergedRect.collisionRect.h > 0);

            Point mergedPosition(tileX + mergedRect.collisionRect.x, tileY + mergedRect.collisionRect.y);
            Point mergedSize(tileProto.width * numTiles, mergedRect.collisionRect.h);

            m_pPhysics->VAddStaticGeometry(
                mergedPosition, 
                mergedSize, 
                mergedRect.collisionType,
                CollisonToFixtureType(mergedRect.collisionType));
        }
        else
        {
            // If not, create them one by one
            for (int tileNumIdx = 0; tileNumIdx < numTiles; tileNumIdx++)
            {
                CreateSinglePhysicsTile(tileX, tileY, tileProto);
                tileX += tileProto.width;
            }
        }
    }
}

void BaseGameLogic::CreateStaticGeometryDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Add_Static_Geometry> pCastEventData =
        static_pointer_cast<EventData_Add_Static_Geometry>(pEventData);
}

void BaseGameLogic::ItemPickedUpDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Item_Picked_Up> pCastEventData =
        static_pointer_cast<EventData_Item_Picked_Up>(pEventData);

    //LOG("Picked up: " + ActorTemplates::EnumToString_PickupTypeToImageSet(pCastEventData->GetPickupType()));
    m_pCurrentLevel->m_LootedPickupsMap[pCastEventData->GetPickupType()]++;
}

void BaseGameLogic::FinishedLevelDelegate(IEventDataPtr pEventData)
{
    VChangeState(GameState_LoadingScoreScreen);
}

void BaseGameLogic::ActorEnteredBossAreaDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Entered_Boss_Area> pCastEventData =
        static_pointer_cast<EventData_Entered_Boss_Area>(pEventData);

    ActorMap::iterator findIt = m_ActorMap.find(pCastEventData->GetControllerId());
    assert(findIt != m_ActorMap.end());

    StrongActorPtr pClaw = findIt->second;

    shared_ptr<ClawControllableComponent> pClawComponent = 
        MakeStrongPtr(pClaw->GetComponent<ClawControllableComponent>());
    assert(pClawComponent != nullptr);

    pClawComponent->SetFrozen(true);
}

void BaseGameLogic::BossFightStartedDelegate(IEventDataPtr pEventData)
{
    shared_ptr<EventData_Boss_Fight_Started> pCastEventData =
        static_pointer_cast<EventData_Boss_Fight_Started>(pEventData);

    ActorMap::iterator findIt = m_ActorMap.find(pCastEventData->GetControllerId());
    assert(findIt != m_ActorMap.end());

    StrongActorPtr pClaw = findIt->second;

    shared_ptr<ClawControllableComponent> pClawComponent =
        MakeStrongPtr(pClaw->GetComponent<ClawControllableComponent>());
    assert(pClawComponent != nullptr);

    pClawComponent->SetFrozen(false);
}

void BaseGameLogic::IngameMenuEndLifeDelegate(IEventDataPtr pEventData)
{
    StrongActorPtr pClaw = GetClawActor();
    assert(pClaw != nullptr);

    shared_ptr<HealthComponent> pClawHealthComponent = MakeStrongPtr(pClaw->GetComponent<HealthComponent>());
    assert(pClawHealthComponent != nullptr);

    pClawHealthComponent->SetCurrentHealth(-1);
}

void BaseGameLogic::WorldFinishedLoadingDelegate(IEventDataPtr pEventData)
{
    for (ActorMap::iterator actorPair = m_ActorMap.begin(); actorPair != m_ActorMap.end(); ++actorPair)
    {
        actorPair->second->OnWorldFinishedLoading();
    }
}

StrongActorPtr BaseGameLogic::GetClawActor()
{
    StrongActorPtr claw = MakeStrongPtr(m_pClawActor);
    if (claw) {
        return claw;
    }

    for (ActorMap::iterator actorIter = m_ActorMap.begin(); actorIter != m_ActorMap.end(); ++actorIter)
    {
        if (actorIter->second->GetName() == "Claw")
        {
            m_pClawActor = actorIter->second;
            return actorIter->second;
        }
    }

    return nullptr;
}

StrongActorPtr BaseGameLogic::FindActorByName(const std::string& name, bool bIsUnique)
{
    StrongActorPtr pFoundActor = nullptr;
    for (ActorMap::iterator actorIter = m_ActorMap.begin(); actorIter != m_ActorMap.end(); ++actorIter)
    {
        if (actorIter->second->GetName() == name)
        {
            if (pFoundActor != nullptr && bIsUnique)
            {
                LOG_ASSERT("Actor with name: \"" + name + "\" was marked as unique but occured more than once !");
            }
            pFoundActor = actorIter->second;
        }
    }

    return pFoundActor;
}

ActorList BaseGameLogic::FindActorByName(const std::string& name)
{
    ActorList actorList;

    for (ActorMap::iterator actorIter = m_ActorMap.begin(); actorIter != m_ActorMap.end(); ++actorIter)
    {
        if (actorIter->second->GetName() == name)
        {
            actorList.push_back(actorIter->second.get());
        }
    }

    return actorList;
}

void BaseGameLogic::UnloadLevel()
{
    // We cannot do this inside event update loop
    assert(IEventMgr::Get()->VIsUpdating() == false);

    LOG("Unloading level: " + ToStr((int)m_ActorMap.size()) + " actors");

    // Handle all pending events before reset
    IEventMgr::Get()->VUpdate(IEventMgr::kINFINITE);

    // We need to create a copy here because the m_ActorMap will be modified by future events will
    // will arise
    ActorMap actorMapCopy = m_ActorMap;

    for (ActorMap::iterator actor = actorMapCopy.begin(); actor != actorMapCopy.end(); ++actor)
    {
        shared_ptr<EventData_Destroy_Actor> pEvent(new EventData_Destroy_Actor(actor->second->GetGUID()));
        IEventMgr::Get()->VTriggerEvent(pEvent);
    }

    // These events have to ensure that the map will be in fact empty
    assert(m_ActorMap.empty());

    LOG("Actors destroyed");

    // Process any pending events which could have arose from deleting all actors
    IEventMgr::Get()->VUpdate(IEventMgr::kINFINITE);

    //m_pCurrentLevel.reset();

    m_pPhysics.reset();
    LOG("Level unloaded");
}

void BaseGameLogic::VResetLevel()
{
    // Handle all pending events before reset
    IEventMgr::Get()->VUpdate(IEventMgr::kINFINITE);

    for (ActorMap::iterator actorIter = m_ActorMap.begin(); actorIter != m_ActorMap.end(); ++actorIter)
    {
        shared_ptr<EventData_Destroy_Actor> pEvent(new EventData_Destroy_Actor(actorIter->second->GetGUID()));
        IEventMgr::Get()->VTriggerEvent(pEvent);
    }

    assert(m_ActorMap.empty());
    assert(m_pClawActor.expired());

    // Process any pending events which could have arose from deleting all actors
    IEventMgr::Get()->VUpdate(IEventMgr::kINFINITE);

    // Reset physics. TODO: is replacing pointer which is shared between multiple classes OK like this ?
    m_pPhysics.reset(CreateClawPhysics());

    // Load new level
    VChangeState(GameState_LoadingLevel);
}

//=====================================================================================================================
// Private
//=====================================================================================================================

void BaseGameLogic::RegisterAllDelegates()
{
    IEventMgr::Get()->VAddListener(MakeDelegate(this, &BaseGameLogic::CollideableTileCreatedDelegate), EventData_Collideable_Tile_Created::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(this, &BaseGameLogic::RequestDestroyActorDelegate), EventData_Destroy_Actor::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(this, &BaseGameLogic::CreateStaticGeometryDelegate), EventData_Add_Static_Geometry::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(this, &BaseGameLogic::ItemPickedUpDelegate), EventData_Item_Picked_Up::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(this, &BaseGameLogic::FinishedLevelDelegate), EventData_Finished_Level::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(this, &BaseGameLogic::MoveActorDelegate), EventData_Move_Actor::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(this, &BaseGameLogic::ActorEnteredBossAreaDelegate), EventData_Entered_Boss_Area::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(this, &BaseGameLogic::BossFightStartedDelegate), EventData_Boss_Fight_Started::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(this, &BaseGameLogic::IngameMenuEndLifeDelegate), EventData_IngameMenu_End_Life::sk_EventType);
    IEventMgr::Get()->VAddListener(MakeDelegate(this, &BaseGameLogic::WorldFinishedLoadingDelegate), EventData_World_Finished_Loading::sk_EventType);
}

void BaseGameLogic::RemoveAllDelegates()
{
    IEventMgr::Get()->VRemoveListener(MakeDelegate(this, &BaseGameLogic::CollideableTileCreatedDelegate), EventData_Collideable_Tile_Created::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(this, &BaseGameLogic::RequestDestroyActorDelegate), EventData_Destroy_Actor::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(this, &BaseGameLogic::CreateStaticGeometryDelegate), EventData_Add_Static_Geometry::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(this, &BaseGameLogic::ItemPickedUpDelegate), EventData_Item_Picked_Up::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(this, &BaseGameLogic::FinishedLevelDelegate), EventData_Finished_Level::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(this, &BaseGameLogic::MoveActorDelegate), EventData_Move_Actor::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(this, &BaseGameLogic::ActorEnteredBossAreaDelegate), EventData_Entered_Boss_Area::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(this, &BaseGameLogic::BossFightStartedDelegate), EventData_Boss_Fight_Started::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(this, &BaseGameLogic::IngameMenuEndLifeDelegate), EventData_IngameMenu_End_Life::sk_EventType);
    IEventMgr::Get()->VRemoveListener(MakeDelegate(this, &BaseGameLogic::WorldFinishedLoadingDelegate), EventData_World_Finished_Loading::sk_EventType);
}

void BaseGameLogic::ExecuteStartupCommands(const std::string& startupCommandsFile)
{
    std::ifstream commandsFile(startupCommandsFile.c_str());
    if (commandsFile.is_open())
    {
        std::string line;
        while (std::getline(commandsFile, line))
        {
            CommandHandler::HandleCommand(line.c_str(), (void*)g_pApp->GetHumanView()->GetConsole().get());
        }
    }
}
