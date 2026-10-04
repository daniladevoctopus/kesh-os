#include "tty.h"
#include "process.h"
#include "fd.h"

typedef struct {
    uint8_t data[TTY_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t used;
} tty_ring_t;

typedef struct {
    int allocated;
    int owner_pid;
    tty_ring_t input;
    tty_ring_t output;
} tty_t;

static tty_t g_ttys[TTY_MAX];

typedef struct {
    uint8_t allocated, master_open, slave_open;
    int master_pid, slave_pid;
    uint32_t mode;
    volatile uint8_t lock;
    tty_ring_t input, output;
    uint8_t line[256];
    uint16_t line_used;
    int foreground_group;
} pty_t;

static pty_t g_ptys[PTY_MAX];

static void ring_reset(tty_ring_t *ring) { ring->head = ring->tail = ring->used = 0; }
static int ring_write(tty_ring_t *ring, const uint8_t *data, size_t size) {
    size_t written = 0;
    while (written < size && ring->used < TTY_BUFFER_SIZE) {
        ring->data[ring->head] = data[written++];
        ring->head = (uint16_t)((ring->head + 1) % TTY_BUFFER_SIZE);
        ring->used++;
    }
    return (int)written;
}
static int ring_read(tty_ring_t *ring, uint8_t *data, size_t size) {
    size_t read = 0;
    while (read < size && ring->used) {
        data[read++] = ring->data[ring->tail];
        ring->tail = (uint16_t)((ring->tail + 1) % TTY_BUFFER_SIZE);
        ring->used--;
    }
    return (int)read;
}
static tty_t *tty_at(int id) { return id >= 0 && id < TTY_MAX && g_ttys[id].allocated ? &g_ttys[id] : 0; }
static void pty_lock(pty_t *pty) { while (__atomic_test_and_set(&pty->lock, __ATOMIC_ACQUIRE)) __asm__ volatile("pause"); }
static void pty_unlock(pty_t *pty) { __atomic_clear(&pty->lock, __ATOMIC_RELEASE); }
static int pty_index(int handle) { int value = handle - 0x100; return value >= 0 ? value / 2 : -1; }
static int pty_is_slave(int handle) { return (handle - 0x100) & 1; }
static pty_t *pty_at(int handle) { int index = pty_index(handle); return index >= 0 && index < PTY_MAX && g_ptys[index].allocated ? &g_ptys[index] : 0; }
static int pty_allowed(const pty_t *pty, int pid, int slave) { return slave ? pty->slave_open && pty->slave_pid == pid : pty->master_open && pty->master_pid == pid; }

void tty_init(void) {
    for (int i = 0; i < TTY_MAX; ++i) {
        g_ttys[i].allocated = 0;
        g_ttys[i].owner_pid = -1;
    }
    for (int i = 0; i < PTY_MAX; ++i) g_ptys[i].allocated = 0;
}

int tty_selftest(void) {
    static const uint8_t input[] = { 'o', 'k' };
    uint8_t out[sizeof(input)];
    int id = tty_create_for_process(4242);
    if (id < 0) return -1;
    if (tty_write_input(id, input, sizeof(input)) != (int)sizeof(input)) return -2;
    if (tty_available_process(4242, id) != (int)sizeof(input)) return -3;
    if (tty_read_process(4243, id, out, sizeof(out)) != -1) return -4;
    if (tty_read_process(4242, id, out, sizeof(out)) != (int)sizeof(out)) return -5;
    if (out[0] != input[0] || out[1] != input[1]) return -6;
    if (tty_close(4243, id) != -1 || tty_close(4242, id) != 0) return -7;
    return 0;
}

int tty_create_for_process(int owner_pid) {
    if (owner_pid < 0) return -1;
    for (int i = 0; i < TTY_MAX; ++i) if (!g_ttys[i].allocated) {
        g_ttys[i].allocated = 1;
        g_ttys[i].owner_pid = owner_pid;
        ring_reset(&g_ttys[i].input);
        ring_reset(&g_ttys[i].output);
        return i;
    }
    return -1;
}

int tty_close(int owner_pid, int id) {
    tty_t *tty = tty_at(id);
    if (!tty || tty->owner_pid != owner_pid) return -1;
    tty->allocated = 0;
    tty->owner_pid = -1;
    ring_reset(&tty->input);
    ring_reset(&tty->output);
    return 0;
}

void tty_close_owner(int owner_pid) {
    for (int i = 0; i < TTY_MAX; ++i) {
        if (g_ttys[i].allocated && g_ttys[i].owner_pid == owner_pid) (void)tty_close(owner_pid, i);
    }
    for (int i = 0; i < PTY_MAX; ++i) if (g_ptys[i].allocated) {
        pty_t *pty = &g_ptys[i];
        pty_lock(pty);
        if (pty->master_open && pty->master_pid == owner_pid) pty->master_open = 0;
        if (pty->slave_open && pty->slave_pid == owner_pid) pty->slave_open = 0;
        int release = !pty->master_open && !pty->slave_open;
        pty_unlock(pty);
        if (release) pty->allocated = 0;
    }
}

int tty_write_input(int id, const uint8_t *data, size_t size) { tty_t *tty = tty_at(id); return tty && data ? ring_write(&tty->input, data, size) : -1; }
int tty_read_input(int id, uint8_t *data, size_t size) { tty_t *tty = tty_at(id); return tty && data ? ring_read(&tty->input, data, size) : -1; }
int tty_write_output(int id, const uint8_t *data, size_t size) { tty_t *tty = tty_at(id); return tty && data ? ring_write(&tty->output, data, size) : -1; }
int tty_read_output(int id, uint8_t *data, size_t size) { tty_t *tty = tty_at(id); return tty && data ? ring_read(&tty->output, data, size) : -1; }
int tty_write_process(int owner_pid, int id, const uint8_t *data, size_t size) { tty_t *tty = tty_at(id); return tty && tty->owner_pid == owner_pid && data ? ring_write(&tty->output, data, size) : -1; }
int tty_read_process(int owner_pid, int id, uint8_t *data, size_t size) { tty_t *tty = tty_at(id); return tty && tty->owner_pid == owner_pid && data ? ring_read(&tty->input, data, size) : -1; }
int tty_available_process(int owner_pid, int id) { tty_t *tty = tty_at(id); return tty && tty->owner_pid == owner_pid ? tty->input.used : -1; }

int pty_create(int owner_pid, int handles[2]) {
    if (owner_pid < 0 || !handles) return -1;
    for (int i = 0; i < PTY_MAX; ++i) if (!g_ptys[i].allocated) {
        pty_t *pty = &g_ptys[i];
        pty->allocated = pty->master_open = pty->slave_open = 1;
        pty->master_pid = pty->slave_pid = owner_pid;
        pty->mode = PTY_MODE_CANONICAL | PTY_MODE_ECHO;
        pty->lock = 0; pty->line_used = 0;
        pty->foreground_group = -1;
        ring_reset(&pty->input); ring_reset(&pty->output);
        handles[0] = 0x100 + i * 2; handles[1] = handles[0] + 1;
        return 0;
    }
    return -1;
}

int pty_attach(int owner_pid, int slave_handle, int target_pid) {
    pty_t *pty = pty_at(slave_handle);
    if (!pty || !pty_is_slave(slave_handle) || pty->master_pid != owner_pid || !process_exists(target_pid)) return -1;
    if (fd_bind_pty_stdio(target_pid, slave_handle) != 0) return -1;
    pty_lock(pty); pty->slave_pid = target_pid; pty->foreground_group = process_get_group(target_pid); pty_unlock(pty);
    return process_set_controlling_pty(target_pid, slave_handle);
}

int pty_close(int owner_pid, int handle) {
    pty_t *pty = pty_at(handle);
    int slave = pty_is_slave(handle);
    if (!pty || !pty_allowed(pty, owner_pid, slave)) return -1;
    pty_lock(pty);
    if (slave) pty->slave_open = 0; else pty->master_open = 0;
    int release = !pty->master_open && !pty->slave_open;
    pty_unlock(pty);
    if (release) pty->allocated = 0;
    return 0;
}

int pty_read(int owner_pid, int handle, uint8_t *data, size_t size) {
    pty_t *pty = pty_at(handle);
    int slave = pty_is_slave(handle);
    if (!pty || !data || !pty_allowed(pty, owner_pid, slave)) return -1;
    pty_lock(pty);
    int result = ring_read(slave ? &pty->input : &pty->output, data, size);
    pty_unlock(pty);
    return result;
}

int pty_write(int owner_pid, int handle, const uint8_t *data, size_t size) {
    pty_t *pty = pty_at(handle);
    int slave = pty_is_slave(handle);
    if (!pty || !data || !pty_allowed(pty, owner_pid, slave)) return -1;
    pty_lock(pty);
    if (slave) { int result = ring_write(&pty->output, data, size); pty_unlock(pty); return result; }
    size_t consumed = 0;
    for (; consumed < size; ++consumed) {
        uint8_t c = data[consumed];
        if (c == 3 || c == 26) {
            int group = pty->foreground_group;
            pty_unlock(pty);
            if (group >= 0) (void)process_signal_group(-1, group, c == 3 ? KESH_SIGINT : KESH_SIGSTOP);
            return (int)(consumed + 1);
        }
        if (!(pty->mode & PTY_MODE_CANONICAL)) { if (ring_write(&pty->input, &c, 1) != 1) break; if (pty->mode & PTY_MODE_ECHO) ring_write(&pty->output, &c, 1); continue; }
        if (c == 8 || c == 127) {
            if (pty->line_used) { pty->line_used--; if (pty->mode & PTY_MODE_ECHO) { static const uint8_t erase[] = {8, ' ', 8}; ring_write(&pty->output, erase, sizeof(erase)); } }
            continue;
        }
        if (c == 4) {
            if (pty->line_used) { if (TTY_BUFFER_SIZE - pty->input.used < pty->line_used) break; ring_write(&pty->input, pty->line, pty->line_used); pty->line_used = 0; }
            continue;
        }
        if (pty->line_used >= sizeof(pty->line)) break;
        pty->line[pty->line_used++] = c;
        if (pty->mode & PTY_MODE_ECHO) ring_write(&pty->output, &c, 1);
        if (c == '\n') { if (TTY_BUFFER_SIZE - pty->input.used < pty->line_used) break; ring_write(&pty->input, pty->line, pty->line_used); pty->line_used = 0; }
    }
    pty_unlock(pty);
    return (int)consumed;
}

int pty_available(int owner_pid, int handle) {
    pty_t *pty = pty_at(handle);
    int slave = pty_is_slave(handle);
    if (!pty || !pty_allowed(pty, owner_pid, slave)) return -1;
    pty_lock(pty); int result = slave ? pty->input.used : pty->output.used; pty_unlock(pty);
    return result;
}

int pty_set_mode(int owner_pid, int master_handle, uint32_t mode) {
    pty_t *pty = pty_at(master_handle);
    if (!pty || pty_is_slave(master_handle) || pty->master_pid != owner_pid || (mode & ~(PTY_MODE_CANONICAL | PTY_MODE_ECHO))) return -1;
    pty_lock(pty); pty->mode = mode; pty->line_used = 0; pty_unlock(pty);
    return 0;
}

int pty_set_foreground_group(int owner_pid, int master_handle, int process_group) {
    pty_t *pty = pty_at(master_handle);
    if (!pty || pty_is_slave(master_handle) || pty->master_pid != owner_pid || process_group < 0) return -1;
    pty_lock(pty); pty->foreground_group = process_group; pty_unlock(pty);
    return 0;
}

int pty_get_foreground_group(int owner_pid, int master_handle) {
    pty_t *pty = pty_at(master_handle);
    if (!pty || pty_is_slave(master_handle) || pty->master_pid != owner_pid) return -1;
    pty_lock(pty); int group = pty->foreground_group; pty_unlock(pty);
    return group;
}

int pty_signal_foreground(int owner_pid, int master_handle, int signal) {
    pty_t *pty = pty_at(master_handle);
    if (!pty || pty_is_slave(master_handle) || pty->master_pid != owner_pid) return -1;
    if (signal != KESH_SIGINT && signal != KESH_SIGTERM && signal != KESH_SIGKILL && signal != KESH_SIGSTOP && signal != KESH_SIGCONT) return -1;
    pty_lock(pty); int group = pty->foreground_group; pty_unlock(pty);
    return process_signal_group(-1, group, signal);
}
