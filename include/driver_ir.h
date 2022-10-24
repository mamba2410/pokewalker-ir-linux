#ifndef DRIVER_IR_H
#define DRIVER_IR_H

#include <stdint.h>
#include <stddef.h>

// Uncomment to enable printing reads/writes to stdout
//#define DRIVER_IR_DEBUG_WRITE
//#define DRIVER_IR_DEBUG_READ

int pw_ir_read(uint8_t *buf, size_t max_len);
int pw_ir_read_raw(uint8_t *buf, size_t max_len);
int pw_ir_write(uint8_t *buf, size_t len);
int pw_ir_clear_rx();
void pw_ir_flush_rx();
int pw_ir_init();
void pw_ir_deinit();

#endif /* DRIVER_IR_H */
