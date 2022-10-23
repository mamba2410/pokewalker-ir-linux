#ifndef DRIVER_EEPROM_LINUX_H
#define DRIVER_EEPROM_LINUX_H

#include <stdint.h>
#include <stddef.h>

#define EEPROM_SIZE (64*1024)
#define EEPROM_WRITE_ALIGN  1   // write sizes must be aligned to this many bytes
                                // and address must be aligned to nearest multiple

void pw_eeprom_raw_init();
void pw_eeprom_raw_read(uint16_t addr, uint8_t *buf, size_t len);
void pw_eeprom_raw_write(uint16_t addr, uint8_t *buf, size_t len);


#endif /* DRIVER_EEPROM_LINUX_H */
