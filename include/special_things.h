#ifndef SPECIAL_THINGS_H
#define SPECIAL_THINGS_H

#include <stdint.h>

#include "trainer_info.h"
#include "pw_ir.h"

ir_err_t send_event_data(
        pokemon_summary_t *ps,
        special_pokemon_info_t *sd,
        uint8_t *sprite,
        uint8_t *pokemon_name,
        uint16_t le_item,
        uint8_t *item_name
        );


#endif /* SPECIAL_THINGS_H */
