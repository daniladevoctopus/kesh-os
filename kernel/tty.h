#ifndef KESHOS_TTY_H
#define KESHOS_TTY_H

#include <stddef.h>
#include <stdint.h>

#define TTY_MAX 4
#define TTY_BUFFER_SIZE 1024
#define PTY_MAX 4
#define PTY_MODE_CANONICAL 0x01U
#define PTY_MODE_ECHO      0x02U

void tty_init(void);
int tty_selftest(void);
int tty_create_for_process(int owner_pid);
int tty_close(int owner_pid, int tty_id);
void tty_close_owner(int owner_pid);
int tty_write_input(int tty_id, const uint8_t *data, size_t size);
int tty_read_input(int tty_id, uint8_t *data, size_t size);
int tty_write_output(int tty_id, const uint8_t *data, size_t size);
int tty_read_output(int tty_id, uint8_t *data, size_t size);
int tty_write_process(int owner_pid, int tty_id, const uint8_t *data, size_t size);
int tty_read_process(int owner_pid, int tty_id, uint8_t *data, size_t size);
int tty_available_process(int owner_pid, int tty_id);
int pty_create(int owner_pid, int handles[2]);
int pty_attach(int owner_pid, int slave_handle, int target_pid);
int pty_close(int owner_pid, int handle);
int pty_read(int owner_pid, int handle, uint8_t *data, size_t size);
int pty_write(int owner_pid, int handle, const uint8_t *data, size_t size);
int pty_available(int owner_pid, int handle);
int pty_set_mode(int owner_pid, int master_handle, uint32_t mode);
int pty_set_foreground_group(int owner_pid, int master_handle, int process_group);
int pty_get_foreground_group(int owner_pid, int master_handle);
int pty_signal_foreground(int owner_pid, int master_handle, int signal);

#endif
