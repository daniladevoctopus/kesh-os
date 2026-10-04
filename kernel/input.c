#include "input.h"
#include "random.h"

static input_event_t g_queue[INPUT_QUEUE_SIZE];
static volatile uint32_t g_head, g_tail;

void input_init(void) { g_head = g_tail = 0; }
int input_push(const input_event_t *event) {
    if (!event) return -1;
    uint32_t next = (g_head + 1) % INPUT_QUEUE_SIZE;
    if (next == g_tail) return -1;
    g_queue[g_head] = *event;
    random_add_entropy(event, sizeof(*event), 1);
    __asm__ volatile("" ::: "memory");
    g_head = next;
    return 0;
}
int input_poll(input_event_t *event) {
    if (!event || g_tail == g_head) return 0;
    *event = g_queue[g_tail];
    __asm__ volatile("" ::: "memory");
    g_tail = (g_tail + 1) % INPUT_QUEUE_SIZE;
    return 1;
}
