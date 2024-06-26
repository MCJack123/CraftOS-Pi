#ifndef TERMINAL_H
#define TERMINAL_H
#include "../common.hpp"

extern int TERM_WIDTH, TERM_HEIGHT;
#define NO_CURSOR -1

extern const uint32_t defaultPalette[16];
extern uint32_t palette[16];

extern void terminal_task(void);
extern void terminal_init(int width, int height, int stride);
extern void terminal_deinit(void);
extern void terminal_clear(int line, uint8_t colors);
extern void terminal_scroll(int lines, uint8_t colors);
extern void terminal_write(int x, int y, const uint8_t* text, int len, uint8_t colors);
#define terminal_write_string(x, y, text, colors) terminal_write(x, y, (const uint8_t*)(text), strlen(text), colors)
#define terminal_write_literal(x, y, text, colors) terminal_write(x, y, (const uint8_t*)text, sizeof(text)-1, colors)
extern void terminal_blit(int x, int y, const uint8_t* text, const uint8_t* colors, int len);
extern void terminal_cursor(int8_t color, int x, int y);
extern void terminal_setPalette(uint8_t color, uint8_t r, uint8_t g, uint8_t b);

#endif