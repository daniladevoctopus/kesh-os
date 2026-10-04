#ifndef KESHOS_EVDEV_H
#define KESHOS_EVDEV_H

#include <stdint.h>
#include <stddef.h>

#define EVDEV_DEV_KBD   0
#define EVDEV_DEV_MOUSE 1
#define EVDEV_MAX_DEVS  2

/* Linux Input Event Types */
#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03

/* Synchronization Events */
#define SYN_REPORT 0

/* Mouse Relative Axes */
#define REL_X 0x00
#define REL_Y 0x01
#define REL_WHEEL 0x08

/* Mouse Buttons */
#define BTN_MOUSE   0x110
#define BTN_LEFT    0x110
#define BTN_RIGHT   0x111
#define BTN_MIDDLE  0x112

struct linux_timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};

struct linux_input_event {
    struct linux_timeval time;
    uint16_t type;
    uint16_t code;
    int32_t  value;
};

/* ioctls */
#define EVIOCGVERSION 0x80044501U
#define EVIOCGID      0x80084502U

void evdev_init(void);
int evdev_open(int dev_id, int pid);
int evdev_read(void *custom_ptr, void *dst, uint32_t max_bytes);
int evdev_poll(void *custom_ptr, uint32_t events);
void evdev_release(void *custom_ptr);
int evdev_ioctl(void *custom_ptr, uint64_t req, uint64_t arg);

/* Feed functions from kernel drivers */
void evdev_feed_key(uint8_t scancode, int pressed, int extended);
void evdev_feed_mouse(int x, int y, int left, int right);

#endif /* KESHOS_EVDEV_H */
