#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <stdio.h>

#include "driver_ir.h"
#include "pw_ir.h"
#include "pw_ir_actions.h"
#include "app_comms.h"

static uint8_t g_comm_substate = 0;

void pw_comms_init() {

    g_comm_substate = 1;
    g_advertising_attempts = 0;
    pw_ir_set_connect_status(CONNECT_STATUS_AWAITING);

}

void pw_comms_event_loop() {

    connect_status_t cs = pw_ir_get_connect_status();
    if(cs == CONNECT_STATUS_DISCONNECTED) return;

    ir_err_t err = IR_ERR_GENERAL;
    size_t n_read;

    switch(cs) {
        case CONNECT_STATUS_AWAITING: {
            err = pw_try_connect_loop(rx_buf, PW_TX_BUF_LEN, &g_comm_substate);
            break;
        }
        case CONNECT_STATUS_SLAVE:
            printf("We are slave!\n");
            // TODO: need to allow non-fixed read lengths
            err = pw_ir_recv_packet(rx_buf, PW_TX_BUF_LEN, &n_read);
            pw_comms_slave_perform_action(rx_buf, PW_TX_BUF_LEN);
            break;
        case CONNECT_STATUS_MASTER:
            printf("We are master!\n");
            // only thing we can do is ask for peer play

            pw_ir_set_connect_status(CONNECT_STATUS_DISCONNECTED);
            break;
        default:
            printf("Unknown state: %d\n", cs);
        case CONNECT_STATUS_DISCONNECTED:
            break;
    }

    if(err != IR_OK) {

        printf("Error connecting, disconnecting\n");
        printf("\tError code: %02x: %s\n", err, PW_IR_ERR_NAMES[err]);
        pw_ir_set_connect_status(CONNECT_STATUS_DISCONNECTED);
        return;
    }

}



int pw_comms_slave_perform_action(uint8_t *packet, size_t len) {

    switch(packet[0]) {

        default:
            return -1;
    }

    return 0;
}

int pw_comms_sequence_peer_play() {

    // send CMD_PEER_PLAY_START
    // recv CMD_PEER_PLAY_RSP
    // send master EEPROM:0x91BE to slave EEPROM:0xF400
    // send master EEPROM:0xCC00 to slave EEPROM:0xDC00
    // read slave EEPROM:0x91BE to master EEPROM:0xF400
    // read slave EEPROM:0xCC00 to master EEPROM:0xDC00
    // send CMD_PEER_PLAY_DX
    // recv CMD_PEER_PLAY_DX ?
    // write data to master EEPROM:0xF6C0
    // send CMD_PEER_PLAY_END
    // recv CMD_PEER_PLAY_END
    // display animation
    // calculate gift

    return 0;
}

