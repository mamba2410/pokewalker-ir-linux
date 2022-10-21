#include <stdio.h>

#include "pw_ir.h"
#include "pw_ir_actions.h"


/*
 *  Runs in a loop, hence the global variable
 *  TODO: move global to static variable in function?
 */
ir_err_t pw_ir_advertise_and_listen(uint8_t *rx, size_t *n_read) {

    ir_err_t err = pw_ir_recv_packet(rx, 8, n_read);

    // if we didn't read anything, send an advertising packet
    if(*n_read == 0 && err == IR_OK) {
        err = pw_ir_send_advertising_packet();

        g_advertising_attempts++;
        if(g_advertising_attempts > MAX_ADVERTISING_PACKETS) {
            return IR_ERR_ADVERTISING_MAX;
        }
    }

    return err;
}


/*
 *  We should be in status CONNECT_STATUS_AWAITING and COMM_STATE_IDLE
 *  Do one action per iteration, called in the main event loop
 */
ir_err_t pw_try_connect_loop(uint8_t *packet, size_t packet_max, uint8_t *substate) {
    ir_err_t err;
    size_t n_read = 0;

    switch(*substate) {
        case 1: {   // substate listen and advertise
            err = pw_ir_advertise_and_listen(packet, &n_read);
            switch(err) {
                case IR_ERR_SIZE_MISMATCH:
                case IR_OK:
                    // we got a valid packet back, now check if master or slave
                    // set substate to master/slave check
                    *substate = 2;
                    printf("Recv packet during advertisement: first byte: %02x; size: %lu\n", packet[0], n_read);
                    break;
                case IR_ERR_TIMEOUT: // ignore timeout
                    break;
                case IR_ERR_ADVERTISING_MAX:
                    // TODO: cannot connect
                    //break;
                default:
                    // TODO: Handle error messages and quitting
                    pw_ir_set_connect_status(CONNECT_STATUS_DISCONNECTED);
                    err = IR_ERR_GENERAL;
                    break;
            }

        }
        case 2: {   // have peer, determine master/slave
            // We should already have a response in the packet buffer
            switch(packet[0]) {
                case CMD_ADVERTISING: // we found peer, we request master
                    packet[0x00] = CMD_ASSERT_MASTER;
                    packet[0x01] = EXTRA_BYTE_FROM_WALKER;
                    err = pw_ir_send_packet(packet, 8, &n_read);
                    *substate = 3;
                    break;
                case CMD_ASSERT_MASTER: // peer found us, peer requests master
                    packet[0x00] = CMD_SLAVE_ACK;
                    packet[0x01] = EXTRA_BYTE_FROM_WALKER;
                    err = pw_ir_send_packet(packet, 8, &n_read);
                    // we acknowledge master, then wait for instructions
                    pw_ir_set_connect_status(CONNECT_STATUS_SLAVE);
                    break;
                default:
                    return IR_ERR_UNEXPECTED_PACKET;
            }
            break;
        }
        case 3: {   // we have sent master

            // wait for answer
            err = pw_ir_recv_packet(packet, 8, &n_read);
            if(err != IR_OK) return err;

            // expect CMD_SLAVE_ACK
            if(packet[0] != CMD_SLAVE_ACK) {
                return IR_ERR_UNEXPECTED_PACKET;
            }

            // TODO: combine keys

            // key exchange done, we are now master
            pw_ir_set_connect_status(CONNECT_STATUS_MASTER);

            break;
        }

    }
    return IR_OK;
}

