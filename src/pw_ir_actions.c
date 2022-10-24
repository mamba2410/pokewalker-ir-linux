#include <stdio.h>

#include <stdlib.h> // rand()
#include <unistd.h> // usleep()

#include "eeprom_map.h"
#include "pw_ir.h"
#include "pw_ir_actions.h"
#include "eeprom.h"


/*
 *  Listen for a packet.
 *  If we don't hear anything, send advertising byte
 */
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
        case COMM_SUBSTATE_FINDING_PEER: {

            err = pw_action_listen_and_advertise(packet, &n_read, padvertising_attempts);

            switch(err) {
                case IR_ERR_SIZE_MISMATCH:  // also ok since we might recv 0xfc
                case IR_OK:
                    // we got a valid packet back, now check if master or slave on next iteration
                    *psubstate = COMM_SUBSTATE_DETERMINE_ROLE;
                case IR_ERR_TIMEOUT: err = IR_OK; break; // ignore timeout
                case IR_ERR_ADVERTISING_MAX: return IR_ERR_ADVERTISING_MAX;
                default: return IR_ERR_GENERAL;
            }

            break;
        }
        case COMM_SUBSTATE_DETERMINE_ROLE: {

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

                    pw_ir_set_comm_state(COMM_STATE_SLAVE);
                    break;
                default: return IR_ERR_UNEXPECTED_PACKET;
            }
            break;
        }
        case COMM_SUBSTATE_AWAITING_SLAVE_ACK: {   // we have sent master request

            // wait for answer
            err = pw_ir_recv_packet(packet, 8, &n_read);
            if(err != IR_OK) return err;

            if(packet[0] != CMD_SLAVE_ACK) return IR_ERR_UNEXPECTED_PACKET;

            // combine keys
            for(int i = 0; i < 4; i++)
                session_id[i] ^= packet[4+i];

            // key exchange done, we are now master
            pw_ir_set_comm_state(COMM_STATE_MASTER);
            break;
        }
        default: return IR_ERR_UNKNOWN_SUBSTATE;
    }
    return err;
}


/*
 *  We are slave, given already recv'd packet, respond appropriately
 */
ir_err_t pw_action_slave_perform_request(uint8_t *packet, size_t len) {

    ir_err_t err;

    switch(packet[0]) {
        default:
            err = IR_ERR_NOT_IMPLEMENTED;
            break;
    }

    return err;
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
ir_err_t pw_action_peer_play(comm_substate_t *psubstate, uint8_t *counter, uint8_t *packet, size_t max_len) {
    ir_err_t err = IR_ERR_GENERAL;
    size_t n_read;

    switch(*psubstate) {
        case COMM_SUBSTATE_START_PEER_PLAY: {

            packet[0] = CMD_PEER_PLAY_START;
            packet[1] = EXTRA_BYTE_FROM_WALKER;
            pw_eeprom_reliable_read(PW_EEPROM_ADDR_IDENTITY_DATA_1, PW_EEPROM_ADDR_IDENTITY_DATA_2,
                    packet+8, PW_EEPROM_SIZE_IDENTITY_DATA_1);
            packet[0x18] = (uint8_t)(rand()&0xff);  // Hack to change UID each time
                                                    // to prevent "alreadt connected" error
                                                    // TODO: remove this in proper code
            err = pw_ir_send_packet(packet, 8+PW_EEPROM_SIZE_IDENTITY_DATA_1, &n_read);
            if(err != IR_OK) return err;

            *psubstate = COMM_SUBSTATE_PEER_PLAY_ACK;
            break;
        }
        case COMM_SUBSTATE_PEER_PLAY_ACK: {

            err = pw_ir_recv_packet(packet, 8+PW_EEPROM_SIZE_IDENTITY_DATA_1, &n_read);
            if(err != IR_OK) return err;
            if(packet[0] != CMD_PEER_PLAY_RSP) return IR_ERR_UNEXPECTED_PACKET;
            switch(packet[0]) {
                case CMD_PEER_PLAY_RSP: break;
                case CMD_PEER_PLAY_SEEN: return IR_ERR_PEER_ALREADY_SEEN;
                default: return IR_ERR_UNEXPECTED_PACKET;
            }

            *psubstate = COMM_SUBSTATE_SEND_MASTER_SPRITES;
            *counter = 0;
            break;
        }
        case COMM_SUBSTATE_SEND_MASTER_SPRITES: {

            size_t write_size = 128;    // should always be 128-bytes
            size_t cur_write_size   = (size_t)(*counter) * write_size;

            err = pw_action_send_large_raw_data_from_eeprom(
                        PW_EEPROM_ADDR_IMG_POKEMON_SMALL_ANIMATED,              // src
                        PW_EEPROM_ADDR_IMG_CURRENT_PEER_POKEMON_ANIMATED_SMALL, // dst
                        PW_EEPROM_SIZE_IMG_POKEMON_SMALL_ANIMATED,              // size
                        write_size, counter, packet, max_len
                    );
            if(err != IR_OK) return err;

            if(cur_write_size >= PW_EEPROM_SIZE_IMG_POKEMON_SMALL_ANIMATED){
                *counter = 0;
                *psubstate = COMM_SUBSTATE_SEND_MASTER_NAME_IMAGE;
            }
            break;
        }
        case COMM_SUBSTATE_SEND_MASTER_NAME_IMAGE: {

            size_t write_size = 128;
            size_t cur_write_size   = (size_t)(*counter) * write_size;

            err = pw_action_send_large_raw_data_from_eeprom(
                        PW_EEPROM_ADDR_TEXT_POKEMON_NAME,               // src
                        PW_EEPROM_ADDR_TEXT_CURRENT_PEER_POKEMON_NAME,  // dst
                        PW_EEPROM_SIZE_TEXT_POKEMON_NAME,               // size
                        write_size, counter, packet, max_len
                    );

            if(cur_write_size >= PW_EEPROM_SIZE_TEXT_POKEMON_NAME){
                *counter = 0;
                *psubstate = COMM_SUBSTATE_SEND_MASTER_TEAMDATA;
            }
            break;
        }
        case COMM_SUBSTATE_SEND_MASTER_TEAMDATA: {

            size_t write_size = 128;
            size_t cur_write_size   = (size_t)(*counter) * write_size;

            err = pw_action_send_large_raw_data_from_eeprom(
                        PW_EEPROM_ADDR_TEAM_DATA_STRUCT,            // src
                        PW_EEPROM_ADDR_CURRENT_PEER_TEAM_DATA,      // dst
                        PW_EEPROM_SIZE_TEAM_DATA_STRUCT,            // size
                        write_size, counter, packet, max_len
                    );

            if(cur_write_size >= PW_EEPROM_SIZE_TEAM_DATA_STRUCT){
                *counter = 0;
                *psubstate = COMM_SUBSTATE_READ_SLAVE_SPRITES;
            }
            break;
        }
        case COMM_SUBSTATE_READ_SLAVE_SPRITES: {

            size_t read_size = 56;  // NOTE: this can be anything so long as it fits in your buffer
                                    // 56 becayse my Linux serial buffer is 64 bytes
                                    // TODO: move this buffer dependancy inti pw_ir_read()
            err = pw_action_read_large_raw_data_from_eeprom(
                        PW_EEPROM_ADDR_IMG_POKEMON_SMALL_ANIMATED,              // src
                        PW_EEPROM_ADDR_IMG_CURRENT_PEER_POKEMON_ANIMATED_SMALL, // dst
                        PW_EEPROM_SIZE_IMG_POKEMON_SMALL_ANIMATED,              // size
                        read_size, counter, packet, max_len
                    );

            size_t cur_read_size   = (size_t)(*counter) * read_size;

            if(cur_read_size >= PW_EEPROM_SIZE_IMG_POKEMON_SMALL_ANIMATED) {
                *counter = 0;
                *psubstate = COMM_SUBSTATE_READ_SLAVE_NAME_IMAGE;
            }
            break;
        }
        case COMM_SUBSTATE_READ_SLAVE_NAME_IMAGE: {

            size_t read_size = 56;  // TODO: See above
            err = pw_action_read_large_raw_data_from_eeprom(
                        PW_EEPROM_ADDR_TEXT_POKEMON_NAME,               // src
                        PW_EEPROM_ADDR_TEXT_CURRENT_PEER_POKEMON_NAME,  // dst
                        PW_EEPROM_SIZE_TEXT_POKEMON_NAME,               // size
                        read_size, counter, packet, max_len
                    );

            size_t cur_read_size   = (size_t)(*counter) * read_size;

            if(cur_read_size >= PW_EEPROM_SIZE_IMG_POKEMON_SMALL_ANIMATED) {
                *counter = 0;
                *psubstate = COMM_SUBSTATE_READ_SLAVE_TEAMDATA;
            }
            break;
        }
        case COMM_SUBSTATE_READ_SLAVE_TEAMDATA: {

            size_t read_size = 56;  // TODO: See above
            err = pw_action_read_large_raw_data_from_eeprom(
                        PW_EEPROM_ADDR_IMG_POKEMON_SMALL_ANIMATED,              // src
                        PW_EEPROM_ADDR_IMG_CURRENT_PEER_POKEMON_ANIMATED_SMALL, // dst
                        PW_EEPROM_SIZE_IMG_POKEMON_SMALL_ANIMATED,              // size
                        read_size, counter, packet, max_len
                    );

            size_t cur_read_size   = (size_t)(*counter) * read_size;

            if(cur_read_size >= PW_EEPROM_SIZE_IMG_POKEMON_SMALL_ANIMATED) {
                *counter = 0;
                *psubstate = COMM_SUBSTATE_SEND_PEER_PLAY_DX;
            }
            break;
        }
        case COMM_SUBSTATE_SEND_PEER_PLAY_DX: {
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

            // TODO: move sizze to #define
            err = pw_ir_send_packet(packet, 0x40, &n_read);;
            if(err != IR_OK) return err;

            *psubstate = COMM_SUBSTATE_RECV_PEER_PLAY_DX;
            break;
        }
        case COMM_SUBSTATE_RECV_PEER_PLAY_DX: {
            err = pw_ir_recv_packet(packet, 0x40, &n_read);
            if(err != IR_OK) return err;

            // TODO: Not 0x80-byte aligned
            pw_eeprom_write(PW_EEPROM_ADDR_CURRENT_PEER_DATA, packet, PW_EEPROM_SIZE_CURRENT_PEER_DATA);

            *psubstate = COMM_SUBSTATE_SEND_PEER_PLAY_END;
            break;
        }
        case COMM_SUBSTATE_SEND_PEER_PLAY_END: {
            packet[0] = CMD_PEER_PLAY_END;
            packet[1] = EXTRA_BYTE_TO_WALKER;
            err = pw_ir_send_packet(packet, 8, &n_read);

            *psubstate = COMM_SUBSTATE_RECV_PEER_PLAY_END;
            break;
        }
        case COMM_SUBSTATE_RECV_PEER_PLAY_END: {
            err = pw_ir_recv_packet(packet, 8, &n_read);
            if(err != IR_OK) return err;
            if(packet[0] != CMD_PEER_PLAY_END) return IR_ERR_UNEXPECTED_PACKET;
            *psubstate = COMM_SUBSTATE_DISPLAY_PEER_PLAY_ANIMATION;
            break;
        }
        case COMM_SUBSTATE_DISPLAY_PEER_PLAY_ANIMATION: {
            err = IR_ERR_NOT_IMPLEMENTED;
            break;
        }
        case COMM_SUBSTATE_CALCULATE_PEER_PLAY_GIFT: {
            err = IR_ERR_NOT_IMPLEMENTED;
            break;
        }
        default:
            err = IR_ERR_UNKNOWN_SUBSTATE;
            break;
    }

    return err;
}


/*
 *  Send an eeprom section from `src` on host to `dst` on peer.
 *  Throws error if `dst` or `final_write_size` isn't 128-byte aligned
 *
 *  Designed to be run in a loop, hence only one read and one write.
 */
ir_err_t pw_action_send_large_raw_data_from_eeprom(uint16_t src, uint16_t dst, size_t final_write_size,
        size_t write_size, uint8_t *pcounter, uint8_t *packet, size_t max_len) {
    ir_err_t err = IR_ERR_GENERAL;

    size_t cur_write_size   = (size_t)(*pcounter) * write_size;
    uint16_t cur_write_addr = dst + cur_write_size;
    uint16_t cur_read_addr  = src + cur_write_size;
    size_t n_read = 0;

    // If we have written something, we expect an acknowledgment
    if(cur_write_size > 0) {
        err = pw_ir_recv_packet(packet, 8, &n_read);
        if(err != IR_OK) return err;
        if(packet[0] != CMD_EEPROM_WRITE_ACK) return IR_ERR_UNEXPECTED_PACKET;
    }

    if( (cur_write_addr&0x07) > 0) return IR_ERR_UNALIGNED_WRITE;
    if( (final_write_size&0x07) > 0) return IR_ERR_UNALIGNED_WRITE;

    usleep(4000);   // ESSENTIAL
                    // TODO: create a pw_ir_delay_ms()

    if( cur_write_size < final_write_size) {
        packet[0] = (uint8_t)(cur_write_addr&0xff) + 2; // Need +2 to make it raw write command
        packet[1] = (uint8_t)(cur_write_addr>>8);
        pw_eeprom_read(cur_read_addr, packet+8, write_size);

        err = pw_ir_send_packet(packet, 8+write_size, &n_read);
        if(err != IR_OK) return err;
        (*pcounter)++;
    }

    return err;
}


/*
 *  Send an eeprom section from `src` on peer to `dst` on host.
 *
 *  Designed to be run in a loop, hence only one read and one write.
 */
ir_err_t pw_action_read_large_raw_data_from_eeprom(uint16_t src, uint16_t dst, size_t final_read_size,
        size_t read_size, uint8_t *pcounter, uint8_t *packet, size_t max_len) {

    ir_err_t err;
    size_t cur_read_size   = (size_t)(*pcounter) * read_size;
    uint16_t cur_write_addr = dst + cur_read_size;
    uint16_t cur_read_addr  = src + cur_read_size;
    size_t n_read = 0;

    size_t remaining_read = final_read_size - cur_read_size;
    if(remaining_read <= 0) return IR_OK;

    read_size = (remaining_read<read_size)?remaining_read:read_size;

    packet[0] = CMD_EEPROM_READ_REQ;;
    packet[1] = EXTRA_BYTE_TO_WALKER;
    packet[8] = (uint8_t)(cur_read_addr>>8);
    packet[9] = (uint8_t)(cur_read_addr&0xff);
    packet[10] = read_size;

    err = pw_ir_send_packet(packet, 8+3, &n_read);
    if(err != IR_OK) return err;

    usleep(4000);   // probably needed
                    // TODO: See above

    err = pw_ir_recv_packet(packet, read_size+8, &n_read);
    if(err != IR_OK) return err;
    if(packet[0] != CMD_EEPROM_READ_RSP) return IR_ERR_UNEXPECTED_PACKET;

    pw_eeprom_write(cur_write_addr, packet+8, read_size);

    (*pcounter)++;

    // TODO: create a new error for awaiting read/write?
    return err;
}


