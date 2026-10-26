#include "ResourceMgr.h"
#include "ResourceCache.h"

ResourceMgrImpl::~ResourceMgrImpl()
{

}

void ResourceMgrImpl::VAddResourceCache(std::shared_ptr<ResourceCache> &pCache)
{
    m_ResourceCacheList.push_back(pCache);
}

std::shared_ptr<ResourceCache> ResourceMgrImpl::VGetResourceCacheFromName(const std::string& resCacheName)
{
    assert(m_ResourceCacheList.size() > 0);

    for (size_t pResCacheIdx = 0; pResCacheIdx < m_ResourceCacheList.size(); pResCacheIdx++)
    {
        std::shared_ptr<ResourceCache>& pResCache = m_ResourceCacheList[pResCacheIdx];
        if (pResCache->GetName() == resCacheName)
        {
            return pResCache;
        }
    }

    return NULL;
}

std::shared_ptr<ResourceHandle> ResourceMgrImpl::VGetHandle(Resource* r, const std::string& resCacheName)
{
    assert(!m_ResourceCacheList.empty());



    // Find res cache by name
    if (!resCacheName.empty())
    {
        std::shared_ptr<ResourceCache> pResCache = VGetResourceCacheFromName(resCacheName);
        assert(pResCache != NULL);

        return pResCache->GetHandle(r);
    }
    else // Find in all available res caches
    {
        for (size_t pResCacheIdx = 0; pResCacheIdx < m_ResourceCacheList.size(); pResCacheIdx++)
        {
            std::shared_ptr<ResourceCache>& pResCache = m_ResourceCacheList[pResCacheIdx];
            if (std::shared_ptr<ResourceHandle> pHandle = pResCache->GetHandle(r))
            {
                return pHandle;
            }
        }
    }

    return nullptr;
}

int32 ResourceMgrImpl::VPreload(const std::string pattern, void(*progressCallback)(int32, bool &), const std::string& resCacheName)
{
    assert(!m_ResourceCacheList.empty());

    int32 totalLoaded = 0;

    // Preload res cache by name
    if (!resCacheName.empty())
    {
        std::shared_ptr<ResourceCache> pResCache = VGetResourceCacheFromName(resCacheName);
        assert(pResCache != NULL);

        return pResCache->Preload(pattern, progressCallback);
    }
    else // Preload all available res caches
    {
        for (size_t pResCacheIdx = 0; pResCacheIdx < m_ResourceCacheList.size(); pResCacheIdx++)
        {
            std::shared_ptr<ResourceCache>& pResCache = m_ResourceCacheList[pResCacheIdx];
            totalLoaded += pResCache->Preload(pattern, progressCallback);
        }
    }

    return totalLoaded;
}

std::vector<std::string> ResourceMgrImpl::VMatch(const std::string pattern, const std::string& resCacheName)
{
    assert(!m_ResourceCacheList.empty());

    std::vector<std::string> matchedStrings;

    if (!resCacheName.empty())
    {
        std::shared_ptr<ResourceCache> pResCache = VGetResourceCacheFromName(resCacheName);
        assert(pResCache != NULL);

        matchedStrings = pResCache->Match(pattern);
    }
    else
    {
        for (size_t pResCacheIdx = 0; pResCacheIdx < m_ResourceCacheList.size(); pResCacheIdx++)
        {
            std::shared_ptr<ResourceCache>& pResCache = m_ResourceCacheList[pResCacheIdx];
            std::vector<std::string> matched = pResCache->Match(pattern);
            matchedStrings.insert(matchedStrings.end(), matched.begin(), matched.end());
        }
    }

    return matchedStrings;
}

std::vector<std::string> ResourceMgrImpl::VGetAllFilesInDirectory(const char* directoryPath, const std::string& resCacheName)
{
    assert(!m_ResourceCacheList.empty());

    std::vector<std::string> allFiles;

    if (!resCacheName.empty())
    {
        std::shared_ptr<ResourceCache> pResCache = VGetResourceCacheFromName(resCacheName);
        assert(pResCache != NULL);

        allFiles = pResCache->GetAllFilesInDirectory(directoryPath);
    }
    else
    {
        for (size_t pResCacheIdx = 0; pResCacheIdx < m_ResourceCacheList.size(); pResCacheIdx++)
        {
            std::shared_ptr<ResourceCache>& pResCache = m_ResourceCacheList[pResCacheIdx];
            std::vector<std::string> files = pResCache->GetAllFilesInDirectory(directoryPath);
            allFiles.insert(allFiles.end(), files.begin(), files.end());
        }
    }

    return allFiles;
}

void ResourceMgrImpl::VFlush(const std::string& resCacheName)
{
    assert(!m_ResourceCacheList.empty());

    std::vector<std::string> allFiles;

    if (!resCacheName.empty())
    {
        std::shared_ptr<ResourceCache> pResCache = VGetResourceCacheFromName(resCacheName);
        assert(pResCache != NULL);

        pResCache->Flush();
    }
    else
    {
        for (size_t pResCacheIdx = 0; pResCacheIdx < m_ResourceCacheList.size(); pResCacheIdx++)
        {
            std::shared_ptr<ResourceCache>& pResCache = m_ResourceCacheList[pResCacheIdx];
            pResCache->Flush();
        }
    }
}

bool ResourceMgrImpl::VHasResourceCache(const std::string& resCacheName)
{
    bool bHasResCache = false;
    for (size_t pResCacheIdx = 0; pResCacheIdx < m_ResourceCacheList.size(); pResCacheIdx++)
    {
        std::shared_ptr<ResourceCache>& pResCache = m_ResourceCacheList[pResCacheIdx];
        if (pResCache->GetName() == resCacheName)
        {
            return true;
        }
    }

    return bHasResCache;
}