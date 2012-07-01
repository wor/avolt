// -*- coding: utf-8 -*- vim:fenc=utf-8:ft=c
#include <stdio.h>
#include <stdbool.h>
#include <strings.h>

#include "avolt.conf.h"
#include "alsa_utils.h"
#include "wutil.h"

/* Program configuration */
#include "avolt.conf"


static const char *Volume_type_to_str[] = {"alsa percentage", "hardware percentage",
     "hardware", "decibels"};

/* Initializes all sound profiles from SOUND_PROFILES array.
 * Returns true if at least one profile was successfully initialized. */
bool init_sound_profiles(snd_mixer_t* handle)
{
    bool one_success = false;
    for (int i = 0; i < SOUND_PROFILES_SIZE; ++i) {
        PD_M("Initializing profile: %s\n", SOUND_PROFILES[i]->profile_name);
        SOUND_PROFILES[i]->mixer_element = get_elem(handle, SOUND_PROFILES[i]->mixer_element_name);
        if (SOUND_PROFILES[i]->mixer_element) {
            SOUND_PROFILES[i]->init_ok = true;
        }
        if (SOUND_PROFILES[i]->volume_cntrl_mixer_element_name) {
            SOUND_PROFILES[i]->volume_cntrl_mixer_element = get_elem(handle, SOUND_PROFILES[i]->volume_cntrl_mixer_element_name);
            if (SOUND_PROFILES[i]->volume_cntrl_mixer_element == NULL) {
                SOUND_PROFILES[i]->init_ok = false;
            }
        }
        else {
            SOUND_PROFILES[i]->volume_cntrl_mixer_element_name = SOUND_PROFILES[i]->mixer_element_name;
            SOUND_PROFILES[i]->volume_cntrl_mixer_element = SOUND_PROFILES[i]->mixer_element;
        }

        // Check if profile initialization was successful
        if (SOUND_PROFILES[i]->init_ok) {
            PD_M("Initializing profile: '%s' ..successful\n", SOUND_PROFILES[i]->profile_name);
            one_success = true;
        }
    }

    return one_success;
}


/* Print information to given FILE* about statically set config options. */
void print_config(FILE* output)
{
    fprintf(output, "Static option help:\n");
    fprintf(output, "The default profile which is used is named default.\n");
    // TODO: list toggle output array

    fprintf(output,
            "Sound profiles:\n");
    const char* indent = "  ";
    for (int i = 0; i < SOUND_PROFILES_SIZE; ++i) {
        print_profile(SOUND_PROFILES[i], indent, output);
    }
    if (USE_SEMAPHORE)
        fprintf(output,
            "Using semaphore named '%s' to prevent concurrent volume "
            "modification.\n", SEMAPHORE_NAME);
}


/* Print volume profile info */
void print_profile(
        struct sound_profile const* profile,
        char const* indent,
        FILE* output)
{
    //snd_mixer_selem_get_name()
    fprintf(output,
            "%sName: %s\n"
            "%s%sMixer element name: %s\n"
            "%s%sVolume control mixer element name: %s\n"
            "%s%sDefault volume: %i\n"
            "%s%sSoft limit volume: %i\n"
            "%s%sSet default volume: %i\n"
            "%s%sVolume type to use: %s\n"
            "%s%sConfirm soft volume limit exceeding: %i\n",
            indent,
            profile->profile_name,
            indent, indent,
            profile->mixer_element_name,
            indent, indent,
            (profile->volume_cntrl_mixer_element_name ?
            profile->volume_cntrl_mixer_element_name : "Same as mixer element."),
            indent, indent,
            profile->default_volume,
            indent, indent,
            profile->soft_limit_volume,
            indent, indent,
            profile->set_default_volume,
            indent, indent,
            Volume_type_to_str[profile->volume_type],
            indent, indent,
            profile->confirm_exceeding_volume_limit
           );
}


/* Gets the current sound profile in use */
struct sound_profile* get_current_sound_profile()
{
    struct sound_profile* current = NULL;
    for (int i = 0; i < SOUND_PROFILES_SIZE; ++i) {
        // Skip sound profiles which have not been successfully installed.
        if (!SOUND_PROFILES[i]->init_ok) {
            continue;
        }

        snd_mixer_elem_t* e = SOUND_PROFILES[i]->mixer_element;
        if (snd_mixer_selem_has_playback_switch(e) &&
                is_mixer_elem_playback_switch_on(e)) {
            if (!current || (
                        strcmp(SOUND_PROFILES[i]->volume_cntrl_mixer_element_name, SOUND_PROFILES[i]->mixer_element_name) != 0 &&
                        is_mixer_elem_playback_switch_on(SOUND_PROFILES[i]->volume_cntrl_mixer_element)))
                    current = SOUND_PROFILES[i];
        }
    }

    assert(current);
    return current;
}


/* Gets target sound profile from TOGGLE_SOUND_PROFILES array */
struct sound_profile* get_target_sound_profile(struct sound_profile* current)
{
    struct sound_profile* target = NULL;
    for (int i = 0; i < TOGGLE_SOUND_PROFILES_SIZE; ++i) {
        snd_mixer_elem_t* e = TOGGLE_SOUND_PROFILES[i]->mixer_element;
        if (strcasecmp(snd_mixer_selem_get_name(current->mixer_element),
                snd_mixer_selem_get_name(e)) == 0) {
            target = i+1 < TOGGLE_SOUND_PROFILES_SIZE ? TOGGLE_SOUND_PROFILES[i+1] : TOGGLE_SOUND_PROFILES[0];
        }
    }

    assert(target);
    return target;
}


/* Gets mixer front panels switch value (on/off).
 * Returns true for "on" and false for "off". */
bool get_mixer_front_panel_switch()
{
    return FRONT_PANEL.init_ok ?
        is_mixer_elem_playback_switch_on(FRONT_PANEL.mixer_element) : false;
}
