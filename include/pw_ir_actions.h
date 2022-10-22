#ifndef PW_IR_ACTIONS_H
#define PW_IR_ACTIONS_H

#include <stdint.h>
#include <stddef.h>

#include "pw_ir.h"

typedef enum {
    COMM_SUBSTATE_NONE,
    COMM_SUBSTATE_FINDING_PEER,
    COMM_SUBSTATE_DETERMINE_ROLE,
    COMM_SUBSTATE_AWAITING_SLAVE_ACK,
    N_COMM_SUBSTATE,
} comm_substate_t;


ir_err_t pw_ir_advertise_and_listen(uint8_t *rx, size_t *pn_read, uint8_t *padvertising_attempts);
ir_err_t pw_try_connect_loop(uint8_t *packet, size_t packet_max,
        comm_substate_t *psubstate, uint8_t *padvertising_attempts);
ir_err_t pw_action_peer_play(comm_substate_t *psubstate);
ir_err_t pw_comms_slave_perform_action(uint8_t *packet, size_t len);


#endif /* PW_IR_ACTIONS_H */
