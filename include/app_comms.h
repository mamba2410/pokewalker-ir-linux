#ifndef PW_APP_COMMS_H
#define PW_APP_COMMS_H

#include <stdint.h>
#include <stddef.h>

#define PW_COMMS_SM_PP_INIT  1

void pw_comms_init();
void pw_comms_event_loop();
void pw_comms_init_display();
void pw_comms_handle_input(uint8_t b);
void pw_comms_draw_update();

int pw_action_peer_play();
int pw_comms_slave_perform_action(uint8_t *packet, size_t len);

#endif /* PW_APP_COMMS_H */