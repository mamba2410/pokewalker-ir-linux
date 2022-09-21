#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "pw-ir-linux.h"
#include "pw_ir.h"
#include "driver_ir.h"

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

int main(int argc, char** argv){

    pw_ir_init();

    for(int i = 0; i < 1; i++) {
        printf("\n\n");
        ir_err_t err = pw_ir_listen_for_handshake();
        printf("Error code: %02x: %s\n", err, PW_IR_ERR_NAMES[err]);

        // If we did handshake, try dump 48k rom
        if(err == IR_OK) {
            pw_ir_clear_rx();

            uint8_t *buf = malloc(shellcode_size+8);
            memcpy(buf+8, rom_dump_exploit_upload_to_0xF956, shellcode_size);
            buf[0] = 0x06;
            buf[1] = 0xf9;

            err = pw_ir_send_packet(buf, shellcode_size+8);
            if(err != IR_OK) printf("Error uploading shellcode: %s\n", PW_IR_ERR_NAMES[err]);

            printf("Sent shellcode\n");
            usleep(1000);
            err = pw_ir_recv_packet(buf, 8);
            if(err != IR_OK) printf("Error in shellcode ack: %s\n", PW_IR_ERR_NAMES[err]);

            printf("Recv shellcode ack\n");

            memcpy(buf+8, trigger_uploaded_code_upload_to_0xF7E0, triggercode_size);
            buf[0] = 0x06;
            buf[1] = 0xf7;

            usleep(1000);
            err = pw_ir_send_packet(buf, triggercode_size+8);
            if(err != IR_OK) printf("Error uploading shellcode: %s\n", PW_IR_ERR_NAMES[err]);
            printf("Sent trigger code\n");


            err = pw_ir_recv_packet(buf, 8);
            if(err != IR_OK) printf("Error in triggercode ack: %s\n", PW_IR_ERR_NAMES[err]);
            printf("Recv trigger ack\n");

            FILE *fp = fopen("./rom.bin", "wb");
            if(!fp) {
                printf("Error: can't open file\n");
                break;
            }

            uint8_t rom_buf[0x88];

            size_t step = 0x80;
            size_t read_size = 0xc000;
            //size_t read_size = 64*1024;
            for(size_t i = 0; i < read_size; i+=step) {
                //usleep(500);
                printf("Reading address: %04x\n", i);

                // hacky way to get around 64-byte rx buffer
                err = pw_ir_recv_packet(rom_buf, 8);
                err = pw_ir_recv_packet(rom_buf+8, 0x40);
                err = pw_ir_recv_packet(rom_buf+0x48, 0x40);

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
            break;
        }
    }


    pw_ir_deinit();

	return 0;
}

int serial_test() {

    uint8_t buf[256];
    int n_bytes = pw_ir_read(buf, 256);

    printf("Read %d bytes: ", n_bytes);
    for(int i = 0; i < n_bytes; i++) {
        printf("%02x", buf[i]);
    }
    printf("\n");

    return 0;
}

