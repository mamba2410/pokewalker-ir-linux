#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "special_things.h"
#include "pw_ir.h"
#include "pw_ir_actions.h"


/*
 *  Shell code taken from Dmitry GR
 */
const uint8_t rom_dump_exploit_upload_to_0xF956[] = {
	0x56,                       //upload address low byte
	0x5E, 0x00, 0xBA, 0x42,     //jsr     common_prologue
	0x19, 0x55,                 //sub.w   r5, r5
//lbl_big_loop:
	0x79, 0x06, 0xf8, 0xd6,     //mov.w   0xf8d6, r6
	0xfc, 0x80,                 //mov.b   0x80, r4l
	0x7b, 0x5c, 0x59, 0x8f,     //eemov.b
	0x79, 0x00, 0xaa, 0x80,     //mov.w   #0xaa80, r0
	0x5e, 0x00, 0x07, 0x72,     //jsr     sendPacket
	0x5E, 0x00, 0x25, 0x9E,     //jsr     wdt_pet
	0x79, 0x25, 0xc0, 0x00,     //cmp.w   r5, #0xc000
	0x46, 0xe4,                 //bne     $-0x1c		//lbl_big_loop
	0x79, 0x00, 0x08, 0xd6,     //mov.w   #&irAppMainLoop, r0
	0x5e, 0x00, 0x69, 0x3a,     //jsr     setProcToCallByMainInLoop
	0x5a, 0x00, 0xba, 0x62      //jmp     common_epilogue
};
const size_t shellcode_size = sizeof(rom_dump_exploit_upload_to_0xF956);

const uint8_t trigger_uploaded_code_upload_to_0xF7E0[] = {
	0xe0,                       //upload address low byte
	0xf9, 0x56                  //pointer to our code to run
};
const size_t triggercode_size = sizeof(trigger_uploaded_code_upload_to_0xF7E0);


void print_packet(uint8_t *buf, size_t len) {
    size_t i;
    for(i = 0; i < 8; i++) {
        printf("%02x", buf[i]);
    }
    printf(" ");
    for(; i < len; i++) {
        printf("%02x", buf[i]);
    }
    printf("\n");
}


uint8_t* load_shellcode(const char* fname, size_t *len) {
    FILE *f = fopen(fname, "r");
    if(f == 0) return 0;

    struct stat s;
    if( stat(fname, &s) == -1 ) return 0;
    *len = s.st_size;
    uint8_t *buf = malloc(*len);

    fread(buf, *len, 1, f);

    fclose(f);
    return buf;
}


int send_shellcode(uint8_t *shellcode, size_t len) {
    ir_err_t err;
    uint8_t *buf = malloc(len+8);
    size_t n_read = 0;

    memcpy(buf+8+1, shellcode, len);

    buf[0] = 0x06;
    buf[1] = 0xf9;
    buf[8] = 0x56;
    print_packet(buf, len+8+1);
    err = pw_ir_send_packet(buf, len+8+1, &n_read);
    if(err != IR_OK) {
    printf("Error uploading shellcode: %s\n", PW_IR_ERR_NAMES[err]);
        print_packet(buf, n_read);
        return -1;
    }
    printf("Sent shellcode\n");

    pw_ir_delay_ms(1);

    err = pw_ir_recv_packet(buf, 8, &n_read);
    if(err != IR_OK) {
        printf("Error in shellcode ack: %s\n", PW_IR_ERR_NAMES[err]);
        print_packet(buf, n_read);
        return -1;
    }
    printf("Recv shellcode ack\n");

    pw_ir_delay_ms(1);
    memcpy(buf+8, trigger_uploaded_code_upload_to_0xF7E0, triggercode_size);

    buf[0] = 0x06;
    buf[1] = 0xf7;
    err = pw_ir_send_packet(buf, triggercode_size+8, &n_read);
    if(err != IR_OK) printf("Error uploading shellcode: %s\n", PW_IR_ERR_NAMES[err]);
    printf("Sent trigger code\n");


    err = pw_ir_recv_packet(buf, 8, &n_read);
    if(err != IR_OK) printf("Error in triggercode ack: %s\n", PW_IR_ERR_NAMES[err]);
    printf("Recv trigger ack\n");

    return 0;
}


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


    return IR_OK;
}


ir_err_t send_event_command(enum gift_type gift, bool stamps) {

    uint8_t *packet = malloc(8+128);
    ir_err_t err;

    size_t n_written;

    switch(gift) {
        case GIFT_POKEMON: packet[0] = CMD_EVENT_POKEMON; break;
        case GIFT_ITEM: packet[0] = CMD_EVENT_ITEM; break;
        default: return IR_ERR_GENERAL;
    }

    if(stamps) {
        // gift all stamps are same commands +0x10
        packet[0] |= 0x10;
    }

    packet[1] = EXTRA_BYTE_TO_WALKER;
    pw_ir_send_packet(packet, 8, &n_written);

    pw_ir_delay_ms(1);
    err = pw_ir_recv_packet(packet, 8, &n_written);

    return err;
}


ir_err_t dump_64k_rom(const char* fname) {
    ir_err_t err;
    uint8_t rom_buf[0x88];
    size_t n_read = 0;

    FILE *fp = fopen(fname, "wb");
    if(!fp) {
        printf("Error: can't open file\n");
        return IR_ERR_GENERAL;
    }

    uint8_t packet[11];

    //size_t step = 56; // 64-byte serial buffer :(
    size_t step = 128;
    size_t read_size = 64*1024;

    for(size_t i = 0; i < read_size; i+=step) {
        printf("Reading address: %04lx\n", i);
        uint8_t read_len = ((read_size-i)>step)?step:read_size-i;

        // TODO: Ask for eeprom read
        packet[0] = CMD_EEPROM_READ_REQ;
        packet[1] = EXTRA_BYTE_TO_WALKER;
        packet[8] = (uint8_t)(i>>8);
        packet[9] = (uint8_t)(i&0xff);
        packet[10] = (uint8_t)read_len;

        err = pw_ir_send_packet(packet, 11, &n_read);
        if(err != IR_OK) break;

        // hacky way to get around 64-byte rx buffer
        err = pw_ir_recv_packet(rom_buf, read_len+8, &n_read);

        // Responses are VALID packets
        if(err != IR_OK) break;

        fwrite(rom_buf+8, 1, read_len, fp);
    }

    if(err == IR_OK) {
        printf("Dump success!\n");
    } else {
        printf("Dump failed\n");
        printf("\tError code: %02x: %s\n", err, PW_IR_ERR_NAMES[err]);
    }

    fclose(fp);
    return err;

}


ir_err_t dump_48k_rom(const char* fname) {
    ir_err_t err;
    uint8_t *buf = malloc(shellcode_size+8);
    size_t n_read = 0;

    memcpy(buf+8, rom_dump_exploit_upload_to_0xF956, shellcode_size);

    buf[0] = 0x06;
    buf[1] = 0xf9;
    err = pw_ir_send_packet(buf, shellcode_size+8, &n_read);
    if(err != IR_OK) printf("Error uploading shellcode: %s\n", PW_IR_ERR_NAMES[err]);
    printf("Sent shellcode\n");

    pw_ir_delay_ms(1);

    err = pw_ir_recv_packet(buf, 8, &n_read);
    if(err != IR_OK) printf("Error in shellcode ack: %s\n", PW_IR_ERR_NAMES[err]);
    printf("Recv shellcode ack\n");

    pw_ir_delay_ms(1);
    memcpy(buf+8, trigger_uploaded_code_upload_to_0xF7E0, triggercode_size);

    buf[0] = 0x06;
    buf[1] = 0xf7;
    err = pw_ir_send_packet(buf, triggercode_size+8, &n_read);
    if(err != IR_OK) printf("Error uploading shellcode: %s\n", PW_IR_ERR_NAMES[err]);
    printf("Sent trigger code\n");


    err = pw_ir_recv_packet(buf, 8, &n_read);
    if(err != IR_OK) printf("Error in triggercode ack: %s\n", PW_IR_ERR_NAMES[err]);
    printf("Recv trigger ack\n");

    FILE *fp = fopen(fname, "wb");
    if(!fp) {
        printf("Error: can't open file\n");
        return IR_ERR_GENERAL;
    }

    uint8_t rom_buf[0x88];

    size_t step = 0x80;
    size_t read_size = 48*1024;
    for(size_t i = 0; i < read_size; i+=step) {
        printf("Reading address: %04lx\n", i);

        // hacky way to get around 64-byte rx buffer
        err = pw_ir_recv_packet(rom_buf, 8, &n_read);
        (void)pw_ir_recv_packet(rom_buf+8, 0x40, &n_read);
        (void)pw_ir_recv_packet(rom_buf+0x48, 0x40, &n_read);

        // if you can read that many in one go
        //err = pw_ir_recv_packet(rom_buf, 0x88);

        if(err != IR_OK && err != IR_ERR_BAD_CHECKSUM) break;
        fwrite(rom_buf+8, 1, step, fp);
    }

    if(err == IR_OK || err == IR_ERR_BAD_CHECKSUM) {
        printf("Dump success!\n");
    } else {
        printf("Dump failed\n");
        printf("\tError code: %02x: %s\n", err, PW_IR_ERR_NAMES[err]);
    }

    fclose(fp);
    return IR_OK;
}


