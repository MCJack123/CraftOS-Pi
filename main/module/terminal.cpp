#include <string.h>
#include <circle/time.h>
#include <circle/timer.h>
#include <circle/sched/semaphore.h>
#include "terminal.hpp"
#include "font.h"

int TERM_WIDTH = 53, TERM_HEIGHT = 22, SCREEN_SIZE = TERM_WIDTH * TERM_HEIGHT;
#define SCREEN_WIDTH TERM_WIDTH
#define SCREEN_HEIGHT TERM_HEIGHT

static const char blinkTimerID = 'b';
static uint8_t * screen;
static uint8_t * colors;
static TKernelTimerHandle timer;
static int cursorX = 0, cursorY = 0;
static bool cursorOn = false;
static int8_t cursorColor = -1;
static bool changed = true;
static int term_stride = 640;

#define COLOR32R(b, g, r, a) COLOR32(r, g, b, a)

const uint32_t defaultPalette[16] = {
    COLOR32R(0xf0, 0xf0, 0xf0, 0xFF),
    COLOR32R(0xf2, 0xb2, 0x33, 0xFF),
    COLOR32R(0xe5, 0x7f, 0xd8, 0xFF),
    COLOR32R(0x99, 0xb2, 0xf2, 0xFF),
    COLOR32R(0xde, 0xde, 0x6c, 0xFF),
    COLOR32R(0x7f, 0xcc, 0x19, 0xFF),
    COLOR32R(0xf2, 0xb2, 0xcc, 0xFF),
    COLOR32R(0x4c, 0x4c, 0x4c, 0xFF),
    COLOR32R(0x99, 0x99, 0x99, 0xFF),
    COLOR32R(0x4c, 0x99, 0xb2, 0xFF),
    COLOR32R(0xb2, 0x66, 0xe5, 0xFF),
    COLOR32R(0x33, 0x66, 0xcc, 0xFF),
    COLOR32R(0x7f, 0x66, 0x4c, 0xFF),
    COLOR32R(0x57, 0xa6, 0x4e, 0xFF),
    COLOR32R(0xcc, 0x4c, 0x4c, 0xFF),
    COLOR32R(0x11, 0x11, 0x11, 0xFF)
};

uint32_t palette[16];

static uint8_t blinkCount = 0, taskCounter = 4;
void terminal_task() {
    //CKernel::kernel->LogI("terminal", "Rendering screen to %p\n", CKernel::kernel->framebuffer);
    if (++taskCounter < 5) return;
    taskCounter = 0;
    if (cursorColor >= 0 && ++blinkCount == 8) {
        blinkCount = 0;
        cursorOn = !cursorOn;
        CKernel::kernel->SetLED(cursorOn);
        changed = true;
    }
    if (changed) {
        changed = false;
        for (int y = 0; y < SCREEN_HEIGHT * 18; y++) {
            uint8_t* line = CKernel::kernel->framebuffer + (y * term_stride);
            for (int x = 0; x < SCREEN_WIDTH * 12; x+=2) {
                const int cp = (y / 18) * SCREEN_WIDTH + (x / 12);
                const uint8_t c = screen[cp];
                line[x] = line[x+1] = font_data[((c >> 4) * 9 + ((y % 18) >> 1)) * 96 + ((c & 0xF) * 6 + ((x >> 1) % 6))] ? colors[cp] & 0x0F : colors[cp] >> 4;
            }
            int cx = cursorX;
            if (cursorOn && y >> 1 == cursorY * 9 + 7 && cx >= 0 && cx < TERM_WIDTH) memset(line + cx * 12, cursorColor, 12);
        }
    }
    //timer = CTimer::Get()->StartKernelTimer(MSEC2HZ(50), terminal_task);
}

void terminal_init(int width, int height, int stride) {
    TERM_WIDTH = width;
    TERM_HEIGHT = height;
    SCREEN_SIZE = width * height;
    term_stride = stride;
    screen = new uint8_t[SCREEN_SIZE];
    colors = new uint8_t[SCREEN_SIZE];
    memcpy(palette, defaultPalette, sizeof(defaultPalette));
    CKernel::kernel->WritePalette(palette);
    CTimer::Get()->RegisterPeriodicHandler(terminal_task);
    CKernel::kernel->RegisterExitFunction(terminal_deinit);
    terminal_clear(-1, 0xF0);
}

void terminal_deinit(void) {
    CTimer::Get()->CancelKernelTimer(timer);
}

void terminal_clear(int line, uint8_t col) {
    if (line < 0) {
        memset(screen, ' ', SCREEN_SIZE);
        memset(colors, col, SCREEN_SIZE);
    } else {
        memset(screen + line*SCREEN_WIDTH, ' ', SCREEN_WIDTH);
        memset(colors + line*SCREEN_WIDTH, col, SCREEN_WIDTH);
    }
    changed = true;
}

void terminal_scroll(int lines, uint8_t col) {
    if (lines > 0 ? (unsigned)lines >= SCREEN_HEIGHT : (unsigned)-lines >= SCREEN_HEIGHT) {
        // scrolling more than the height is equivalent to clearing the screen
        memset(screen, ' ', SCREEN_HEIGHT * SCREEN_WIDTH);
        memset(colors, col, SCREEN_HEIGHT * SCREEN_WIDTH);
    } else if (lines > 0) {
        memmove(screen, screen + lines * SCREEN_WIDTH, (SCREEN_HEIGHT - lines) * SCREEN_WIDTH);
        memset(screen + (SCREEN_HEIGHT - lines) * SCREEN_WIDTH, ' ', lines * SCREEN_WIDTH);
        memmove(colors, colors + lines * SCREEN_WIDTH, (SCREEN_HEIGHT - lines) * SCREEN_WIDTH);
        memset(colors + (SCREEN_HEIGHT - lines) * SCREEN_WIDTH, col, lines * SCREEN_WIDTH);
    } else if (lines < 0) {
        memmove(screen - lines * SCREEN_WIDTH, screen, (SCREEN_HEIGHT + lines) * SCREEN_WIDTH);
        memset(screen, ' ', -lines * SCREEN_WIDTH);
        memmove(colors - lines * SCREEN_WIDTH, colors, (SCREEN_HEIGHT + lines) * SCREEN_WIDTH);
        memset(colors, col, -lines * SCREEN_WIDTH);
    }
    changed = true;
}

void terminal_write(int x, int y, const uint8_t* text, int len, uint8_t col) {
    if (y < 0 || y >= SCREEN_HEIGHT || x >= SCREEN_WIDTH) return;
    if (x < 0) {
        text += x;
        len -= x;
        x = 0;
    }
    if (len <= 0) return;
    if (x + len > SCREEN_WIDTH) len = SCREEN_WIDTH - x;
    if (len <= 0) return;
    memcpy(screen + y*SCREEN_WIDTH + x, text, len);
    memset(colors + y*SCREEN_WIDTH + x, col, len);
    changed = true;
}

void terminal_blit(int x, int y, const uint8_t* text, const uint8_t* col, int len) {
    if (y < 0 || y >= SCREEN_HEIGHT || x >= SCREEN_WIDTH) return;
    if (x < 0) {
        text += x;
        col += x;
        len -= x;
        x = 0;
    }
    if (len <= 0) return;
    if (x + len > SCREEN_WIDTH) len = SCREEN_WIDTH - x;
    if (len <= 0) return;
    memcpy(screen + y*SCREEN_WIDTH + x, text, len);
    memcpy(colors + y*SCREEN_WIDTH + x, col, len);
    changed = true;
}

void terminal_cursor(int8_t color, int x, int y) {
    cursorColor = color;
    cursorX = x;
    cursorY = y;
    if (cursorColor < 0) cursorOn = false;
    changed = true;
}
