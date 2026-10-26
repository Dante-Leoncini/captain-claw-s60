#ifndef __TAPRECOGNIZER_H__
#define __TAPRECOGNIZER_H__

#include <map>
#include "AbstractRecognizer.h"

class TapRecognizer : public AbstractRecognizer {
public:
    TapRecognizer(int id, int zIndex) : AbstractRecognizer(id, zIndex) {};

    Touch_Event VGetEvent(SDL_FingerID finger) override;
    RecognizerState::Type VGetState(SDL_FingerID finger) override;
    void VFingerDetached(SDL_FingerID finger) override;

protected:
    RecognizerState::Type VOnFingerDown(const SDL_TouchFingerEvent &evt) override;
    RecognizerState::Type VOnFingerMotion(const SDL_TouchFingerEvent &evt) override;
    RecognizerState::Type VOnFingerUp(const SDL_TouchFingerEvent &evt) override;

private:
    struct EventState { enum Type {
        DOWN,
        UP_READY,
        UP
    }; };

    RecognizerState::Type RecognizerStateByEventState(EventState::Type eventState);

    std::map<SDL_FingerID, std::pair<Touch_TapEvent, EventState::Type> > m_Events;
};


#endif
