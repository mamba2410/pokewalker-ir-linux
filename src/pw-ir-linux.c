#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "pw-ir-linux.h"
#include "pw_ir.h"
#include "driver_ir.h"
#include "driver_eeprom_linux.h"
#include "app_comms.h"


ir_err_t dump_64k_rom();

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

int main(int argc, char** argv){

    pw_ir_init();
    ir_err_t err;
    comm_state_t cs;

    uint8_t buf[8];
    memset(buf, 0, 8);

    /*
    err = pw_ir_recv_packet(buf, 8);
    print_packet(buf, 8);


    buf[0] = 0xfc^0xaa;
    //pw_ir_send_packet(buf, 1);
    //err = pw_ir_write(buf, 1);

    usleep(5000);

    err = pw_ir_recv_packet(buf, 8);
    print_packet(buf, 8);
    */

    pw_comms_init();
    pw_eeprom_raw_init();

    // Run our comms loop
    do {
        pw_comms_event_loop();
    //} while( (cs=pw_ir_get_comm_state()) == COMM_STATE_AWAITING );
    } while( (cs=pw_ir_get_comm_state()) != COMM_STATE_DISCONNECTED );

    return 0;

    switch(cs) {
        case COMM_STATE_MASTER: {
            printf("Attempting dump.\n");

            // try our action
            err = dump_64k_rom();
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
            printf("Unknown state, aborting.\n");
            break;
        }
    }

    // safely shut down IR
    pw_ir_deinit();

	return 0;
}

ir_err_t dump_64k_rom() {
    ir_err_t err;
    uint8_t rom_buf[0x88];
    size_t n_read = 0;

    FILE *fp = fopen("./eeprom.bin", "wb");
    if(!fp) {
        printf("Error: can't open file\n");
        return IR_ERR_GENERAL;
    }

    uint8_t packet[11];

    size_t step = 56; // 64-byte serial buffer :()
    size_t read_size = 64*1024;

    for(size_t i = 0; i < read_size; i+=step) {
        printf("Reading address: %04x\n", i);
        uint8_t read_len = ((read_size-i)>step)?step:read_size-i;

        // TODO: Ask for eeprom read
        packet[0] = CMD_EEPROM_READ_REQ;
        packet[1] = EXTRA_BYTE_TO_WALKER;
        packet[8] = (uint8_t)(i>>8);
        packet[9] = (uint8_t)(i&0xff);
        packet[10] = (uint8_t)read_len;

        err = pw_ir_send_packet(packet, 11, &n_read);
        if(err != IR_OK) {
            printf("Error code: %02x: %s\n", err, PW_IR_ERR_NAMES[err]);
            break;
        }

        // hacky way to get around 64-byte rx buffer
        err = pw_ir_recv_packet(rom_buf, read_len+8, &n_read);

        // Responses are VALID packets
        if(err != IR_OK) {
            printf("Error code: %02x: %s\n", err, PW_IR_ERR_NAMES[err]);
            break;
        }

        fwrite(rom_buf+8, 1, read_len, fp);
    }

    printf("Dump success!\n");
    fclose(fp);
    return IR_OK;

}


ir_err_t dump_48k_rom() {
    ir_err_t err;
    uint8_t *buf = malloc(shellcode_size+8);
    size_t n_read = 0;

    memcpy(buf+8, rom_dump_exploit_upload_to_0xF956, shellcode_size);
    buf[0] = 0x06;
    buf[1] = 0xf9;

    err = pw_ir_send_packet(buf, shellcode_size+8, &n_read);
    if(err != IR_OK) printf("Error uploading shellcode: %s\n", PW_IR_ERR_NAMES[err]);

    printf("Sent shellcode\n");
    usleep(1000);
    err = pw_ir_recv_packet(buf, 8, &n_read);
    if(err != IR_OK) printf("Error in shellcode ack: %s\n", PW_IR_ERR_NAMES[err]);

    printf("Recv shellcode ack\n");

    memcpy(buf+8, trigger_uploaded_code_upload_to_0xF7E0, triggercode_size);
    buf[0] = 0x06;
    buf[1] = 0xf7;

    usleep(1000);
    err = pw_ir_send_packet(buf, triggercode_size+8, &n_read);
    if(err != IR_OK) printf("Error uploading shellcode: %s\n", PW_IR_ERR_NAMES[err]);
    printf("Sent trigger code\n");


    err = pw_ir_recv_packet(buf, 8, &n_read);
    if(err != IR_OK) printf("Error in triggercode ack: %s\n", PW_IR_ERR_NAMES[err]);
    printf("Recv trigger ack\n");

    FILE *fp = fopen("./rom.bin", "wb");
    if(!fp) {
        printf("Error: can't open file\n");
        return err;
    }

    uint8_t rom_buf[0x88];

    size_t step = 0x80;
    size_t read_size = 0xc000;
    //size_t read_size = 64*1024;
    for(size_t i = 0; i < read_size; i+=step) {
        //usleep(500);
        printf("Reading address: %04x\n", i);

        // hacky way to get around 64-byte rx buffer
        err = pw_ir_recv_packet(rom_buf, 8, &n_read);
        err = pw_ir_recv_packet(rom_buf+8, 0x40, &n_read);
        err = pw_ir_recv_packet(rom_buf+0x48, 0x40, &n_read);

        // if you can read that many in one go
        //err = pw_ir_recv_packet(rom_buf, 0x88);

        /*
        // Responses are invalid packets, do not check error
        if(err != IR_OK) {
            printf("Error code: %02x: %s\n", err, PW_IR_ERR_NAMES[err]);
            break;
        }
        */

        fwrite(rom_buf+8, 1, step, fp);
    }

    printf("Dump success!\n");
    fclose(fp);
    return IR_OK;
}


ir_err_t peer_play() {

    return IR_OK;
}
