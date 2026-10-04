#include "evdev.h"
#include "fd.h"
#include "timer.h"
#include "memory.h"
#include "serial.h"
#include "process.h"

#define EVDEV_RING_SIZE 128

typedef struct {
    int in_use;
    int dev_id;
    int pid;
    struct linux_input_event queue[EVDEV_RING_SIZE];
    uint32_t head;
    uint32_t tail;
} evdev_instance_t;

#define MAX_EVDEV_INSTANCES 16
static evdev_instance_t g_instances[MAX_EVDEV_INSTANCES];

static int s_last_mouse_x = -1;
static int s_last_mouse_y = -1;
static int s_last_mouse_left = 0;
static int s_last_mouse_right = 0;

void evdev_init(void) {
    for (int i = 0; i < MAX_EVDEV_INSTANCES; i++) {
        g_instances[i].in_use = 0;
        g_instances[i].head = 0;
        g_instances[i].tail = 0;
    }
}

static void evdev_push_event(evdev_instance_t *inst, uint16_t type, uint16_t code, int32_t val) {
    uint32_t next = (inst->head + 1) % EVDEV_RING_SIZE;
    if (next == inst->tail) {
        /* Drop oldest on overflow */
        inst->tail = (inst->tail + 1) % EVDEV_RING_SIZE;
    }
    struct linux_input_event *ev = &inst->queue[inst->head];
    uint64_t ms = timer_millis();
    ev->time.tv_sec = (int64_t)(ms / 1000ULL);
    ev->time.tv_usec = (int64_t)((ms % 1000ULL) * 1000ULL);
    ev->type = type;
    ev->code = code;
    ev->value = val;
    inst->head = next;
}

void evdev_feed_key(uint8_t scancode, int pressed, int extended) {
    uint16_t linux_code = 0;
    if (extended) {
        switch (scancode & 0x7F) {
            case 0x48: linux_code = 103; break; /* KEY_UP */
            case 0x4B: linux_code = 105; break; /* KEY_LEFT */
            case 0x4D: linux_code = 106; break; /* KEY_RIGHT */
            case 0x50: linux_code = 108; break; /* KEY_DOWN */
            case 0x1D: linux_code = 97;  break; /* KEY_RIGHTCTRL */
            case 0x38: linux_code = 100; break; /* KEY_RIGHTALT */
            case 0x47: linux_code = 102; break; /* KEY_HOME */
            case 0x4F: linux_code = 107; break; /* KEY_END */
            case 0x49: linux_code = 104; break; /* KEY_PAGEUP */
            case 0x51: linux_code = 109; break; /* KEY_PAGEDOWN */
            case 0x52: linux_code = 110; break; /* KEY_INSERT */
            case 0x53: linux_code = 111; break; /* KEY_DELETE */
            default:   linux_code = (uint16_t)(scancode & 0x7F); break;
        }
    } else {
        linux_code = (uint16_t)(scancode & 0x7F);
    }

    if (linux_code == 0) return;

    for (int i = 0; i < MAX_EVDEV_INSTANCES; i++) {
        if (g_instances[i].in_use && g_instances[i].dev_id == EVDEV_DEV_KBD) {
            evdev_push_event(&g_instances[i], EV_KEY, linux_code, pressed ? 1 : 0);
            evdev_push_event(&g_instances[i], EV_SYN, SYN_REPORT, 0);
        }
    }
}

void evdev_feed_mouse(int x, int y, int left, int right) {
    if (s_last_mouse_x < 0) {
        s_last_mouse_x = x;
        s_last_mouse_y = y;
        s_last_mouse_left = left;
        s_last_mouse_right = right;
        return;
    }

    int dx = x - s_last_mouse_x;
    int dy = y - s_last_mouse_y;
    int left_changed = (left != s_last_mouse_left);
    int right_changed = (right != s_last_mouse_right);

    if (dx == 0 && dy == 0 && !left_changed && !right_changed) return;

    s_last_mouse_x = x;
    s_last_mouse_y = y;
    s_last_mouse_left = left;
    s_last_mouse_right = right;

    for (int i = 0; i < MAX_EVDEV_INSTANCES; i++) {
        if (g_instances[i].in_use && g_instances[i].dev_id == EVDEV_DEV_MOUSE) {
            if (dx != 0) evdev_push_event(&g_instances[i], EV_REL, REL_X, dx);
            if (dy != 0) evdev_push_event(&g_instances[i], EV_REL, REL_Y, dy);
            if (left_changed) evdev_push_event(&g_instances[i], EV_KEY, BTN_LEFT, left ? 1 : 0);
            if (right_changed) evdev_push_event(&g_instances[i], EV_KEY, BTN_RIGHT, right ? 1 : 0);
            evdev_push_event(&g_instances[i], EV_SYN, SYN_REPORT, 0);
        }
    }
}

int evdev_open(int dev_id, int pid) {
    for (int i = 0; i < MAX_EVDEV_INSTANCES; i++) {
        if (!g_instances[i].in_use) {
            g_instances[i].in_use = 1;
            g_instances[i].dev_id = dev_id;
            g_instances[i].pid = pid;
            g_instances[i].head = 0;
            g_instances[i].tail = 0;

            if (dev_id == EVDEV_DEV_MOUSE) {
                extern int32_t mouse_x, mouse_y;
                s_last_mouse_x = mouse_x;
                s_last_mouse_y = mouse_y;
            }

            int fd = fd_create_custom(pid, FD_KIND_EVDEV, &g_instances[i], FD_OPEN_READ);
            return fd;
        }
    }
    return -1;
}

int evdev_read(void *custom_ptr, void *dst, uint32_t max_bytes) {
    if (!custom_ptr || !dst || max_bytes < sizeof(struct linux_input_event)) return -11;
    evdev_instance_t *inst = (evdev_instance_t *)custom_ptr;

    if (inst->head == inst->tail) {
        return -11; /* EAGAIN: Non-blocking, no events pending */
    }

    uint32_t count = 0;
    struct linux_input_event *out = (struct linux_input_event *)dst;
    uint32_t max_events = max_bytes / (uint32_t)sizeof(struct linux_input_event);

    while (inst->head != inst->tail && count < max_events) {
        out[count] = inst->queue[inst->tail];
        inst->tail = (inst->tail + 1) % EVDEV_RING_SIZE;
        count++;
    }

    return (int)(count * sizeof(struct linux_input_event));
}

int evdev_poll(void *custom_ptr, uint32_t events) {
    if (!custom_ptr) return 0;
    evdev_instance_t *inst = (evdev_instance_t *)custom_ptr;
    if ((events & FD_POLL_READ) && inst->head != inst->tail) {
        return FD_POLL_READ;
    }
    return 0;
}

void evdev_release(void *custom_ptr) {
    if (!custom_ptr) return;
    evdev_instance_t *inst = (evdev_instance_t *)custom_ptr;
    inst->in_use = 0;
    inst->head = 0;
    inst->tail = 0;
}

static void evdev_set_bit(uint8_t *bits, uint32_t bytes, uint32_t bit) {
    uint32_t byte = bit / 8U;
    if (byte < bytes) bits[byte] |= (uint8_t)(1U << (bit % 8U));
}

int evdev_ioctl(void *custom_ptr, uint64_t req, uint64_t arg) {
    if (!custom_ptr) return -1;
    evdev_instance_t *inst = (evdev_instance_t *)custom_ptr;
    uint32_t request = (uint32_t)req;
    uint32_t type = (request >> 8) & 0xFFU;
    uint32_t number = request & 0xFFU;
    uint32_t size = (request >> 16) & 0x3FFFU;

    if (request == EVIOCGVERSION || request == 0x80045501U) {
        if (!arg) return -1;
        uint32_t version = 0x010001;
        return copy_to_user((void *)arg, &version, sizeof(version));
    }

    if (request == EVIOCGID || request == 0x80085502U) {
        if (!arg) return -1;
        struct {
            uint16_t bustype;
            uint16_t vendor;
            uint16_t product;
            uint16_t version;
        } id = { 3, 0x4B53, (uint16_t)(inst->dev_id + 1), 1 };
        return copy_to_user((void *)arg, &id, sizeof(id));
    }

    if (type != 0x45U) return -1;

    if (number == 0x06U) {
        if (!arg || size == 0) return -1;
        const char *name = (inst->dev_id == EVDEV_DEV_KBD) ? "KeshOS Keyboard" : "KeshOS Mouse";
        size_t len = 0;
        while (name[len]) len++;
        len++;
        if (len > size) len = size;
        return copy_to_user((void *)arg, name, len);
    }

    if (number >= 0x20U && number <= 0x3FU) {
        if (!arg || size == 0) return -1;
        uint8_t bits[128];
        uint32_t bytes = size > sizeof(bits) ? sizeof(bits) : size;
        for (uint32_t i = 0; i < bytes; ++i) bits[i] = 0;
        uint32_t event_type = number - 0x20U;
        if (event_type == EV_SYN) {
            evdev_set_bit(bits, bytes, EV_SYN);
            evdev_set_bit(bits, bytes, EV_KEY);
            if (inst->dev_id == EVDEV_DEV_MOUSE) evdev_set_bit(bits, bytes, EV_REL);
        } else if (event_type == EV_KEY) {
            if (inst->dev_id == EVDEV_DEV_KBD) {
                for (uint32_t key = 1; key <= 127; ++key) evdev_set_bit(bits, bytes, key);
            } else {
                evdev_set_bit(bits, bytes, BTN_LEFT);
                evdev_set_bit(bits, bytes, BTN_RIGHT);
                evdev_set_bit(bits, bytes, BTN_MIDDLE);
            }
        } else if (event_type == EV_REL && inst->dev_id == EVDEV_DEV_MOUSE) {
            evdev_set_bit(bits, bytes, REL_X);
            evdev_set_bit(bits, bytes, REL_Y);
            evdev_set_bit(bits, bytes, REL_WHEEL);
        }
        return copy_to_user((void *)arg, bits, bytes);
    }

    if (number == 0x19U) {
        if (!arg || size == 0) return -1;
        uint8_t bits[16];
        uint32_t bytes = size > sizeof(bits) ? sizeof(bits) : size;
        for (uint32_t i = 0; i < bytes; ++i) bits[i] = 0;
        return copy_to_user((void *)arg, bits, bytes);
    }

    if (number == 0x03U || number == 0x90U) return 0;
    return -1;
}
