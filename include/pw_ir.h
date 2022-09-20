#ifndef PW_IR_H
#define PW_IR_H

#include <stdint.h>
#include <stddef.h>

#define PW_RX_BUF_LEN   256
#define PW_TX_BUF_LEN   256

typedef enum {
    IR_STATE_IDLE,
    IR_STATE_KEYEX,
    IR_STATE_READY,
    IR_STATE_ADVERTISING,
    IR_STATE_COUNT,
} ir_state_t;

typedef enum {
    IR_OK,
    IR_ERR_GENERAL,
    IR_ERR_UNEXPECTED_PACKET,
    IR_ERR_LONG_PACKET,
    IR_ERR_SHORT_PACKET,
    IR_ERR_SIZE_MISMATCH,
    IR_ERR_ADVERTISING_MAX,
    IR_ERR_TIMEOUT,
    IR_ERR_BAD_SEND,
    IR_ERR_BAD_SESSID,
    IR_ERR_BAD_CHECKSUM,
    IR_ERR_COUNT,
} ir_err_t;

extern const char* const PW_IR_ERR_NAMES[];

ir_err_t pw_ir_send_packet(uint8_t *packet, size_t len);
ir_err_t pw_ir_recv_packet(uint8_t *packet, size_t len);
ir_err_t pw_ir_listen_for_handshake();
uint16_t pw_ir_checksum(uint8_t *packet, size_t len);

#endif /* PW_IR_H */

