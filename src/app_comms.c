#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <stdio.h>

#include "driver_ir.h"
#include "pw_ir.h"
#include "app_comms.h"

static comm_state_t g_comm_state = COMM_STATE_IDLE;
static uint8_t g_comm_substate = 0;
static size_t g_advertising_counts = 0;

static uint8_t app_comms_buf[PW_TX_BUF_LEN];

void pw_comms_init() {

    g_comm_state = COMM_STATE_IDLE;
    g_comm_substate = 0;
    g_advertising_counts = 0;
    pw_ir_set_connect_status(CONNECT_STATUS_AWAITING);

}

void pw_comms_event_loop() {

    connect_status_t cs = pw_ir_get_connect_status();
    if(cs == CONNECT_STATUS_DISCONNECTED) return;

    ir_err_t err = IR_ERR_GENERAL;
    size_t n_read;

    switch(cs) {
        case CONNECT_STATUS_AWAITING: {
            break;
        }
        case CONNECT_STATUS_SLAVE:
            printf("We are slave!\n");
            // TODO: need to use ir buffer
            // TODO: need to allow non-fixed read lengths
            // TODO: need to return read length
            err = pw_ir_recv_packet(app_comms_buf, PW_TX_BUF_LEN, &n_read);
            pw_comms_slave_perform_action(app_comms_buf, PW_TX_BUF_LEN);
            break;
        case CONNECT_STATUS_MASTER:
            printf("We are master!\n");
            // only thing we can do is ask for peer play

            pw_ir_set_connect_status(CONNECT_STATUS_DISCONNECTED);
            break;
        case CONNECT_STATUS_DISCONNECTED:
            break;
    }

    if(err != IR_OK) {
        pw_ir_set_connect_status(CONNECT_STATUS_DISCONNECTED);
        return;
    }

}

ir_err_t pw_ir_advertise_and_listen(uint8_t *rx, size_t *n_read) {
    uint8_t advertising_buf[] = {CMD_ADVERTISING^0xaa};

    int n = pw_ir_write(advertising_buf, 1);
    if(n <= 0) {
        return IR_ERR_BAD_SEND;
    }

    g_advertising_counts++;
    if(g_advertising_counts > MAX_ADVERTISING_PACKETS) {
        return IR_ERR_ADVERTISING_MAX;
    }

    ir_err_t err = pw_ir_recv_packet(rx, 8, n_read);

    return err;
}

ir_err_t pw_comms_try_connect_loop() {
    ir_err_t err;
    size_t n_read = 0;

    switch(g_comm_substate) {
        case 1: {   // substate listen and advertise
            err = pw_ir_advertise_and_listen(app_comms_buf, &n_read);
            switch(err) {
                case IR_ERR_SIZE_MISMATCH:
                case IR_OK:
                    // we got a valid packet back, now check if master or slave
                    // set substate to master/slave check
                    g_comm_substate = 2;
                    printf("Recv packet during advertisement: first byte: %02x; size: %lu\n", app_comms_buf[0], n_read);
                    break;
                case IR_ERR_TIMEOUT:
                    // TODO: ignore
                    break;
                case IR_ERR_ADVERTISING_MAX:
                    // TODO: cannot connect
                    break;
                default:
                    // TODO: Handle error messages and quitting
                    pw_ir_set_connect_status(CONNECT_STATUS_DISCONNECTED);
                    break;
            }

        }
        case 2: {   // have peer, determine master/slave
            switch(app_comms_buf[0]) {
                case CMD_ADVERTISING: // we found peer, we request master
                    // send CMD_ASSERT_MASTER
                    g_comm_substate = 3;
                    break;
                case CMD_ASSERT_MASTER: // peer found us, peer requests master
                    // generate sessid
                    // send CMD_SLAVE_ACK
                    // set state to CONNECT_STATUS_SLAVE
                    break;
                default:
                    return IR_ERR_UNEXPECTED_PACKET;
            }

            break;
        }
        case 3: {   // we have sent master
            // expect CMD_SLAVE_ACK
            // combine keys
            // set state to CONNECT_STATUS_MASTER
            break;
        }

    }
    return IR_OK;
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

