#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "driver_ir.h"
#include "pw_ir.h"

static ir_state_t current_state = IR_STATE_IDLE;
static uint8_t session_id[4] = {5, 5, 5, 5};

static uint8_t rx_buf[PW_RX_BUF_LEN];
static uint8_t tx_buf[PW_TX_BUF_LEN];
static uint8_t tx_buf_aa[PW_TX_BUF_LEN];
static uint8_t rx_buf_aa[PW_RX_BUF_LEN];

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


ir_err_t pw_ir_send_packet(uint8_t *packet, size_t len) {

    for(uint8_t i = 0; i < 4; i++)
        packet[4+i] = session_id[i];

    packet[0x02] = 0;
    packet[0x03] = 0;

    uint16_t chk = pw_ir_checksum(packet, len);

    packet[0x02] = (uint8_t)(chk>>8);
    packet[0x03] = (uint8_t)(chk&0xff);


    for(size_t i = 0; i < len; i++)
        tx_buf_aa[i] = packet[i] ^ 0xaa;

    int n_written = pw_ir_write(tx_buf_aa, len);

    if(n_written != len)
        return IR_ERR_BAD_SEND;

    return IR_OK;
}

ir_err_t pw_ir_recv_packet(uint8_t *packet, size_t len) {

    for(size_t i = 0; i < len; i++) {
        rx_buf_aa[i] = 0;
    }

    int n_read = pw_ir_read(rx_buf_aa, len);

    size_t mismatch = (size_t)n_read;
    if(mismatch != len) {
        printf("read %d; expected %d\n", n_read, len);
        printf("Packet header: ");
        for(size_t i = 0; i < 8; i++) {
            printf("0x%02x ", packet[i]);
        }
        printf("\n");
        return IR_ERR_SIZE_MISMATCH;

    }

    for(size_t i = 0; i < len; i++)
        packet[i] = rx_buf_aa[i] ^ 0xaa;

    uint16_t chk = pw_ir_checksum(packet, len);
    uint16_t packet_chk = (uint16_t)packet[2]<<8 | (uint16_t)packet[3];

    if(packet_chk != chk)
        return IR_ERR_BAD_CHECKSUM;

    bool session_id_ok = true;
    for(size_t i = i; (i < 4)&&session_id_ok; i++)
        session_id_ok = packet[4+i] == session_id[i];

    if(!session_id_ok)
        return IR_ERR_BAD_SESSID;

    return IR_OK;
}


uint16_t pw_ir_checksum(uint8_t *packet, size_t len) {
    uint16_t crc = 0x0002;

    for(size_t i = 0; i < len; i++) {
        uint16_t v = packet[i];

        if(!(i&1)) v <<= 8;

        crc += v;
    }

    while(crc>>16) crc = (uint16_t)crc + (crc>>16);

    return crc;
}


ir_err_t pw_ir_listen_for_handshake() {
    uint8_t *tx = tx_buf;
    uint8_t *rx = rx_buf;
    ir_err_t err;

    err = pw_ir_recv_packet(rx, 1);
    usleep(5*1000);

    if(err != IR_OK && err != IR_ERR_BAD_SESSID && err != IR_ERR_BAD_CHECKSUM) {
        return err;
    }

    printf("Received response: 0x%02x\n", rx[0]);
    if(rx[0] != 0xfc)
        return IR_ERR_UNEXPECTED_PACKET;

    printf("Got advert packet\n");

    tx[0] = 0xfa;
    tx[1] = 0x01;
    for(size_t i = 0; i < 4; i++) {
        tx[4+i] = session_id[i];
    }

    err = pw_ir_send_packet(tx, 8);
    if(err != IR_OK) return err;
    printf("Sent response packet\n");

    pw_ir_flush_rx();
    err = pw_ir_recv_packet(rx, 8);
    if(err != IR_OK) {
        printf("Packet header: ");
        //for(size_t i = 0; i < 8; i++) {
        for(size_t i = 0; i < 8; i++) {
            printf("0x%02x ", rx[i]);
        }
        printf("\n");

        return err;
    }

    if(rx[0] == 0xfa) {
        printf("Got to resp\n");
    } else if(tx[0] != 0xf8) {
        printf("Got resp: %02x\n", tx[0]);
        return IR_ERR_UNEXPECTED_PACKET;
    }

    printf("Keyex successful!\nsession_id: ");
    for(size_t i = 0; i < 4; i++) {
        session_id[i] = tx[4+i];
        printf("%02x", session_id[i]);
    }
    printf("\n");


    return IR_OK;
}
