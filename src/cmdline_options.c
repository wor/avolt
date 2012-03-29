#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <limits.h>   /* INT_MAX and so on */

#include "cmdline_options.h"
#include "alsa_utils.h"
#include "avolt.conf.h"


/* Reads cmd_line options to given options struct variable */
bool read_cmd_line_options(
        const int argc,
        const char** argv,
        struct cmd_options* cmd_opt)
{
    const char* input_help = "[[-s] [+|-]<volume>]] [-t] [-to] [-v]"
        "\n\n"
        "Option help:\n"
        "v:\tBe more verbose.\n"
        "s:\tSet volume.\n"
        "t:\tToggle volume.\n"
        "to:\tToggle output.\n";

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-s") == 0) && (i+1 < argc)) {
            get_vol_from_arg(argv[++i], &cmd_opt->new_vol, &cmd_opt->inc);
        } else if (strcmp(argv[i], "-s") == 0) {
            /* If "-s" with no volume given set the default volume */
             cmd_opt->set_default_vol = true;
        } else if (strcmp(argv[i], "-v") == 0) {
            cmd_opt->verbose_level++;
        } else if (strcmp(argv[i], "-t") == 0) {
            cmd_opt->toggle_vol = 1;
        } else if (strcmp(argv[i], "-to") == 0) {
            cmd_opt->toggle_output = true;
        } else if (strcmp(argv[i], "-tf") == 0) { // XXX: Depracated output toggling
            cmd_opt->toggle_output = true;
        } else {
            get_vol_from_arg(argv[i], &cmd_opt->new_vol, &cmd_opt->inc);
            if (strcmp(argv[i], "0") != 0 &&
                    !(cmd_opt->new_vol != 0
                        && cmd_opt->new_vol != INT_MAX
                        && cmd_opt->new_vol != INT_MIN)) {
                fprintf(stderr, "avolt - v%s: %s %s\n",
                        VERSION, argv[0], input_help);
                print_config(stderr);
                return false;
            }
        }
    }
    return true;
}


/* gets volume from char* string */
void get_vol_from_arg(const char* arg, int* new_vol, bool* inc)
{
    if (strncmp(arg, "+", 1) == 0) {
        *inc = true;
        *new_vol = atoi(arg+1);
    } else if (strncmp(arg, "-", 1) == 0) {
        *new_vol = atoi(arg+1)*-1;
    } else {
        *new_vol = atoi(arg);
    }
}
