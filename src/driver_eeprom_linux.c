#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>

#include "driver_eeprom_linux.h"

static uint8_t *eeprom = 0;

void pw_eeprom_raw_init() {

    FILE *fh = fopen("./eeprom.bin", "rb");
    if(!fh) {
        printf("Error: cannot open eeprom file for reading.\n");
        return;
    }

    eeprom = malloc(EEPROM_SIZE);
    if(!eeprom) {
        fclose(fh);
        return;
    }

    fread(eeprom, 1, EEPROM_SIZE, fh);

    fclose(fh);

}

void pw_eeprom_raw_read(uint16_t addr, uint8_t *buf, size_t len) {

    memcpy(buf, eeprom+addr, len);;
}

void pw_eeprom_raw_write(uint16_t addr, uint8_t *buf, size_t len) {

    memcpy(eeprom+addr, buf, len);;
}

void pw_eeprom_raw_set_area(uint16_t addr, uint8_t v, size_t len) {
    memset(eeprom+addr, v, len);
}

void pw_eeprom_raw_deinit() {

    FILE *fh = fopen("./eeprom.bin", "wb");
    if(!fh) {
        printf("Error: cannot open eeprom file for reading.\n");
        return;
    }

    printf("Writing eeprom back to disk\n");

    fwrite(eeprom, 1, EEPROM_SIZE, fh);

    fclose(fh);
    free(eeprom);
    eeprom = 0;
}
