#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <stdio.h>

#include "driver_ir.h"
#include "pw_ir.h"
#include "pw_ir_actions.h"
#include "app_comms.h"

static comm_substate_t g_comm_substate = COMM_SUBSTATE_NONE;
static uint8_t g_advertising_attempts = 0;

void pw_comms_init() {
    pw_ir_set_comm_state(COMM_STATE_AWAITING);
    g_comm_substate = COMM_SUBSTATE_FINDING_PEER;
    g_advertising_attempts = 0;

}

void pw_comms_event_loop() {
    ir_err_t err = IR_ERR_GENERAL;
    size_t n_read;

    comm_state_t cs = pw_ir_get_comm_state();
    switch(cs) {
        case COMM_STATE_AWAITING: {
            err = pw_action_try_find_peer(rx_buf, PW_TX_BUF_LEN, &g_comm_substate, &g_advertising_attempts);
            break;
        }
        case COMM_STATE_SLAVE: {
            printf("We are slave!\n");

            // TODO: need to allow non-fixed read lengths
            err = pw_ir_recv_packet(rx_buf, PW_TX_BUF_LEN, &n_read);
            if(err == IR_OK || err == IR_ERR_SIZE_MISMATCH) {
                pw_action_slave_perform_request(rx_buf, PW_TX_BUF_LEN);
            }
            break;
        }
        case COMM_STATE_MASTER: {
            printf("We are master!\n");
            // only thing we can do is ask for peer play

            pw_ir_set_comm_state(COMM_STATE_DISCONNECTED);
            break;
        }
        case COMM_STATE_DISCONNECTED: return;
        default: {
            pw_ir_die("Unknown state in pw_comms_event_loop");
            break;
        }
    }

    if(err != IR_OK) {
        printf("\tError code: %02x: %s\n", err, PW_IR_ERR_NAMES[err]);
        pw_ir_die("Error during pw_comms_event_loop");
        return;
    }

}


