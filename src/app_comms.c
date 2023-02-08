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
static uint8_t g_comm_loop_counter = 0;

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
            printf("trying to find peer\n");
            err = pw_action_try_find_peer(rx_buf, PW_RX_BUF_LEN,
                    &g_comm_substate, &g_advertising_attempts);
            break;
        }
        case COMM_STATE_SLAVE: {
            printf("We are slave");
            err = pw_ir_recv_packet(rx_buf, PW_RX_BUF_LEN, &n_read);
            if(err == IR_OK || err == IR_ERR_SIZE_MISMATCH) {
                pw_action_slave_perform_request(rx_buf, PW_TX_BUF_LEN);
            }
            break;
        }
        case COMM_STATE_MASTER: {
            if(g_comm_substate == COMM_SUBSTATE_AWAITING_SLAVE_ACK) {
                g_comm_substate = COMM_SUBSTATE_START_PEER_PLAY;
            }
            printf("We are master\n");

            //err = pw_action_peer_play(&g_comm_substate, &g_comm_loop_counter, rx_buf, PW_RX_BUF_LEN);
            break;
        }
        case COMM_STATE_DISCONNECTED: return;
        default: {
            pw_ir_die("Unknown state in pw_comms_event_loop");
            break;
        }
    }

    if(err != IR_OK) {
        pw_ir_die("Error during pw_comms_event_loop");
        printf("\tError code: %02x: %s\n\tSubstate %d\n", err, PW_IR_ERR_NAMES[err], g_comm_substate);
        return;
    }

}


