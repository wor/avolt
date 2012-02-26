#ifndef CMDLINE_OPTIONS_H_INCLUDED
#define CMDLINE_OPTIONS_H_INCLUDED

#include <stdio.h>
#include <stdbool.h>

#include "avolt.conf.h"

/* Command line options */
struct cmd_options
{
    bool set_default_vol;       // Set the default volume
    int new_vol;                // Set volume to this
    unsigned int toggle_vol;    // Toggle volume 0 <-> default_toggle_vol
    bool toggle_output;         // Toggle output
    bool inc;                   // Do we increase volume
    int verbose_level;          // Verbosity level
};


bool read_cmd_line_options(
        const int argc,
        const char** argv,
        struct cmd_options* cmd_opt);

void get_vol_from_arg(const char* arg, int* new_vol, bool* inc);

void print_config(FILE* output);

void print_profile(
        struct sound_profile const* profile,
        char const* indent,
        FILE* output);

struct sound_profile* get_current_sound_profile();

struct sound_profile* get_target_sound_profile(
        struct sound_profile* current);

void init_sound_profiles(snd_mixer_t* handle);

#endif
