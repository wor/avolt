#ifndef AVOLT_CONF_H_INCLUDED
#define AVOLT_CONF_H_INCLUDED

#include <alsa/asoundlib.h>
#include <stdbool.h>

/* Different presentations for the volume */
enum Volume_type {
    alsa_percentage,        // Volume converted with alsa default algorithm to 0-100 range.
    hardware_percentage,    // Default hardware (probably logarithmic) range
                            // converted to 0-100 range.
    hardware,               // Volume in default hardware range.
    decibels,               // Volume in decibles with default hardware decibel
                            // range dived by 100, so if the hardware range is
                            // [-6000db,0d] then the used range is [-60,0].
};


/* Alsa mixer element config */
struct sound_profile
{
    char* profile_name;

    char* mixer_element_name;
    snd_mixer_elem_t* mixer_element;

    char* volume_cntrl_mixer_element_name;
    snd_mixer_elem_t* volume_cntrl_mixer_element;

    int default_volume;
    enum Volume_type volume_type;
    int soft_limit_volume;
    bool set_default_volume;
    bool confirm_exceeding_volume_limit;
};

void init_sound_profiles(snd_mixer_t* handle);

void print_config(FILE* output);

void print_profile(
        struct sound_profile const* profile,
        char const* indent,
        FILE* output);

struct sound_profile* get_current_sound_profile();

struct sound_profile* get_target_sound_profile(
        struct sound_profile* current);

bool get_mixer_front_panel_switch();

#endif
