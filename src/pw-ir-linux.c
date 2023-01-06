#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>

#include "pw-ir-linux.h"
#include "pw_ir.h"
#include "driver_ir.h"
#include "driver_eeprom_linux.h"
#include "app_comms.h"
#include "pw_ir_actions.h"
#include "eeprom_map.h"
#include "trainer_info.h"
#include "special_things.h"


int send_custom_event_pokemon(const char* sprite_file, const char* pokemon_name_file, const char* item_name_file) {
    pokemon_summary_t ps = {
        le_species: 0x004c, // 4a = geodude
        le_held_item: 0x28, // 28 = potion
        le_moves: {0x0021, 0x006f, 0, 0},   // 0x0021=tackle, 0x006f=def curl
        level: 100,
        pokemon_flags_1: 0x20,  // mask 0x20 = female
        pokemon_flags_2: 0x02,  // mask 0x02 = shiny
    };

    special_pokemon_info_t sd = {
        le_ot_tid: 44,
        le_ot_sid: 55,
        le_location_met: 1, // idfk
        ot_name: {0x0137, 0x0145, 0x0151, 0x0146, 0x0145, 0xffff, 0, 0},    // Mamba
        encounter_type: 0,  // 0 = fateful?
        ability: 1, //
        le_pokeball_item: 28, // potion (maybe breaks?)
        le_unk1: 0,       // changes nature?
        le_unk2: 0x7f,    // changes IVs?
        unk3: {0xfc, 0xfc, 0x00, 0, 0, 0, 0, 0, 0, 0},
    };

    uint16_t item = 0x28; // 28 = potion  (given revive when tested??)


    uint8_t *sprite=0, *pokemon_name=0, *item_name=0;

    FILE *fp = fopen(sprite_file, "r");
    if(!fp) return -1;
    struct stat s;
    if( stat(sprite_file, &s) == -1 ) return -1;
    sprite = malloc(s.st_size);
    fread(sprite, s.st_size, 1, fp);
    fclose(fp);

    fp = fopen(pokemon_name_file, "r");
    if(!fp) return -1;
    if( stat(pokemon_name_file, &s) == -1 ) return -1;
    pokemon_name = malloc(s.st_size);
    fread(pokemon_name, s.st_size, 1, fp);
    fclose(fp);

    fp = fopen(item_name_file, "r");
    if(!fp) return -1;
    if( stat(item_name_file, &s) == -1 ) return -1;
    item_name = malloc(s.st_size);
    fread(item_name, s.st_size, 1, fp);
    fclose(fp);

    ir_err_t err = send_event_data(
        &ps, &sd, sprite, pokemon_name, item, item_name
    );

    if(err != IR_OK) {
        printf("Error when sending special pokemon\n");
        printf("Error code: %02x: %s\n", err, PW_IR_ERR_NAMES[err]);
        return -1;
    }

    return 0;
}

void run_comms_loop() {
    comm_state_t cs;

    // Run our comms loop
    do {
        pw_comms_event_loop();
    } while( (cs=pw_ir_get_comm_state()) != COMM_STATE_DISCONNECTED );
}


void full_send_shellcode(uint8_t *shellcode, size_t len) {
    comm_state_t cs = COMM_STATE_DISCONNECTED;
    ir_err_t err;

    do {
        pw_comms_event_loop();
    } while( (cs=pw_ir_get_comm_state()) == COMM_STATE_AWAITING );

    switch(cs) {
        case COMM_STATE_MASTER: {
            printf("Attempting to send shellcode.\n");

            // try our action
            err = send_shellcode(shellcode, len);

            if(err != 0)
                printf("Error: %d\n", err);

            break;
        }
        case COMM_STATE_SLAVE: {
            printf("We ended up as slave, aborting.\n");
            break;
        }
        case COMM_STATE_DISCONNECTED: {
            printf("Cannot connect to walker.\n");
            break;
        }
        default: {
            printf("Unknown state %d, aborting.\n", cs);
            break;
        }
    }
}

void dump_rom(uint64_t which, const char* fname) {
    comm_state_t cs = COMM_STATE_DISCONNECTED;
    ir_err_t err;

    do {
        pw_comms_event_loop();
    } while( (cs=pw_ir_get_comm_state()) == COMM_STATE_AWAITING );

    switch(cs) {
        case COMM_STATE_MASTER: {
            printf("Attempting dump.\n");

            // try our action
            switch(which) {
                case 48: err = dump_48k_rom(fname); break;
                case 64: err = dump_64k_rom(fname); break;
                default: {
                    printf("Valid options 64 or 48, not %lu\n", which);
                    err = IR_ERR_GENERAL;
                    break;
                }
            }

            if(err != IR_OK)
                printf("Error code: %02x: %s\n", err, PW_IR_ERR_NAMES[err]);

            break;
        }
        case COMM_STATE_SLAVE: {
            printf("We ended up as slave, aborting.\n");
            break;
        }
        case COMM_STATE_DISCONNECTED: {
            printf("Cannot connect to walker.\n");
            break;
        }
        default: {
            printf("Unknown state %d, aborting.\n", cs);
            break;
        }
    }
}


void exit_ok() {
    pw_ir_deinit();
    exit(0);
}


void usage() {
    printf("\
Usage: pw-ir [action, [options] ]\n\
\n\
Actions:\n\
    --dump-rom <rom>, -d <rom>          Dump rom, either \'64\' or \'48\'.\n\
    --shellcode <file>, -s <file>       Upload shellcode from binary file.\n\
    --gift-files <files>, -f <files>    Upload gift files. Must be in the order\n\
                                        pokemon_summary:pokemon_image_name:item_image_name\n\
\n\
Options:\n\
    --out-file <file>                   Output file for things like dumps.\n\
    --stamps                            If we should include stamps in the gift.\n\
    --gift <type>, -g <type>            Send a gift. `type` is either \'item\' or \'pokemon\'.\n\
                                        One at a time, need to have uploaded gift files at least once before.\n\
");

}

int main(int argc, char** argv){

    static int stamps = 0;
    int c, option_index=0;
    int dump=0;
    int has_action=0;

    char *out_fname=0, *shellcode_fname=0;
    char* gift_files[3];
    enum gift_type gift = GIFT_NONE;

    static struct option long_options[] = {
        // name,        arg?,               flag var,   index
        {"dump-rom",    required_argument,  0,          'd'},
        {"shellcode",   required_argument,  0,          's'},
        {"out-file",    required_argument,  0,          'o'},
        {"send-gift",   required_argument,  0,          'g'},
        {"gift-files",  required_argument,  0,          'f'},
        {"stamps",      no_argument,        &stamps,    1},
        {0, 0, 0, 0}
    };
    static const char* short_options = "d:s:o:g:f:m";

    while( (c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1 ) {
        switch(c) {
            case 'd': {
                dump = atoi(optarg);
                if(!has_action) {
                    has_action = 1;
                } else {
                    printf("Error: Can only do one action at a time\n.");
                    usage();
                    exit(EXIT_FAILURE);
                }
                break;
            }
            case 's': {
                shellcode_fname = optarg;
                if(!has_action) {
                    has_action = 1;
                } else {
                    printf("Error: Can only do one action at a time\n.");
                    usage();
                    exit(EXIT_FAILURE);
                }
                break;
            }
            case 'o': {
                out_fname = optarg;
                break;
            }
            case 'f': {
                const char* delims = ",:";
                char* tok = strtok(optarg, delims);
                int i;
                for(i = 0; tok != 0; i++) {
                    gift_files[i] = tok;
                    tok = strtok(0, delims);
                }

                if(i != 3) {
                    printf("Error: expected 3 input files for gift pokemon. Got %d.\n", i);
                    usage();
                    exit(EXIT_FAILURE);
                }

                if(!has_action) {
                    has_action = 1;
                } else {
                    printf("Error: Can only do one action at a time\n.");
                    usage();
                    exit(EXIT_FAILURE);
                }

                break;
            }
            case 'g': {
                // gift item or pokemon
                if (strncmp("item", optarg, 16) == 0 ) {
                    gift = GIFT_ITEM;
                } else if(strncmp("pokemon", optarg, 16) == 0) {
                    gift = GIFT_POKEMON;
                } else {
                    printf("Error: cannot gift \'%s\', expected either \'pokemon\' or \'item\'\n.", optarg);
                    usage();
                    exit(EXIT_FAILURE);
                }
                break;
            }
            default: {
                printf("Unknown arg: %c\n.", c);
                usage();
                break;
            }
        }
    }

    pw_ir_init();
    //pw_eeprom_raw_init();
    pw_comms_init();


    comm_state_t cs = COMM_STATE_AWAITING;

    if(gift_files[0] != 0) {
        printf("Present Pokewalker...\n");
        do {
            pw_comms_event_loop();
        } while( (cs=pw_ir_get_comm_state()) != COMM_STATE_MASTER );

        // pokemon_summary, special_data, sprite, pokemon_name, item id, item_name
        //send_event_data(gift_files[0],gift_files[1], gift_files[2]);

        printf("Done!\n");
        exit_ok();
    }

    if(gift != GIFT_NONE) {
        printf("Present Pokewalker...\n");
        do {
            pw_comms_event_loop();
        } while( (cs=pw_ir_get_comm_state()) != COMM_STATE_MASTER );

        ir_err_t err = send_event_command(gift, stamps);
        if(err != IR_OK) {
            printf("Error: can't send gift command.\n");
            pw_ir_deinit();
            exit(EXIT_FAILURE);
        }
        printf("Done!\n");
        exit_ok();
    }


    if(dump != 0) {
        if(out_fname == 0) {
            printf("Error: Can't dump rom, missing output file (hint: use `-o` to set it)\n");
            pw_ir_deinit();
            exit(EXIT_FAILURE);
        }
        switch(dump) {
            case 48:
            case 64: {
                printf("Present Pokewalker...\n");
                dump_rom(dump, out_fname);
                break;
            }
            default: {
                printf("Error: Can't dump %dk rom. Options are [48, 64]\n", dump);
                pw_ir_deinit();
                exit(EXIT_FAILURE);
            }
        }
        exit_ok();
    }

    if(shellcode_fname != 0) {

        size_t shellcode_len;
        uint8_t *shellcode = load_shellcode(shellcode_fname, &shellcode_len);

        if(shellcode == 0) {
            pw_ir_deinit();
            printf("Error loading shellcode from file \'%s\'\n", shellcode_fname);
            return -1;
        }
        printf("Loaded shellcode\n");
        printf("shellcode: %p, %lu\n", shellcode, shellcode_len);

        printf("Present Pokewalker...\n");
        full_send_shellcode(shellcode, shellcode_len);
        printf("Sent shellcode\n");

        free(shellcode);

        exit_ok();
    }


    exit_ok();
}

