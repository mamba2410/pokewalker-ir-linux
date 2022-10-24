#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "pw-ir-linux.h"
#include "pw_ir.h"
#include "driver_ir.h"
#include "driver_eeprom_linux.h"
#include "app_comms.h"
#include "pw_ir_actions.h"
#include "eeprom_map.h"

void run_comms_loop();
void dump_rom(uint64_t which, const char* fname);

ir_err_t dump_64k_rom(const char* fname);
ir_err_t dump_48k_rom(const char* fname);
void print_packet(uint8_t *buf, size_t len);

/*
 *  Shell code taken from Dmitry GR
 */
static const uint8_t rom_dump_exploit_upload_to_0xF956[] = {
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

static const uint8_t trigger_uploaded_code_upload_to_0xF7E0[] = {
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

void run_comms_loop() {
    comm_state_t cs;

    // Run our comms loop
    do {
        pw_comms_event_loop();
    } while( (cs=pw_ir_get_comm_state()) != COMM_STATE_DISCONNECTED );
}

void dump_rom(uint64_t which, const char* fname) {
    comm_state_t cs = COMM_STATE_DISCONNECTED;
    ir_err_t err;

    do {
        pw_comms_event_loop();
    } while( (cs=pw_ir_get_comm_state()) == COMM_STATE_AWAITING );

    switch(cs) {
        case COMM_STATE_MASTER: {
            printf("Attempting dump.\n");

            // try our action
            switch(which) {
                case 48: err = dump_48k_rom(fname); break;
                case 64: err = dump_64k_rom(fname); break;
                default: {
                    printf("Valid options 64 or 48, not %lu\n", which);
                    err = IR_ERR_GENERAL;
                    break;
                }
            }

            if(err != IR_OK)
                printf("Error code: %02x: %s\n", err, PW_IR_ERR_NAMES[err]);

            break;
        }
        case COMM_STATE_SLAVE: {
            printf("We ended up as slave, aborting.\n");
            break;
        }
        case COMM_STATE_DISCONNECTED: {
            printf("Cannot connect to walker.\n");
            break;
        }
        default: {
            printf("Unknown state %d, aborting.\n", cs);
            break;
        }
    }
}


int main(int argc, char** argv){

    pw_ir_init();
    pw_eeprom_raw_init();
    pw_comms_init();

    //dump_rom(48, "./flashrom.bin");
    //dump_rom(64, "./eeprom.bin");
    run_comms_loop();

    // safely shut down IR
    pw_ir_deinit();

	return 0;
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

    size_t step = 56; // 64-byte serial buffer :(
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

        if(err != IR_OK) break;
        fwrite(rom_buf+8, 1, step, fp);
    }

    if(err == IR_OK) {
        printf("Dump success!\n");
    } else {
        printf("Dump failed\n");
        printf("\tError code: %02x: %s\n", err, PW_IR_ERR_NAMES[err]);
    }

    fclose(fp);
    return IR_OK;
}


ir_err_t peer_play() {

    return IR_OK;
}
