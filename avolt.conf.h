#ifndef AVOLT_CONF_H_INCLUDED
#define AVOLT_CONF_H_INCLUDED

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

static const char *Volume_type_to_str[] = {"alsa percentage", "hardware percentage",
     "hardware", "decibels"};


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
    bool set_default_volume; /* Set default volume when element toggled on */
    bool confirm_exeeding_volume_limit;
    //int (*on_func)(void); // XXX: not used yet
};


#endif