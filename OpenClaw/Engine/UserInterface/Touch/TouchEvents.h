#ifndef __TOUCHEVENTS_H__
#define __TOUCHEVENTS_H__

#include <SDL2/SDL_events.h>

#define SDL_UserTouchEvent (SDL_USEREVENT + 1)

enum Touch_EventType {
    TAP,
    JOYSTICK_MOVE,
    JOYSTICK_RESET,
    PRESS_START,
    PRESS_END,
    SWIPE_START,
    SWIPE_END
};

struct Touch_TapEvent {
    SDL_FingerID fingerId;
    float x;            /**< Normalized in the range 0...1 */
    float y;            /**< Normalized in the range 0...1 */

    Touch_TapEvent() {}

    Touch_TapEvent(const SDL_TouchFingerEvent &tfinger) : fingerId(tfinger.fingerId),
                                                          x(tfinger.x),
                                                          y(tfinger.y) {}
};

struct Touch_JoystickEvent {
    SDL_FingerID fingerId;
    float x;            /**< Normalized in the range 0...1 */
    float y;            /**< Normalized in the range 0...1 */
    float dx;           /**< Normalized in the range -1...1 */
    float dy;           /**< Normalized in the range -1...1 */

    Touch_JoystickEvent() {}

    Touch_JoystickEvent(const SDL_TouchFingerEvent &end) : fingerId(end.fingerId),
                                                           x(0),
                                                           y(0),
                                                           dx(0),
                                                           dy(0) {}

    Touch_JoystickEvent(const SDL_TouchFingerEvent &start,
                        const SDL_TouchFingerEvent &end) : fingerId(start.fingerId),
                                                           x(start.x),
                                                           y(start.y),
                                                           dx(end.x - start.x),
                                                           dy(end.y - start.y) {}
};

struct Touch_PressEvent {
    SDL_FingerID fingerId;
    float x;            /**< Normalized in the range 0...1 */
    float y;            /**< Normalized in the range 0...1 */

    Touch_PressEvent() {}

    Touch_PressEvent(const SDL_TouchFingerEvent &tfinger) : fingerId(tfinger.fingerId),
                                                            x(tfinger.x),
                                                            y(tfinger.y) {}
};

struct Touch_SwipeEvent {
    SDL_FingerID fingerId;
    float x;            /**< Normalized in the range 0...1 */
    float y;            /**< Normalized in the range 0...1 */
    // dx > 0 -> right
    double dx;          /**< Normalized in the range -1...1 */
    // dy > 0 -> down
    double dy;          /**< Normalized in the range -1...1 */

    Touch_SwipeEvent() {}

    Touch_SwipeEvent(const SDL_TouchFingerEvent &tfinger,
                     double dx = 0, double dy = 0) : fingerId(tfinger.fingerId),
                                                     x(tfinger.x),
                                                     y(tfinger.y),
                                                     dx(dx),
                                                     dy(dy) {}
};

struct Touch_Event {
    // sdlEvent.user.data1 is a pointer to Touch_Event.
    SDL_Event sdlEvent;
    int recognizerId;
    struct {
        Touch_TapEvent tap;
        Touch_JoystickEvent joystick;
        Touch_PressEvent press;
        Touch_SwipeEvent swipe;
    } customData;

    Touch_Event() {
        Init();
    }

    Touch_Event(int recognizerId, Touch_EventType type, const Touch_TapEvent &tap) {
        Init();
        customData.tap = tap;
        sdlEvent.user.code = type;
        this->recognizerId = recognizerId;
    }

    Touch_Event(int recognizerId, Touch_EventType type, const Touch_JoystickEvent &joystick) {
        Init();
        customData.joystick = joystick;
        sdlEvent.user.code = type;
        this->recognizerId = recognizerId;
    }

    Touch_Event(int recognizerId, Touch_EventType type, const Touch_PressEvent &press) {
        Init();
        customData.press = press;
        sdlEvent.user.code = type;
        this->recognizerId = recognizerId;
    }

    Touch_Event(int recognizerId, Touch_EventType type, const Touch_SwipeEvent &swipe) {
        Init();
        customData.swipe = swipe;
        sdlEvent.user.code = type;
        this->recognizerId = recognizerId;
    }

private:
    void Init() {
        sdlEvent.type = SDL_UserTouchEvent;
        sdlEvent.user.code = 0;
        sdlEvent.user.data1 = this;
        recognizerId = 0;
    }
};

#endif
