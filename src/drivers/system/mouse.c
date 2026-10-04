// мышка ps/2, двигаем курсор
#include <stdint.h>
#include <stdbool.h>
#include "pic.h"
#include "input.h"

#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_CMD_PORT     0x64

int mouse_x = 0;
int mouse_y = 0;
int mouse_left_clicked = 0;
int mouse_right_clicked = 0;

static inline void io_wait(void) {
    outb(0x80, 0x00);
}

typedef struct {
    int32_t x;
    int32_t y;
    uint32_t max_x;
    uint32_t max_y;
    bool left_button;
    bool right_button;
    bool middle_button;
} mouse_state_t;

static mouse_state_t g_mouse = {0};
static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[3];

static void mouse_irq_handler(void); 

static void mouse_irq_handler(void); 

static inline int32_t apply_mouse_accel(int32_t delta) {
    if (delta == 0) return 0;
    int32_t sign = (delta < 0) ? -1 : 1;
    int32_t mag = (delta < 0) ? -delta : delta;

    if (mag <= 2) {
        return delta;
    } else if (mag <= 7) {
        return sign * (mag + (mag >> 1));
    } else if (mag <= 16) {
        return sign * (mag * 2);
    } else {
        int32_t res = mag * 5 / 2;
        if (res > 160) res = 160;
        return sign * res;
    }
}

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;

    if (type == 0) {

        while (timeout--) {
            if (inb(PS2_STATUS_PORT) & 0x01)
                return;
            io_wait();
        }
    } else {

        while (timeout--) {
            if (!(inb(PS2_STATUS_PORT) & 0x02))
                return;
            io_wait();
        }
    }
}

static void mouse_write(uint8_t write_byte) {
    mouse_wait(1);
    outb(PS2_CMD_PORT, 0xD4);

    mouse_wait(1);
    outb(PS2_DATA_PORT, write_byte);
}

static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(PS2_DATA_PORT);
}

void mouse_set_bounds(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0)
        return;

    g_mouse.max_x = width;
    g_mouse.max_y = height;

    if (g_mouse.x < 0)
        g_mouse.x = 0;
    if (g_mouse.y < 0)
        g_mouse.y = 0;

    if (g_mouse.x >= (int32_t)width)
        g_mouse.x = (int32_t)width - 1;

    if (g_mouse.y >= (int32_t)height)
        g_mouse.y = (int32_t)height - 1;

    mouse_x = g_mouse.x;
    mouse_y = g_mouse.y;
}

void mouse_get_state(int32_t *x, int32_t *y,
                     bool *btn_left, bool *btn_right) {
    if (x) *x = g_mouse.x;
    if (y) *y = g_mouse.y;
    if (btn_left) *btn_left = g_mouse.left_button;
    if (btn_right) *btn_right = g_mouse.right_button;
}

void mouse_handle_byte(uint8_t data) {
    switch (mouse_cycle) {
        case 0:

            if ((data & 0x08) != 0) {
                mouse_packet[0] = data;
                mouse_cycle = 1;
            }
            break;

        case 1:
            mouse_packet[1] = data;
            mouse_cycle = 2;
            break;

        case 2:
            mouse_packet[2] = data;
            mouse_cycle = 0;

            if (mouse_packet[0] & 0xC0)
                return;

            g_mouse.left_button =
                (mouse_packet[0] & 0x01) != 0;

            g_mouse.right_button =
                (mouse_packet[0] & 0x02) != 0;

            g_mouse.middle_button =
                (mouse_packet[0] & 0x04) != 0;

            int32_t rel_x = (int32_t)(int8_t)mouse_packet[1];
            int32_t rel_y = (int32_t)(int8_t)mouse_packet[2];

            if (mouse_packet[0] & 0x10) rel_x |= 0xFFFFFF00;
            if (mouse_packet[0] & 0x20) rel_y |= 0xFFFFFF00;

            rel_x = apply_mouse_accel(rel_x);
            rel_y = apply_mouse_accel(rel_y);

            g_mouse.x += rel_x;
            g_mouse.y -= rel_y;

            if (g_mouse.x < 0)
                g_mouse.x = 0;

            if (g_mouse.y < 0)
                g_mouse.y = 0;

            if (g_mouse.max_x > 0 &&
                g_mouse.x >= (int32_t)g_mouse.max_x) {
                g_mouse.x = (int32_t)g_mouse.max_x - 1;
            }

            if (g_mouse.max_y > 0 &&
                g_mouse.y >= (int32_t)g_mouse.max_y) {
                g_mouse.y = (int32_t)g_mouse.max_y - 1;
            }

            mouse_x = g_mouse.x;
            mouse_y = g_mouse.y;
            mouse_left_clicked = g_mouse.left_button ? 1 : 0;
            mouse_right_clicked = g_mouse.right_button ? 1 : 0;

            extern void evdev_feed_mouse(int x, int y, int left, int right) __attribute__((weak));
            if (evdev_feed_mouse) evdev_feed_mouse(mouse_x, mouse_y, mouse_left_clicked, mouse_right_clicked);

            input_event_t input = {
                .type = INPUT_EVENT_POINTER,
                .pressed = 0,
                .code = 0,
                .modifiers = 0,
                .x = g_mouse.x,
                .y = g_mouse.y,
                .buttons = (g_mouse.left_button ? 1 : 0) | (g_mouse.right_button ? 2 : 0) | (g_mouse.middle_button ? 4 : 0)
            };
            (void)input_push(&input);
            break;
    }
}

void mouse_init(void) {

    mouse_wait(1);
    outb(PS2_CMD_PORT, 0xA8);

    mouse_wait(1);
    outb(PS2_CMD_PORT, 0x20);

    uint8_t status = mouse_read();

    status |= 0x02;
    status &= (uint8_t)~0x20;

    mouse_wait(1);
    outb(PS2_CMD_PORT, 0x60);

    mouse_wait(1);
    outb(PS2_DATA_PORT, status);

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF3);
    mouse_read();

    mouse_write(200);
    mouse_read();

    mouse_write(0xE8);
    mouse_read();

    mouse_write(0x03);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();

    if (g_mouse.max_x == 0)
        g_mouse.max_x = 1024;

    if (g_mouse.max_y == 0)
        g_mouse.max_y = 768;

    g_mouse.x = (int32_t)(g_mouse.max_x / 2);
    g_mouse.y = (int32_t)(g_mouse.max_y / 2);

    mouse_cycle = 0;
    mouse_left_clicked = 0;
    mouse_right_clicked = 0;

    mouse_x = g_mouse.x;
    mouse_y = g_mouse.y;

    irq_install_handler(12, mouse_irq_handler);
    pic_clear_mask(2);
    pic_clear_mask(12);
}

void init_mouse(void) {
    mouse_init();
}

static void mouse_drain_hw(void) {
    for (int i = 0; i < 64; i++) {
        uint8_t st = inb(PS2_STATUS_PORT);
        if (!(st & 0x01)) break;
        if (!(st & 0x20)) break; 
        uint8_t data = inb(PS2_DATA_PORT);
        mouse_handle_byte(data);
    }
}

static void mouse_irq_handler(void) {
    mouse_drain_hw();
}

void poll_mouse(void) {
    mouse_drain_hw();
}
