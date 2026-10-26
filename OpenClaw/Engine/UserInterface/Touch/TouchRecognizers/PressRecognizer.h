#ifndef __PRESSRECOGNIZER_H__
#define __PRESSRECOGNIZER_H__

#include <map>
#include "AbstractRecognizer.h"

class PressRecognizer : public AbstractRecognizer {
public:
    PressRecognizer(int id, int zIndex) : AbstractRecognizer(id, zIndex),
                                          m_ThresholdDistance(0),
                                          m_ThresholdTime(0) {};

    PressRecognizer(int id, int zIndex,
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
        DOWN_READY,
        DOWN,
        UP_READY,
        UP
    }; };

    RecognizerState::Type RecognizerStateByEventState(EventState::Type eventState);

    std::map<SDL_FingerID, std::pair<Touch_PressEvent, EventState::Type> > m_Events;

    // Collect new fingers and expect thresholdDistance won't be reached during thresholdMs
    std::map<SDL_FingerID, SDL_TouchFingerEvent> m_Candidates;
    float m_ThresholdDistance;
    int m_ThresholdTime;
};


#endif
