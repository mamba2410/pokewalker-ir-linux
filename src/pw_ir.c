#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "driver_ir.h"
#include "pw_ir.h"

static connect_status_t g_connect_status = CONNECT_STATUS_DISCONNECTED;
uint8_t g_advertising_attempts = 0;

uint8_t session_id[4] = {0xde, 0xad, 0xbe, 0xef};

uint8_t tx_buf[PW_TX_BUF_LEN];
uint8_t rx_buf[PW_RX_BUF_LEN];
uint8_t tx_buf_aa[PW_TX_BUF_LEN];
uint8_t rx_buf_aa[PW_RX_BUF_LEN];

const char* const PW_IR_ERR_NAMES[] = {
    [IR_OK] = "ok",
    [IR_ERR_GENERAL] = "general",
    [IR_ERR_UNEXPECTED_PACKET] = "unexpected packet",
    [IR_ERR_LONG_PACKET] = "long packet",
    [IR_ERR_SHORT_PACKET] = "Short packet",
    [IR_ERR_SIZE_MISMATCH] = "size mismatch",
    [IR_ERR_ADVERTISING_MAX] = "max advertising",
    [IR_ERR_TIMEOUT] = "timeout",
    [IR_ERR_BAD_SEND] = "bad send",
    [IR_ERR_BAD_SESSID] = "bad session id",
    [IR_ERR_BAD_CHECKSUM] = "bad checksum",
};


ir_err_t pw_ir_send_packet(uint8_t *packet, size_t len, size_t *n_write) {

    for(uint8_t i = 0; i < 4; i++)
        packet[4+i] = session_id[i];

    packet[0x02] = 0;
    packet[0x03] = 0;

    uint16_t chk = pw_ir_checksum(packet, len);

    // Packet checksum little-endian
    packet[0x02] = (uint8_t)(chk&0xff);
    packet[0x03] = (uint8_t)(chk>>8);



    for(size_t i = 0; i < len; i++)
        tx_buf_aa[i] = packet[i] ^ 0xaa;

    *n_write = (size_t)pw_ir_write(tx_buf_aa, len);

    if(*n_write != len)
        return IR_ERR_BAD_SEND;

    return IR_OK;
}

ir_err_t pw_ir_recv_packet(uint8_t *packet, size_t len, size_t *n_read) {

    for(size_t i = 0; i < len; i++) {
        rx_buf_aa[i] = 0;
    }

    *n_read = (size_t)pw_ir_read(rx_buf_aa, len);

    if(*n_read <= 0) {
        return IR_ERR_TIMEOUT;
    }

    for(size_t i = 0; i < len; i++)
        packet[i] = rx_buf_aa[i] ^ 0xaa;

    if(*n_read != len) {
        return IR_ERR_SIZE_MISMATCH;
    }


    // packet chk LE
    uint16_t packet_chk = (((uint16_t)packet[3])<<8) + ((uint16_t)packet[2]);
    uint16_t chk = pw_ir_checksum(packet, len);

    if(packet_chk != chk) {
        return IR_ERR_BAD_CHECKSUM;
    }

    bool session_id_ok = true;
    for(size_t i = i; (i < 4)&&session_id_ok; i++)
        session_id_ok = packet[4+i] == session_id[i];

    if(!session_id_ok)
        return IR_ERR_BAD_SESSID;

    return IR_OK;
}


uint16_t pw_ir_checksum_seeded(uint8_t *data, size_t len, uint16_t seed) {
    // Dmitry's palm
    uint32_t crc = seed;
    for(size_t i = 0; i < len; i++) {
        uint16_t v = data[i];

        if(!(i&1)) v <<= 8;

        crc += v;
    }

    while(crc>>16) crc = (uint16_t)crc + (crc>>16);

    return crc;
}

uint16_t pw_ir_checksum(uint8_t *packet, size_t len) {

    uint16_t crc;
    uint8_t hc, lc;

    // save original checksum
    lc = packet[2];
    hc = packet[3];

    // zero checksum area
    packet[2] = 0;
    packet[3] = 0;

    crc = pw_ir_checksum_seeded(packet, 8, 0x0002);
    if(len>8) {
        crc = pw_ir_checksum_seeded(packet+8, len-8, crc);
    }

    // reload original checksum
    packet[2] = lc;
    packet[3] = hc;

    return crc;
}


void pw_ir_set_connect_status(connect_status_t s) {
    g_connect_status = s;
}

connect_status_t pw_ir_get_connect_status() {
    return g_connect_status;
}

ir_err_t pw_ir_send_advertising_packet() {

    uint8_t advertising_buf[] = {CMD_ADVERTISING^0xaa};

    int n = pw_ir_write(advertising_buf, 1);
    if(n <= 0) {
        return IR_ERR_BAD_SEND;
    }

    return IR_OK;
}


void pw_ir_die(const char* message) {
    printf("IR disconnecting: %s\n", message);
    pw_ir_set_connect_status(CONNECT_STATUS_DISCONNECTED);
}

