#ifndef ACTORFACTORY_H_
#define ACTORFACTORY_H_

#include <map>

#include "ActorComponent.h"

//-------------------------------------------------------------------------------------------------
// Actor factory
//-------------------------------------------------------------------------------------------------

class ActorFactory
{
public:
    ActorFactory();

    // With OPENCLAW_FRAME_PROFILE: logs the time spent per component kind since the last call
    static void LogCreationProfile();

    StrongActorPtr CreateActor(TiXmlElement* pActorRoot, TiXmlElement* overrides);
    StrongActorPtr CreateActor(const char* actorResource, TiXmlElement* overrides);
    void ModifyActor(StrongActorPtr actor, TiXmlElement* overrides);

    virtual StrongActorComponentPtr VCreateComponent(TiXmlElement* data);

protected:
    GenericObjectFactory<ActorComponent, uint32_t> _componentFactory;

private:
    uint32_t _lastActorGUID;
    uint32_t GetNextActorGUID() { ++_lastActorGUID; return _lastActorGUID; }
};

#endif