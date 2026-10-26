#ifndef __ABSTRACTRECOGNIZER_H__
#define __ABSTRACTRECOGNIZER_H__

#include "../TouchEvents.h"

namespace RecognizerState {
enum Type {
    DoNothing,   // Recognizer does not track this finger. Do not call VOnFinger* and VGetState for this finger anymore
    Recognizing, // Recognizer tracks this finger. Wait other inputs, Touch manager calls another recognizers

    EventReady, // Event is ready. Touch manager may call VGetEvent() if there is no another recognizers with high zIndex
    Hold, // All available events were fired. Wait other inputs. Detach another recognizers

    Failed, // Recognizer does not track this finger anymore. Do not call VOnFinger* and VGetState for this finger anymore
    Done // Event is end. Detach this finger
};
}

struct Rect {
    float x, y, w, h;

    Rect() {}
    Rect(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}
};

class AbstractRecognizer {
private:
    const int id;
    bool enabled;
    const int zIndex;

protected:
    Rect frame;

    // This group of methods should collect information about touches and change VGetState result
    virtual RecognizerState::Type VOnFingerDown(const SDL_TouchFingerEvent &evt) = 0;
    virtual RecognizerState::Type VOnFingerMotion(const SDL_TouchFingerEvent &evt) = 0;
    virtual RecognizerState::Type VOnFingerUp(const SDL_TouchFingerEvent &evt) = 0;

    bool isTouchWithinFrame(float x, float y);

public:

    AbstractRecognizer(int id, int zIndex);

    int GetId() const {
        return id;
    }

    bool IsEnabled() const {
        return enabled;
    }

    void SetEnabled(bool enabled) {
        this->enabled = enabled;
    }

    void SetFrame(const Rect &rect) {
        frame = rect;
    }

    // Call before VGetState method
    virtual void VOnUpdate() {}

    // Call only if VGetState returns EventReady.
    // Recognizer must store events until VGetEvent or VFingerDetached calls.
    virtual Touch_Event VGetEvent(SDL_FingerID finger) = 0;

    virtual RecognizerState::Type VGetState(SDL_FingerID finger) = 0;

    // Forget this finger and erase all data/events about it.
    virtual void VFingerDetached(SDL_FingerID finger) = 0;

    RecognizerState::Type OnFingerDown(const SDL_TouchFingerEvent &evt);
    RecognizerState::Type OnFingerMotion(const SDL_TouchFingerEvent &evt);
    RecognizerState::Type OnFingerUp(const SDL_TouchFingerEvent &evt);

    bool operator<(const AbstractRecognizer &b) const {
        return this->zIndex < b.zIndex;
    }

};

#endif
