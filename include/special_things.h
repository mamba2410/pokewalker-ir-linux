#ifndef SPECIAL_THINGS_H
#define SPECIAL_THINGS_H

#include <stdint.h>
#include <stdbool.h>

#include "trainer_info.h"
#include "pw_ir.h"

enum gift_type {
    GIFT_NONE,
    GIFT_POKEMON,
    GIFT_ITEM
};

ir_err_t send_event_data(
        pokemon_summary_t *ps,
        special_pokemon_info_t *sd,
        uint8_t *sprite,
        uint8_t *pokemon_name,
        uint16_t le_item,
        uint8_t *item_name
        );
ir_err_t send_event_command(enum gift_type gift, bool stamps);

uint8_t* load_shellcode(const char* fname, size_t *len);
int send_shellcode(uint8_t *shellcode, size_t len);

ir_err_t dump_64k_rom(const char* fname);
ir_err_t dump_48k_rom(const char* fname);

void print_packet(uint8_t *buf, size_t len);


#endif /* SPECIAL_THINGS_H */
