#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "special_things.h"
#include "pw_ir.h"
#include "pw_ir_actions.h"

/*
 *  Send event data. Both a pokemon and an item.
 *  Event data starts on a non 128-byte boundary so clobbers 0x44 bytes before
 *  Ends on a 128-byte boundary so nothing clobbered aferwards.
 *  On the whole, write 0xba00-0xbf00, 1280 bytes
 *
 */
ir_err_t send_event_data(
        pokemon_summary_t *ps,
        special_pokemon_info_t *sd,
        uint8_t *sprite,
        uint8_t *pokemon_name,
        uint16_t le_item,
        uint8_t *item_name
        ) {
    //uint8_t *packet = malloc(8+sizeof(pokemon_summary_t)+sizeof(special_pokemon_info_t));
    uint8_t *buf = malloc(1280);
    uint8_t *packet = malloc(8+128);
    ir_err_t err;
    size_t cursor = 0;

    /*
     *  Fill buffer
     */
    // 0x44 bytes of zeros
    for(cursor = 0; cursor < 0x44; cursor++) {
        buf[cursor] = 0;
    }

    memcpy(buf+cursor, ps, sizeof(pokemon_summary_t));
    cursor += sizeof(pokemon_summary_t);

    memcpy(buf+cursor, sd, sizeof(special_pokemon_info_t));
    cursor += sizeof(special_pokemon_info_t);

    memcpy(buf+cursor, sprite, 384);
    cursor += 384;

    memcpy(buf+cursor, pokemon_name, 320);
    cursor += 320;

    for(size_t i = 0; i < 6; i++)
        buf[cursor+i] = 0;
    cursor += 6;

    memcpy(buf+cursor, &le_item, 2);
    cursor += 2;

    memcpy(buf+cursor, item_name, 384);
    cursor += 384;

    cursor += 56;   // unused bytes

    printf("Final buf size: %lu (should be 1280)\n", cursor);
    cursor = 0;

    // pokemon_sumary_t -> PW_EEPROM_ADDR_EVENT_POKEMON_BASIC_DATA
    // special_pokemon_into_t -> PW_EEPROM_ADDR_EVENT_POKEMON_EXTRA_DATA
    // (large) sprite -> PW_EEPROM_ADDR_IMG_EVENT_POKEMON_SMALL_ANIMATED
    // (large) name -> PW_EEPROM_ADDR_TEXT_EVENT_POKEMON_NAME

    size_t size = 1280, written=0, write_size=128, max_len=128+8;
    uint8_t counter = 0;

    do {
        written = counter*write_size;
//ir_err_t pw_action_send_large_raw_data_from_pointer(uint8_t *src, uint16_t dst, size_t final_write_size,
//        size_t write_size, uint8_t *pcounter, uint8_t *packet, size_t max_len);
        err = pw_action_send_large_raw_data_from_pointer(buf, 0xba00, size, write_size, &counter, packet, max_len);
    } while(written < size && err == IR_OK);

    if(err != IR_OK) {
        return err;
    }

    size_t n_written;
    packet[0] = CMD_EVENT_POKEMON_STAMPS;
    packet[1] = EXTRA_BYTE_TO_WALKER;
    pw_ir_send_packet(packet, 8, &n_written);

    pw_ir_delay_ms(1);
    err = pw_ir_recv_packet(packet, 8, &n_written);
    // assume ok

    packet[0] = CMD_EVENT_ITEM;
    packet[1] = EXTRA_BYTE_TO_WALKER;
    pw_ir_send_packet(packet, 8, &n_written);

    pw_ir_delay_ms(1);
    err = pw_ir_recv_packet(packet, 8, &n_written);
    // assume ok

    return IR_OK;
}



