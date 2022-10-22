#ifndef PW_IR_ACTIONS_H
#define PW_IR_ACTIONS_H

#include <stdint.h>
#include <stddef.h>

#include "pw_ir.h"

ir_err_t pw_ir_advertise_and_listen(uint8_t *rx, size_t *n_read);
ir_err_t pw_try_connect_loop(uint8_t *packet, size_t packet_max, uint8_t *substate);

#define IR_SUBSTATE_FINDING_PEER        1
#define IR_SUBSTATE_DETERMINE_ROLE      2
#define IR_SUBSTATE_AWAITING_SLAVE_ACK  3

#endif /* PW_IR_ACTIONS_H */
