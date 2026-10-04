#ifndef KESHOS_INPUT_H
#define KESHOS_INPUT_H

#include <stdint.h>

#define INPUT_QUEUE_SIZE 128
typedef enum { INPUT_EVENT_KEY = 1, INPUT_EVENT_POINTER = 2 } input_event_type_t;
typedef struct {
    input_event_type_t type;
    uint8_t pressed;
    uint8_t code;
    uint8_t modifiers;
    int32_t x;
    int32_t y;
    uint8_t buttons;
} input_event_t;

void input_init(void);
int input_push(const input_event_t *event);
int input_poll(input_event_t *event);

#endif
