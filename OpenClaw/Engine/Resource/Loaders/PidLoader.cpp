#include "PidLoader.h"

#include "../../Graphics2D/Image.h"
#include "../../GameApp/BaseGameApp.h"
#include "ResourceCorrection.h"

#include <map>

// Images still in use, by path and palette: an image the resource cache has dropped while some
// actor still shows it is reused instead of being loaded (and uploaded to the GPU) again
typedef std::map<std::pair<std::string, WapPal*>, weak_ptr<Image> > LiveImageMap;
static LiveImageMap s_LiveImages;

//=================================================================================================
// class PidResourceExtraData
//
//     This class implements the IResourceExtraData
//

PidResourceExtraData::~PidResourceExtraData()
{
    if (_pid != NULL)
    {
        WAP_PidDestroy(_pid);
    }
}

void PidResourceExtraData::LoadPid(char* rawBuffer, uint32 size, WapPal* palette, const char* resourceString)
{
    if (_pid == NULL)
    {
        _pid = WAP_PidLoadFromData(rawBuffer, size, palette);
        OnPidLoaded(resourceString, _pid);
    }
}

void PidResourceExtraData::LoadImage(char* rawBuffer, uint32 size, WapPal* palette, const char* resourceString)
{
    if (_pid == NULL)
    {
        LoadPid(rawBuffer, size, palette, resourceString);
    }
    if (_image == NULL)
    {
        SDL_Renderer* renderer = g_pApp->GetRenderer();
        _image = shared_ptr<Image>(Image::CreateImage(_pid, renderer));
        WAP_PidDestroy(_pid); _pid = NULL;
        //SAFE_DELETE_ARRAY(rawBuffer);
    }
}

//=================================================================================================
// class PidResourceLoader
//
//     This class implements the IResourceLoader interface with PID file loading
//

WapPid* PidResourceLoader::LoadAndReturnPid(const char* resourceString, WapPal* palette)
{
    Resource resource(resourceString);

    shared_ptr<ResourceHandle> handle = g_pApp->GetResourceCache()->GetHandle(&resource);
    shared_ptr<PidResourceExtraData> extraData = std::static_pointer_cast<PidResourceExtraData>(handle->GetExtraData());

    if (!extraData)
    {
        extraData = shared_ptr<PidResourceExtraData>(new PidResourceExtraData());
        extraData->LoadPid(handle->GetDataBuffer(), handle->GetSize(), palette, resourceString);

        if (extraData->GetPid() == NULL)
        {
            LOG_ERROR(extraData->VToString() + ": GetPid() returned NULL. Check if PidResourceLoader is registered.");
            return NULL;
        }

        handle->SetExtraData(extraData);
    }

    return extraData->GetPid();
}

shared_ptr<Image> PidResourceLoader::LoadAndReturnImage(const char* resourceString, WapPal* palette)
{
    const std::pair<std::string, WapPal*> liveKey(resourceString, palette);
    LiveImageMap::iterator liveIt = s_LiveImages.find(liveKey);
    if (liveIt != s_LiveImages.end())
    {
        if (shared_ptr<Image> pLiveImage = liveIt->second.lock())
        {
            return pLiveImage;
        }
    }

    Resource resource(resourceString);

    shared_ptr<ResourceHandle> handle = g_pApp->GetResourceCache()->GetHandle(&resource);
    shared_ptr<PidResourceExtraData> extraData = std::static_pointer_cast<PidResourceExtraData>(handle->GetExtraData());

    if (!extraData)
    {
        extraData = shared_ptr<PidResourceExtraData>(new PidResourceExtraData());
        extraData->LoadImage(handle->GetDataBuffer(), handle->GetSize(), palette, resourceString);

        if (!extraData->GetImage())
        {
            LOG_ERROR(extraData->VToString() + ": GetImage() returned nullptr. Check if PidResourceLoader is registered.");
            return NULL;
        }

        handle->SetExtraData(extraData);
        handle->ReleaseBuffer();
    }
    // Extra data could be created within LoadAndReturnPid but not the image
    else if (!extraData->GetImage())
    {
        extraData->LoadImage(handle->GetDataBuffer(), handle->GetSize(), palette, resourceString);

        if (!extraData->GetImage())
        {
            LOG_ERROR(extraData->VToString() + ": GetImage() returned nullptr. Check if PidResourceLoader is registered.");
            return NULL;
        }
    }

    shared_ptr<Image> pImage = extraData->GetImage();
    if (s_LiveImages.size() > 4096)
    {
        for (LiveImageMap::iterator it = s_LiveImages.begin(); it != s_LiveImages.end(); )
        {
            if (it->second.expired())
                s_LiveImages.erase(it++);
            else
                ++it;
        }
    }
    s_LiveImages[liveKey] = pImage;
    return pImage;
}

shared_ptr<PidResourceLoader> PidResourceLoader::Create()
{
    return shared_ptr<PidResourceLoader>(new PidResourceLoader());
};