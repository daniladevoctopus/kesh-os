#ifndef KESHOS_SERIAL_H
#define KESHOS_SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_write_char(char c);
void serial_print(const char *str);
void serial_print_hex(uint64_t val);
void serial_print_dec(uint64_t val);

#endif
