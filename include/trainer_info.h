#ifndef PW_TRAINER_INFO_H
#define PW_TRAINER_INFO_H

#include <stdint.h>

typedef struct {
    uint8_t data[0x28];
} unique_identity_data_t;

typedef struct {
    uint8_t data[0x10];
} event_bitmap_t;

typedef struct __attribute__((__packed__)) {
    uint32_t le_unk0;
    uint32_t le_unk1;
    uint16_t le_unk2;
    uint16_t le_unk3;
    uint16_t le_tid;
    uint16_t le_sid;
    unique_identity_data_t identity_data;
    event_bitmap_t event_bitmap;
    uint16_t le_trainer_name[8];
    uint8_t unk4[3];
    uint8_t flags;
    uint8_t protocol_ver;
    uint8_t unk5;
    uint8_t protocol_subver;
    uint8_t unk8;
    uint32_t be_last_sync;
    uint32_t be_step_count;
} walker_info_t;

typedef struct __attribute__((__packed__)) {
    uint32_t be_total_steps;
    uint32_t be_today_steps;
    uint32_t be_last_sync;
    uint16_t be_total_days;
    uint16_t be_current_watts;
    uint8_t unk[4];
    uint8_t padding[3];
    uint8_t settings;
} health_data_t;

typedef struct __attribute__((__packed__)) {
    uint8_t flags;
    uint8_t commands[0x3f];
} lcd_config_t;

typedef struct __attribute__((__packed__)) {
    uint32_t le_current_steps;
    uint16_t le_current_watts;
    uint8_t padding[2];
    uint32_t le_unk0;
    uint16_t le_unk2;
    uint16_t le_species;
    uint16_t pokemon_name[11];
    uint16_t trainer_name[8];
    uint8_t pokemon_flags_1;
    uint8_t pokemon_flags_2;
} peer_play_data_t;

typedef struct __attribute__((__packed__)) {
    uint16_t le_species;
    uint16_t le_held_item;
    uint16_t le_moves[4];
    uint8_t level;
    uint8_t pokemon_flags_1;
    uint8_t pokemon_flags_2;
    uint8_t padding;
} pokemon_summary_t;

typedef struct __attribute__((__packed__)) {
    uint16_t le_species;
    uint16_t le_held_item;
    uint16_t le_moves[4];
    uint16_t le_ot_tid;
    uint16_t le_ot_sid;
    uint32_t le_pid;
    uint32_t ivs;   // packed to 5-bits each
    uint8_t evs[6];
    uint8_t pokemon_flags_1;
    uint8_t source_game;
    uint8_t ability;
    uint8_t happiness;
    uint8_t level;
    uint8_t padding;
    uint16_t nickname[10];
} pokemon_info_t;

typedef struct __attribute__((__packed__)) {
    uint32_t le_unk0;
    uint16_t le_ot_tid;
    uint16_t le_ot_sid;
    uint16_t le_unk1;
    uint16_t le_location_met;
    uint16_t le_unk2;
    uint16_t ot_name[8];
    uint8_t encounter_type;
    uint8_t ability;
    uint16_t le_pokeball_item;
    uint8_t unk3[10];
} special_pokemon_info_t;

typedef struct __attribute__((__packed__)) {
    uint8_t factory_data[3];
    unique_identity_data_t unique_data;
    uint8_t padding_1;
    lcd_config_t lcd_config;
    uint8_t padding_2; // random 0xbf byte?
    walker_info_t walker_info;
    uint8_t padding_3;
    health_data_t health_data;
    uint8_t padding_4;
    uint8_t copy_marker;
    uint8_t padding[16];
} reliable_data_t;

typedef struct {
    uint8_t stamp_heart: 1;
    uint8_t stamp_space: 1;
    uint8_t stamp_diamond: 1;
    uint8_t stamp_club: 1;
    uint8_t special_map: 1;
    uint8_t event_pokemon: 1;
    uint8_t evet_item: 1;
    uint8_t special_route: 1;
} special_inventory_t;

typedef struct {
    /* +0x00 */ pokemon_summary_t pokemon_summary;
    /* +0x10 */ uint16_t pokemon_nickname[11];
    /* +0x26 */ uint8_t pokemon_happiness;
    /* +0x27 */ uint8_t route_image_index;
    /* +0x28 */ uint16_t route_name[21];
    /* +0x52 */ pokemon_summary_t route_pokemon[3];
    /* +0x82 */ uint16_t le_route_pokemon_steps[3];
    /* +0x88 */ uint8_t route_pokemon_percent[3];
    /* +0x8b */ uint8_t padding;
    /* +0x8c */ uint16_t le_route_items[10];
    /* +0xa0 */ uint16_t le_route_item_steps[10];
    /* +0xb4 */ uint8_t route_item_percent[10];
} route_info_t;


/*
 *  size: 0x5e = 94 bytes
 *  dmitry: struct EventLogItem
 */
typedef struct __attribute__((__packed__)) {
    /* 0x00 */ uint32_t be_time;
    /* 0x04 */ uint32_t le_unk0;
    /* 0x08 */ uint16_t le_unk2;
    /* 0x0a */ uint16_t le_our_species;
    /* 0x0c */ uint16_t le_other_species;
    /* 0x0e */ uint16_t le_extra;
    /* 0x10 */ uint16_t other_trainer_name[8];
    /* 0x20 */ uint16_t our_pokemon_name[11];
    /* 0x36 */ uint16_t other_pokemon_name[11];
    /* 0x4c */ uint8_t  route_image_index;
    /* 0x4d */ uint8_t  pokemon_friendship;
    /* 0x4e */ uint16_t be_our_watts;
    /* 0x50 */ uint16_t be_other_watts;
    /* 0x52 */ uint32_t be_steps;
    /* 0x56 */ uint32_t be_other_steps;
    /* 0x5a */ uint8_t  event_type;
    /* 0x5b */ uint8_t  our_pokemon_flags;
    /* 0x5c */ uint8_t  other_pokemon_flags;
    /* 0x5d */ uint8_t  padding;
} event_log_item_t;



#endif /* PW_TRAINER_INFO_H */
