// CONTROL.C
// Handles keyboard/joystick interfaces into a unified system.
// Copyright (C)1997 BJ Eirich

#include <map>
#include <vector>
#include <stdio.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include "control.h"
#include "wasm.h"
#include "main.h"
#include "keyboard.h"


keyb_map key_map[128];         // for recording bound keys

char keyboard_map[128];
char last_pressed;

char j;                               // use joystick or not

char b1, b2, b3, b4;                  // four button flags for GamePad
char up, down, left, right;           // stick position flags
int jx, jy;                           // joystick x / y values

char foundx, foundy;                  // found-flags for joystick read.
int cenx, ceny;                       // stick-center values
int upb, downb, leftb, rightb;        // barriers for axis determination

// --- Control masks
int kb1 = SCAN_ENTER;
int kb2 = SCAN_TAB;
int kb3 = SCAN_ESC;
int kb4 = SCAN_SPACE;
int jb1 = 1;
int jb2 = 2;
int jb3 = 3;
int jb4 = 4;              // joystick definable controls.
static std::vector<verge::InputEvent> inputEvents;

void ScreenShot();

void readbuttons();
void readjoystick();

namespace verge {
    std::map<DOMScanCode, VScanCode> scanMap;

    EM_BOOL onKeyDown(int eventType, const EmscriptenKeyboardEvent *e, void *userData) {
        inputEvents.push_back(verge::InputEvent{ verge::EventType::KeyDown, int(e->keyCode) });
        return true;
    }

    EM_BOOL onKeyUp(int eventType, const EmscriptenKeyboardEvent *e, void *userData) {
        inputEvents.push_back(verge::InputEvent{ verge::EventType::KeyUp, int(e->keyCode) });
        return true;
    }

    void registerInputEventHandlers() {
        EMSCRIPTEN_RESULT result;
        result = emscripten_set_keydown_callback(
            "body",
            nullptr,
            true,
            &onKeyDown
        );
        // TEST_RESULT(result);

        result = emscripten_set_keyup_callback(
            "body",
            nullptr,
            true,
            &onKeyUp
        );
        // TEST_RESULT(result);
    }
}

void readKeyboard() {
    for (const auto& event: inputEvents) {
        //printf("Key event scan=%i down? %i\n", event.keyCode, event.type == verge::EventType::KeyDown);
        auto xl = verge::scanMap[DOMScanCode(event.keyCode)];

        keyboard_map[xl] = event.type == verge::EventType::KeyDown;
    }
    inputEvents.clear();
}

void initcontrols(char joystk) {
    verge::registerInputEventHandlers();
    verge::scanMap[DOMScanCode::VK_UP] = SCAN_UP;
    verge::scanMap[DOMScanCode::VK_DOWN] = SCAN_DOWN;
    verge::scanMap[DOMScanCode::VK_LEFT] = SCAN_LEFT;
    verge::scanMap[DOMScanCode::VK_RIGHT] = SCAN_RIGHT;
    verge::scanMap[DOMScanCode::VK_ENTER] = SCAN_ENTER;
    verge::scanMap[DOMScanCode::VK_ESCAPE] = SCAN_ESC;
    verge::scanMap[DOMScanCode::VK_SPACE] = SCAN_SPACE;
    verge::scanMap[DOMScanCode::VK_META] = SCAN_ALT;
    verge::scanMap[DOMScanCode::VK_TAB] = SCAN_TAB;
    verge::scanMap[DOMScanCode::VK_TILDE] = SCAN_TILDE;
}

void readb() {
    if (j) {
        readbuttons();
    } else {
        b1 = 0;
        b2 = 0;
        b3 = 0;
        b4 = 0;
    }
    if (keyboard_map[kb1]) {
        b1 = 1;
    }
    if (keyboard_map[kb2]) {
        b2 = 1;
    }
    if (keyboard_map[kb3]) {
        b3 = 1;
    }
    if (keyboard_map[kb4]) {
        b4 = 1;
    }

    if ((keyboard_map[SCAN_ALT]) &&
            (keyboard_map[SCAN_X])) {
        err("Exiting: ALT-X pressed.");
    }

    if (keyboard_map[SCAN_F10]) {
        keyboard_map[SCAN_F10] = 0;
        ScreenShot();
    }
}

void readcontrols() {
    emscripten_sleep(0);
    readcontrols_noSleep();
}

void readcontrols_noSleep() {
    readKeyboard();

    int i;
    if (j) {
        readjoystick();
    } else {
        b1 = 0;
        b2 = 0;
        b3 = 0;
        b4 = 0;
        up = 0;
        down = 0;
        left = 0;
        right = 0;
    }

    if (keyboard_map[SCAN_UP]) {
        up = 1;
    }
    if (keyboard_map[SCAN_DOWN]) {
        down = 1;
    }
    if (keyboard_map[SCAN_LEFT]) {
        left = 1;
    }
    if (keyboard_map[SCAN_RIGHT]) {
        right = 1;
    }
    if (keyboard_map[kb1]) {
        b1 = 1;
    }
    if (keyboard_map[kb2]) {
        b2 = 1;
    }
    if (keyboard_map[kb3]) {
        b3 = 1;
    }
    if (keyboard_map[kb4]) {
        b4 = 1;
    }

    for (i = 0; i < 128; i++) {                    /* -- ric: 03/May/98 -- */
        key_map[i].pressed = 0;
        if (keyboard_map[i]) {
            key_map[i].pressed = 1;    // no keys are bound yet
        }
    }

    if ((keyboard_map[SCAN_ALT]) &&
            (keyboard_map[SCAN_X])) {
        err("Exiting: ALT-X pressed.");
    }

    if (keyboard_map[SCAN_F10]) {
        keyboard_map[SCAN_F10] = 0;
        ScreenShot();
    }
}

void readbuttons() {
}

void getcoordinates() {
    // Gets raw, machine dependant coordinates from the joystick.
    foundx = 0;
    foundy = 0;
}

int calibrate() {
    // assumes the stick is centered when called.

    getcoordinates();                  // read stick position
    if ((!foundx) || (!foundy)) {
        printf("Could not detect joystick. Disabling.\n");
        return 0;
    }

    cenx = jx;
    ceny = jy;
    upb = (ceny * 75) / 100;           // 25% dead zone
    leftb = (cenx * 75) / 100;
    rightb = (cenx * 125) / 100;       // 25% dead zone
    downb = (ceny * 125) / 100;
    return 1;
}

void readjoystick() {
    readbuttons();
    getcoordinates();
    up = 0;
    down = 0;
    left = 0;
    right = 0;

    if (jx < leftb) {
        left = 1;
    }
    if (jx > rightb) {
        right = 1;
    }
    if (jy < upb) {
        up = 1;
    }
    if (jy > downb) {
        down = 1;
    }
}

int keyboard_init() {
    return 1;
}

void keyboard_chain(int) {
}

void keyboard_close() {
}
