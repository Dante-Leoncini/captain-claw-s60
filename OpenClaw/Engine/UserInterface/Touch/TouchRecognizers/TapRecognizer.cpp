#include <cassert>
#include "TapRecognizer.h"

Touch_Event TapRecognizer::VGetEvent(SDL_FingerID finger) {
    std::map<SDL_FingerID, std::pair<Touch_TapEvent, EventState::Type> >::iterator it = m_Events.find(finger);
    assert (it != m_Events.end());
    it->second.second = EventState::UP;
    return Touch_Event(GetId(), TAP, it->second.first);
}

RecognizerState::Type TapRecognizer::VGetState(SDL_FingerID finger) {
    std::map<SDL_FingerID, std::pair<Touch_TapEvent, EventState::Type> >::iterator it = m_Events.find(finger);
    if (it != m_Events.end()) {
        return RecognizerStateByEventState(it->second.second);
    }
    return RecognizerState::Failed;
}

void TapRecognizer::VFingerDetached(SDL_FingerID finger) {
    m_Events.erase(finger);
}

RecognizerState::Type TapRecognizer::VOnFingerDown(const SDL_TouchFingerEvent &evt) {
    m_Events[evt.fingerId] = std::make_pair(Touch_TapEvent(evt), EventState::DOWN);
    return RecognizerStateByEventState(EventState::DOWN);
}

RecognizerState::Type TapRecognizer::VOnFingerMotion(const SDL_TouchFingerEvent &evt) {
    std::map<SDL_FingerID, std::pair<Touch_TapEvent, EventState::Type> >::iterator it = m_Events.find(evt.fingerId);
    if (it != m_Events.end()) {
        return RecognizerStateByEventState(it->second.second);
    }
    return RecognizerState::Failed;
}

RecognizerState::Type TapRecognizer::VOnFingerUp(const SDL_TouchFingerEvent &evt) {
    std::map<SDL_FingerID, std::pair<Touch_TapEvent, EventState::Type> >::iterator it = m_Events.find(evt.fingerId);
    if (it != m_Events.end()) {
        it->second.second = EventState::UP_READY;
        return RecognizerStateByEventState(EventState::UP_READY);
    }
    return RecognizerState::Failed;
}

RecognizerState::Type TapRecognizer::RecognizerStateByEventState(TapRecognizer::EventState::Type eventState) {
    switch (eventState) {
        case EventState::DOWN:
            return RecognizerState::Recognizing;
        case EventState::UP_READY:
            return RecognizerState::EventReady;
        case EventState::UP:
            return RecognizerState::Done;
        default:
            assert (false && "Unknown state !");
            return RecognizerState::Failed;
    }
}
