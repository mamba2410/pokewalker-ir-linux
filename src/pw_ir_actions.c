#include <stdio.h>

#include "pw_ir.h"
#include "pw_ir_actions.h"


ir_err_t pw_ir_advertise_and_listen(uint8_t *rx, size_t *pn_read, uint8_t *padvertising_attempts) {

    ir_err_t err = pw_ir_recv_packet(rx, 8, pn_read);

    // if we didn't read anything, send an advertising packet
    if(*pn_read == 0) {
        (void)pw_ir_send_advertising_packet();

        (*padvertising_attempts)++;
        if(*padvertising_attempts > MAX_ADVERTISING_PACKETS) {
            return IR_ERR_ADVERTISING_MAX;
        }
    }

    return err;
}


/*
 *  We should be in COMM_STATE_AWAITING
 *  Do one action per call, called in the main event loop
 */
ir_err_t pw_try_connect_loop(uint8_t *packet, size_t packet_max,
        comm_substate_t *psubstate, uint8_t *padvertising_attempts) {

    ir_err_t err;
    size_t n_read = 0;

    switch(*psubstate) {
        case COMM_SUBSTATE_FINDING_PEER: {   // substate listen and advertise
            err = pw_ir_advertise_and_listen(packet, &n_read, padvertising_attempts);

            switch(err) {
                case IR_ERR_SIZE_MISMATCH:
                case IR_OK:
                    // we got a valid packet back, now check if master or slave on next iteration
                    *psubstate = COMM_SUBSTATE_DETERMINE_ROLE;
                    break;
                case IR_ERR_TIMEOUT: // ignore timeout
                    break;
                case IR_ERR_ADVERTISING_MAX:
                    // TODO: cannot connect
                    //break;
                default:
                    // TODO: Handle error messages and quitting
                    pw_ir_set_comm_state(COMM_STATE_DISCONNECTED);
                    err = IR_ERR_GENERAL;
                    break;
            }

            break;
        }
        case COMM_SUBSTATE_DETERMINE_ROLE: {   // have peer, determine master/slave
            // We should already have a response in the packet buffer
            switch(packet[0]) {
                case CMD_ADVERTISING: // we found peer, we request master
                    packet[0x00] = CMD_ASSERT_MASTER;
                    packet[0x01] = EXTRA_BYTE_FROM_WALKER;
                    err = pw_ir_send_packet(packet, 8, &n_read);
                    *psubstate = COMM_SUBSTATE_AWAITING_SLAVE_ACK;
                    break;
                case CMD_ASSERT_MASTER: // peer found us, peer requests master
                    packet[0x00] = CMD_SLAVE_ACK;
                    packet[0x01] = EXTRA_BYTE_FROM_WALKER;
                    err = pw_ir_send_packet(packet, 8, &n_read);
                    // we acknowledge master, then wait for instructions
                    pw_ir_set_comm_state(COMM_STATE_SLAVE);
                    break;
                default:
                    pw_ir_set_comm_state(COMM_STATE_DISCONNECTED);
                    return IR_ERR_UNEXPECTED_PACKET;
            }
            break;
        }
        case COMM_SUBSTATE_AWAITING_SLAVE_ACK: {   // we have sent master

            // wait for answer
            err = pw_ir_recv_packet(packet, 8, &n_read);
            if(err != IR_OK) return err;

            // expect CMD_SLAVE_ACK
            if(packet[0] != CMD_SLAVE_ACK) {
                return IR_ERR_UNEXPECTED_PACKET;
            }

            // TODO: combine keys

            // key exchange done, we are now master
            pw_ir_set_comm_state(COMM_STATE_MASTER);

            break;
        }
        default: {
            printf("In unknown substate: %d\n", *psubstate);
            return IR_ERR_GENERAL;
        }

    }
    return IR_OK;
}

ir_err_t pw_comms_slave_perform_action(uint8_t *packet, size_t len) {

    switch(packet[0]) {
        default:
            return IR_ERR_GENERAL;
    }

    return IR_OK;
}


ir_err_t pw_action_peer_play(comm_substate_t *psubstate) {

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

    return IR_OK;
}

