// маппинг клавиш
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define KBD_MOD_SHIFT 0x01
#define KBD_MOD_CTRL  0x02
#define KBD_MOD_ALT   0x04
#define KBD_MOD_CAPS  0x08

typedef struct {
    uint8_t scancode;   
    uint8_t extended;   
    uint8_t pressed;    
    uint8_t mods;       
} kbd_event_t;

void init_keyboard(void);

int keyboard_poll_event(kbd_event_t *ev);

char keyboard_event_to_char(const kbd_event_t *ev);

char keyboard_getchar(void);

int keyboard_consume_alt_f4(void);

#endif
