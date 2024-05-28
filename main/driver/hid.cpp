#include <string.h>
#include <circle/timer.h>
#include <circle/startup.h>
#include "../event.hpp"

#define HID_LEFT_CTRL       0x01
#define HID_LEFT_SHIFT      0x02
#define HID_LEFT_ALT        0x04
#define HID_LEFT_GUI        0x08
#define HID_RIGHT_CTRL      0x10
#define HID_RIGHT_SHIFT     0x20
#define HID_RIGHT_ALT       0x40
#define HID_RIGHT_GUI       0x80

static const char keycode2ascii[128][2] =  {
    {0     , 0      }, /* 0x00 */
    {0     , 0      }, /* 0x01 */
    {0     , 0      }, /* 0x02 */
    {0     , 0      }, /* 0x03 */
    {'a'   , 'A'    }, /* 0x04 */
    {'b'   , 'B'    }, /* 0x05 */
    {'c'   , 'C'    }, /* 0x06 */
    {'d'   , 'D'    }, /* 0x07 */
    {'e'   , 'E'    }, /* 0x08 */
    {'f'   , 'F'    }, /* 0x09 */
    {'g'   , 'G'    }, /* 0x0a */
    {'h'   , 'H'    }, /* 0x0b */
    {'i'   , 'I'    }, /* 0x0c */
    {'j'   , 'J'    }, /* 0x0d */
    {'k'   , 'K'    }, /* 0x0e */
    {'l'   , 'L'    }, /* 0x0f */
    {'m'   , 'M'    }, /* 0x10 */
    {'n'   , 'N'    }, /* 0x11 */
    {'o'   , 'O'    }, /* 0x12 */
    {'p'   , 'P'    }, /* 0x13 */
    {'q'   , 'Q'    }, /* 0x14 */
    {'r'   , 'R'    }, /* 0x15 */
    {'s'   , 'S'    }, /* 0x16 */
    {'t'   , 'T'    }, /* 0x17 */
    {'u'   , 'U'    }, /* 0x18 */
    {'v'   , 'V'    }, /* 0x19 */
    {'w'   , 'W'    }, /* 0x1a */
    {'x'   , 'X'    }, /* 0x1b */
    {'y'   , 'Y'    }, /* 0x1c */
    {'z'   , 'Z'    }, /* 0x1d */
    {'1'   , '!'    }, /* 0x1e */
    {'2'   , '@'    }, /* 0x1f */
    {'3'   , '#'    }, /* 0x20 */
    {'4'   , '$'    }, /* 0x21 */
    {'5'   , '%'    }, /* 0x22 */
    {'6'   , '^'    }, /* 0x23 */
    {'7'   , '&'    }, /* 0x24 */
    {'8'   , '*'    }, /* 0x25 */
    {'9'   , '('    }, /* 0x26 */
    {'0'   , ')'    }, /* 0x27 */
    {0     , 0      }, /* 0x28 */
    {0     , 0      }, /* 0x29 */
    {0     , 0      }, /* 0x2a */
    {0     , 0      }, /* 0x2b */
    {' '   , ' '    }, /* 0x2c */
    {'-'   , '_'    }, /* 0x2d */
    {'='   , '+'    }, /* 0x2e */
    {'['   , '{'    }, /* 0x2f */
    {']'   , '}'    }, /* 0x30 */
    {'\\'  , '|'    }, /* 0x31 */
    {'#'   , '~'    }, /* 0x32 */
    {';'   , ':'    }, /* 0x33 */
    {'\''  , '\"'   }, /* 0x34 */
    {'`'   , '~'    }, /* 0x35 */
    {','   , '<'    }, /* 0x36 */
    {'.'   , '>'    }, /* 0x37 */
    {'/'   , '?'    }, /* 0x38 */
                                
    {0     , 0      }, /* 0x39 */
    {0     , 0      }, /* 0x3a */
    {0     , 0      }, /* 0x3b */
    {0     , 0      }, /* 0x3c */
    {0     , 0      }, /* 0x3d */
    {0     , 0      }, /* 0x3e */
    {0     , 0      }, /* 0x3f */
    {0     , 0      }, /* 0x40 */
    {0     , 0      }, /* 0x41 */
    {0     , 0      }, /* 0x42 */
    {0     , 0      }, /* 0x43 */
    {0     , 0      }, /* 0x44 */
    {0     , 0      }, /* 0x45 */
    {0     , 0      }, /* 0x46 */
    {0     , 0      }, /* 0x47 */
    {0     , 0      }, /* 0x48 */
    {0     , 0      }, /* 0x49 */
    {0     , 0      }, /* 0x4a */
    {0     , 0      }, /* 0x4b */
    {0     , 0      }, /* 0x4c */
    {0     , 0      }, /* 0x4d */
    {0     , 0      }, /* 0x4e */
    {0     , 0      }, /* 0x4f */
    {0     , 0      }, /* 0x50 */
    {0     , 0      }, /* 0x51 */
    {0     , 0      }, /* 0x52 */
    {0     , 0      }, /* 0x53 */
                                
    {'/'   , '/'    }, /* 0x54 */
    {'*'   , '*'    }, /* 0x55 */
    {'-'   , '-'    }, /* 0x56 */
    {'+'   , '+'    }, /* 0x57 */
    {0     , 0      }, /* 0x58 */
    {'1'   , 0      }, /* 0x59 */
    {'2'   , 0      }, /* 0x5a */
    {'3'   , 0      }, /* 0x5b */
    {'4'   , 0      }, /* 0x5c */
    {'5'   , '5'    }, /* 0x5d */
    {'6'   , 0      }, /* 0x5e */
    {'7'   , 0      }, /* 0x5f */
    {'8'   , 0      }, /* 0x60 */
    {'9'   , 0      }, /* 0x61 */
    {'0'   , 0      }, /* 0x62 */
    {'.'   , 0      }, /* 0x63 */
    {0     , 0      }, /* 0x64 */
    {0     , 0      }, /* 0x65 */
    {0     , 0      }, /* 0x66 */
    {'='   , '='    }, /* 0x67 */
};

static const uint8_t cc_keymap[256] = {
    0, 0, 0, 0, 30, 48, 46, 32,                 // 00
    18, 33, 34, 35, 23, 36, 37, 38,             // 08
    50, 49, 24, 25, 16, 19, 31, 20,             // 10
    22, 47, 17, 45, 21, 44, 2, 3,               // 18
    4, 5, 6, 7, 8, 9, 10, 11,                   // 20
    28, 1, 14, 15, 57, 12, 13, 26,              // 28
    27, 43, 0, 39, 40, 41, 51, 52,              // 30
    43, 58, 59, 60, 61, 62, 63, 64,             // 38
    65, 66, 67, 68, 87, 88, 196, 70,            // 40
    197, 198, 199, 201, 211, 207, 209, 205,     // 48
    203, 208, 200, 69, 181, 55, 74, 78,         // 50
    156, 79, 80, 81, 75, 76, 77, 71,            // 58
    72, 73, 82, 83, 43, 0, 0, 0,                // 60
    0, 0, 0, 0, 0, 0, 0, 0,                     // 68
    0, 0, 0, 0, 0, 0, 0, 0,                     // 70
    0, 0, 0, 0, 0, 0, 0, 0,                     // 78
    0, 0, 0, 0, 0, 179, 141, 0,                 // 80
    112, 125, 121, 123, 0, 0, 0, 0,             // 88
    0, 0, 0, 0, 0, 0, 0, 0,                     // 90
    0, 0, 0, 0, 0, 0, 0, 0,                     // 98
    0, 0, 0, 0, 0, 0, 0, 0,                     // A0
    0, 0, 0, 0, 0, 0, 0, 0,                     // A8
    0, 0, 0, 0, 0, 0, 0, 0,                     // B0
    0, 0, 0, 0, 0, 0, 0, 0,                     // B8
    0, 0, 0, 0, 0, 0, 0, 0,                     // C0
    0, 0, 0, 0, 0, 0, 0, 0,                     // C8
    0, 0, 0, 0, 0, 0, 0, 0,                     // D0
    0, 0, 0, 0, 0, 0, 0, 0,                     // D8
    29, 42, 56, 91, 157, 54, 184, 92,           // E0
    0, 0, 0, 0, 0, 0, 0, 0,                     // E8
    0, 0, 0, 0, 0, 0, 0, 0,                     // F0
    0, 0, 0, 0, 0, 0, 0, 0,                     // F8
};
static const uint8_t cc_modifiers[8] = {29, 42, 56, 91, 157, 54, 184, 92};
static const char *hid_proto_name_str[] = {
    "NONE",
    "KEYBOARD",
    "MOUSE"
};

static uint32_t keypress_time[256] = {0};
static unsigned char prev_report_keys[6];
static unsigned char prev_report_modifier;
static bool shift_held = false, ctrl_held = false, caps_lock = false;

static void key_cb(uint8_t key, bool isHeld) {
    event_t event;
    event.type = EVENT_TYPE_KEY;
    event.key.keycode = key;
    event.key.repeat = isHeld;
    event_push(&event);
}

static void keyUp_cb(uint8_t key) {
    event_t event;
    event.type = EVENT_TYPE_KEY_UP;
    event.key.keycode = key;
    event_push(&event);
}

static void char_cb(char c) {
    event_t event;
    event.type = EVENT_TYPE_CHAR;
    event.character.c = c;
    event_push(&event);
}

static void repeat_timer(TKernelTimerHandle tmr, void* arg1, void* arg2) {
    uint32_t t = CTimer::Get()->GetTicks() * (1000 / HZ);
    if (ctrl_held) {
        if (keypress_time[0x15] && t - keypress_time[0x15] > 600) {
            reboot();
        }
        if (keypress_time[0x17] && t - keypress_time[0x17] > 600) {
            event_t event;
            event.type = EVENT_TYPE_TERMINATE;
            event_push(&event);
            keypress_time[0x17] = 0;
        }
    }
    for (int key = 0; key < 256; key++) {
        if (keypress_time[key] && cc_keymap[key] && t - keypress_time[key] > 600) {
            key_cb(cc_keymap[key], true);
            if (keycode2ascii[key][0])
                char_cb(keycode2ascii[key][(shift_held != caps_lock) ? 1 : 0]);
        }
    }
    CTimer::Get()->StartKernelTimer(MSEC2HZ(100), repeat_timer);
}

// look up new key in previous keys
static inline bool find_key_in_report(const unsigned char RawKeys[6], uint8_t keycode) {
    for (uint8_t i=0; i<6; i++) {
        if (RawKeys[i] == keycode) return true;
    }

    return false;
}

static void hid_host_keyboard_report_callback(unsigned char modifier, const unsigned char RawKeys[6]) {
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t key = RawKeys[i];
        if (key && !keypress_time[key]) {
            if (cc_keymap[key]) {
                key_cb(cc_keymap[key], false);
                if (keycode2ascii[key][0])
                    char_cb(keycode2ascii[key][((modifier & (HID_LEFT_SHIFT | HID_RIGHT_SHIFT)) != caps_lock) ? 1 : 0]);
            }
            if (key == 0x39) { // caps lock
                caps_lock = !caps_lock;
                uint8_t rp = caps_lock ? 2 : 0;
                //hid_class_request_set_report(hid_device_handle, HID_REPORT_TYPE_OUTPUT, HID_REPORT_TYPE_OUTPUT, &rp, 1);
            }
            keypress_time[key] = CTimer::Get()->GetTicks() * (1000 / HZ);
        }
    }

    uint8_t mods = modifier ^ prev_report_modifier;
    for (uint8_t i = 0; i < 8; i++) {
        if (mods & (1 << i)) {
            if (modifier & (1 << i)) key_cb(cc_modifiers[i], false);
            else keyUp_cb(cc_modifiers[i]);
        }
    }
    shift_held = (modifier & HID_LEFT_SHIFT) || (modifier & HID_RIGHT_SHIFT);
    ctrl_held = (modifier & HID_LEFT_CTRL) || (modifier & HID_RIGHT_CTRL);

    for (uint8_t i = 0; i < 6; i++) {
        uint8_t key = prev_report_keys[i];
        if (key && !find_key_in_report(RawKeys, key)) {
            if (cc_keymap[key]) keyUp_cb(cc_keymap[key]);
            keypress_time[key] = 0;
        }
    }

    memcpy(prev_report_keys, RawKeys, 6);
    prev_report_modifier = modifier;
}

static bool setTimer = false, setHandler = false;

static void device_removed(CDevice *dev, void *ud) {
    setHandler = false;
}

void hid_init(void) {
    if (setHandler) return;
    CUSBKeyboardDevice * kbd = (CUSBKeyboardDevice*)CDeviceNameService::Get()->GetDevice("ukbd1", false);
    if (kbd == NULL) return;
    //kbd->Initialize();
    kbd->RegisterKeyStatusHandlerRaw(hid_host_keyboard_report_callback, false);
    kbd->RegisterRemovedHandler(device_removed);
    setHandler = true;
    if (setTimer) return;
    CTimer::Get()->StartKernelTimer(MSEC2HZ(100), repeat_timer);
    setTimer = true;
}
