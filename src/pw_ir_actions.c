#include <stdio.h>

#include "eeprom_map.h"
#include "pw_ir.h"
#include "pw_ir_actions.h"
#include "eeprom.h"


ir_err_t pw_action_listen_and_advertise(uint8_t *rx, size_t *pn_read, uint8_t *padvertising_attempts) {

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
ir_err_t pw_action_try_find_peer(uint8_t *packet, size_t packet_max,
        comm_substate_t *psubstate, uint8_t *padvertising_attempts) {

    ir_err_t err;
    size_t n_read = 0;

    switch(*psubstate) {
        case COMM_SUBSTATE_FINDING_PEER: {   // substate listen and advertise
            err = pw_action_listen_and_advertise(packet, &n_read, padvertising_attempts);

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

            // combine keys
            for(int i = 0; i < 4; i++)
                session_id[i] ^= packet[4+i];

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

ir_err_t pw_action_slave_perform_request(uint8_t *packet, size_t len) {

    switch(packet[0]) {
        default:
            return IR_ERR_GENERAL;
    }

    return IR_OK;
}

/*
 *  Sequence:
 *  send CMD_PEER_PLAY_START
 *  recv CMD_PEER_PLAY_RSP
 *  send master EEPROM:0x91BE to slave EEPROM:0xF400
 *  send master EEPROM:0xCC00 to slave EEPROM:0xDC00
 *  read slave EEPROM:0x91BE to master EEPROM:0xF400
 *  read slave EEPROM:0xCC00 to master EEPROM:0xDC00
 *  send CMD_PEER_PLAY_DX
 *  recv CMD_PEER_PLAY_DX ?
 *  write data to master EEPROM:0xF6C0
 *  send CMD_PEER_PLAY_END
 *  recv CMD_PEER_PLAY_END
 *  display animation
 *  calculate gift
 */
ir_err_t pw_action_peer_play(comm_substate_t *psubstate, uint8_t *packet, size_t max_len) {
    ir_err_t err = IR_ERR_GENERAL;
    size_t n_read;

    switch(*psubstate) {
        case COMM_SUBSTATE_START_PEER_PLAY: {
            printf("Starting peer play!\n");
            packet[0] = CMD_PEER_PLAY_START;
            packet[1] = EXTRA_BYTE_TO_WALKER;
            //pw_eeprom_reliable_read(PW_EEPROM_ADDR_IDENTITY_DATA_1, PW_EEPROM_ADDR_IDENTITY_DATA_2,
            //        packet+8, PW_EEPROM_SIZE_IDENTITY_DATA_1);
            pw_eeprom_read(PW_EEPROM_ADDR_IDENTITY_DATA_1, packet+8, PW_EEPROM_SIZE_IDENTITY_DATA_1);
            err = pw_ir_send_packet(packet, 8+PW_EEPROM_SIZE_IDENTITY_DATA_1, &n_read);
            printf("wrote %lu bytes\n", n_read);

            if(err != IR_OK) return err;
            *psubstate = COMM_SUBSTATE_PEER_PLAY_ACK;
            break;
        }
        case COMM_SUBSTATE_PEER_PLAY_ACK: {
            printf("Awaiting response\n");
            err = pw_ir_recv_packet(packet, 8+PW_EEPROM_SIZE_IDENTITY_DATA_1, &n_read);
            printf("Got peer play resp\n");

            if(err != IR_OK) return err;
            if(packet[0] != CMD_PEER_PLAY_RSP) return IR_ERR_UNEXPECTED_PACKET;

            printf("Peer play resp ok\n");

            // TODO: cast to identity_data_t struct

            usleep(3000);
            packet[0] = CMD_PEER_PLAY_DX;
            packet[1] = 1;

            // TODO: Actually make proper peer_play_data_t
            packet[0x08] = 0x0f;    // current steps = 9999
            packet[0x09] = 0x27;
            packet[0x0a] = 0;
            packet[0x0b] = 0;
            packet[0x0c] = 0x0f;    // current watts = 9999
            packet[0x0d] = 0x27;
            // 0x0e, 0x0f padding
            packet[0x10] = 1;   // identity_data_t.unk0
            packet[0x11] = 0;
            packet[0x12] = 0;
            packet[0x13] = 0;
            packet[0x14] = 7;   // identity_data_t.unk2
            packet[0x15] = 0;
            // species
            pw_eeprom_read(PW_EEPROM_ADDR_ROUTE_INFO+0, packet+0x16, 2);
            // 22 bytes pokemon nickname
            pw_eeprom_read(PW_EEPROM_ADDR_ROUTE_INFO+10, packet+0x18, 22);
            // 16 bytes trainer name
            pw_eeprom_read(PW_EEPROM_ADDR_IDENTITY_DATA_1+72, packet+0x2e, 16);
            // 1 byte pokemon gender
            pw_eeprom_read(PW_EEPROM_ADDR_ROUTE_INFO+13, packet+0x3e, 1);
            // 1 byte pokeIsSpecial
            pw_eeprom_read(PW_EEPROM_ADDR_ROUTE_INFO+14, packet+0x3f, 1);

            // send
            err = pw_ir_send_packet(packet, 0x40, &n_read);;
            if(err != IR_OK) return err;

            printf("Done sending DX, moving on\n");
            *psubstate = COMM_SUBSTATE_SEND_MASTER_SPRITES;

            break;
        }
        case COMM_SUBSTATE_SEND_MASTER_SPRITES: {
            break;
        }
        case COMM_SUBSTATE_SEND_MASTER_TEAMDATA: {
            break;
        }
        case COMM_SUBSTATE_READ_SLAVE_SPRITES: {
            break;
        }
        case COMM_SUBSTATE_READ_SLAVE_TEAMDATA: {
            break;
        }
        case COMM_SUBSTATE_SEND_PEER_PLAY_DX: {
            break;
        }
        case COMM_SUBSTATE_RECV_PEER_PLAY_DX: {
            break;
        }
        case COMM_SUBSTATE_WRITE_PEER_PLAY_DATA: {
            break;
        }
        case COMM_SUBSTATE_SEND_PEER_PLAY_END: {
            break;
        }
        case COMM_SUBSTATE_RECV_PEER_PLAY_END: {
            break;
        }
        case COMM_SUBSTATE_DISPLAY_PEER_PLAY_ANIMATION: {
            break;
        }
        case COMM_SUBSTATE_CALCULATE_PEER_PLAY_GIFT: {
            break;
        }
        default:
            // die
            printf("Unknown state\n");
            break;
    }


    return err;
}

