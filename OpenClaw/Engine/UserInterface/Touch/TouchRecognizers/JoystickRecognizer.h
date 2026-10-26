#ifndef __JOYSTICKRECOGNIZER_H__
#define __JOYSTICKRECOGNIZER_H__

#include <map>
#include "AbstractRecognizer.h"

class JoystickRecognizer : public AbstractRecognizer {
public:
    JoystickRecognizer(int id, int zIndex,
                       float thresholdDistance, int thresholdTime) : AbstractRecognizer(id, zIndex),
                                                                     m_ThresholdDistance(thresholdDistance),
                                                                     m_ThresholdTime(thresholdTime) {};

    Touch_Event VGetEvent(SDL_FingerID finger) override;
    RecognizerState::Type VGetState(SDL_FingerID finger) override;
    void VFingerDetached(SDL_FingerID finger) override;

    void VOnUpdate() override;

protected:
    RecognizerState::Type VOnFingerDown(const SDL_TouchFingerEvent &evt) override;
    RecognizerState::Type VOnFingerMotion(const SDL_TouchFingerEvent &evt) override;
    RecognizerState::Type VOnFingerUp(const SDL_TouchFingerEvent &evt) override;

private:
    struct EventState { enum Type {
        MOVE_READY,
        MOVED,
        RESET_READY,
        RESETED
    }; };

    RecognizerState::Type RecognizerStateByEventState(EventState::Type eventState);

    struct EventData {
        SDL_TouchFingerEvent firstEvent;
        SDL_TouchFingerEvent lastEvent;

        EventData() {}
        EventData(const SDL_TouchFingerEvent &firstEvent,
                  const SDL_TouchFingerEvent &lastEvent) : firstEvent(firstEvent),
                                                           lastEvent(lastEvent) {}
    };

    std::map<SDL_FingerID, std::pair<EventData, EventState::Type> > m_Events;

    // Collect new fingers and wait thresholdDistance during thresholdMs
    std::map<SDL_FingerID, SDL_TouchFingerEvent> m_Candidates;
    float m_ThresholdDistance;
    int m_ThresholdTime;
};


#endif
