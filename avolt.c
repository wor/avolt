/* © 2010-2011 Esa S. Määttä <esa maatta at iki fi>
 * See LICENSE file for license details. */

/* Simple program to set/get/toggle alsa (Master) volume.
 *
 * XXX: Now there's a issue that received alsa volumes both seem to be on
 * logarithmic scale, (the db and the normal playback volume).
 * So when setting volume you set the volume on 0-100 as percentage of the
 * logarithmic volume. Meaning the perceived volume level increases faster on
 * the high levels than the low.
 *
 * TODO: save current volume and restore it if front panel toggling
 * fails.
 * TODO: check that front panel volume toggling works. 0<-->front panel default.
 * */

/* compile with:
 * [gcc|clang] $(pkg-config --cflags --libs alsa) -lm -std=c99 avolt.c -o avolt
 */

// TODO: now only master volume is set.
/* TODO: check const correctness */

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>   /* access */
#include <limits.h>   /* INT_MAX and so on */
#include <stdbool.h>

#include "cmdline_options.h"
#include "volume_change.h"
#include "avolt.conf.h"
#include "avolt.conf"
#include "wutil.h" // TODO: rename to util.h

/* Function declarations */
// TODO: move alsa stuff to alsa_utils.h
static snd_mixer_elem_t* get_elem(snd_mixer_t* handle, char const* name);
static snd_mixer_t* get_handle(void);
static bool get_mixer_front_panel_switch();
static bool is_mixer_elem_playback_switch_on(snd_mixer_elem_t* elem);
static struct sound_profile* get_target_sound_profile(
        struct sound_profile* current);
static void init_sound_profiles(snd_mixer_t* handle);
//static void list_mixer_elements(snd_mixer_t* handle); DEBUG func


/* Get alsa handle */
snd_mixer_t* get_handle()
{
    snd_mixer_t* handle = NULL;

    int ret_val = snd_mixer_open(&handle, 0);
    assert(ret_val >= 0);

    snd_mixer_attach(handle, "default");
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    return handle;
}


/* Get mixer elem with given name from the handle */
snd_mixer_elem_t* get_elem(snd_mixer_t* handle, char const* name)
{
    snd_mixer_elem_t* elem = NULL;

    /* get snd_mixer_elem_t pointer, corresponding DEFAULT.volume_cntrl_mixer_element_name */
    snd_mixer_elem_t* var = snd_mixer_first_elem(handle);
    while (var != NULL) {
        if (strcasecmp(name, snd_mixer_selem_get_name(var)) == 0) {
            elem = var;
            break;
        }
        var = snd_mixer_elem_next(var);
    }

    assert(elem);
    return elem;
}


bool is_mixer_elem_playback_switch_on(snd_mixer_elem_t* elem)
{
    /* XXX: Could assert that snd_mixer_selem_has_playback_switch(elem) */
    int temp_switch = -1;
    snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &temp_switch);
    return temp_switch;
}


/* Gets mixer front panels switch value (on/off).
 * Returns true for "on" and false for "off". */
bool get_mixer_front_panel_switch()
{
    return is_mixer_elem_playback_switch_on(FRONT_PANEL.mixer_element);
}


/* Initializes all sound profiles from SOUND_PROFILES array */
void init_sound_profiles(snd_mixer_t* handle)
{
    for (int i = 0; i < SOUND_PROFILES_SIZE; ++i) {
        SOUND_PROFILES[i]->mixer_element = get_elem(handle, SOUND_PROFILES[i]->mixer_element_name);
        if (SOUND_PROFILES[i]->volume_cntrl_mixer_element_name)
            SOUND_PROFILES[i]->volume_cntrl_mixer_element = get_elem(handle, SOUND_PROFILES[i]->volume_cntrl_mixer_element_name);
        else {
            SOUND_PROFILES[i]->volume_cntrl_mixer_element_name = SOUND_PROFILES[i]->mixer_element_name;
            SOUND_PROFILES[i]->volume_cntrl_mixer_element = SOUND_PROFILES[i]->mixer_element;
        }
    }
}


/* Get's current sound profile in use */
struct sound_profile* get_current_sound_profile()
{
    struct sound_profile* current = NULL;
    for (int i = 0; i < SOUND_PROFILES_SIZE; ++i) {
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



/* Print info about existing mixer elements */
/*
void list_mixer_elements(snd_mixer_t* handle)
{
    snd_mixer_elem_t* elem = snd_mixer_first_elem(handle);
    for (int i = 1; elem != NULL; ++i) {
        printf("%i. Element name: %s\n", i, snd_mixer_selem_get_name(elem));
        if (snd_mixer_selem_has_playback_switch(elem))
            printf("  Element has playback switch.\n");
        elem = snd_mixer_elem_next(elem);
    }
}
*/


/*****************************************************************************
 * Main function
 * */
int main(const int argc, const char* argv[])
{
    /* Init command line options instance */
    struct cmd_options cmd_opt = {
        .set_default_vol = false,
        .new_vol = INT_MAX,
        .toggle_vol = 0,
        .toggle_output = false,
        .inc = false,
        .verbose_level = 0
    };


    /* Read parameters to cmd_opt */
    if (!read_cmd_line_options(argc, argv, &cmd_opt)) return 1;

    /* Create needed variables */
    snd_mixer_t* handle = get_handle();
    init_sound_profiles(handle);

    /* list_mixer_elements(handle); // DEBUG */

    /* First we must determine witch profile is "on" */
    struct sound_profile* current_sp = get_current_sound_profile();

    /* First do possible output profile change */
    if (cmd_opt.toggle_output) {
        PD_M("Toggling the output.\n");

        struct sound_profile* target_sp = get_target_sound_profile(current_sp);

        // For now this code doesn't work if profiles and given volume types
        // differ // TOOD: fix sometime
        assert(VOLUME_TYPE == target_sp->volume_type);

        long int current_vol = -1;
        get_vol(current_sp->volume_cntrl_mixer_element, target_sp->volume_type, &current_vol);


        /* Check if no new volume given */
        if (cmd_opt.new_vol == INT_MAX) {
            /* Check if default volume is to be set */
            if (target_sp->set_default_volume) {
                PD_M("Setting the default volume.\n");
                cmd_opt.new_vol = target_sp->default_volume;
            }
            else { // Else no volume change
                cmd_opt.new_vol = current_vol;
            }
        }

        assert(cmd_opt.new_vol != INT_MAX);

        /* Check volume limit if setting new volume */
        if (target_sp->confirm_exceeding_volume_limit &&
                cmd_opt.new_vol > target_sp->soft_limit_volume) {
            printf("Are you sure you want to set the main volume to %i? [N/y]: ",
                    cmd_opt.new_vol);
            if (fgetc(stdin) != 'y')
                cmd_opt.new_vol = target_sp->default_volume;
        }

        /* If setting to a lower volume set volume before switching element to avoid volume spikes */
        //if (!cmd_opt.inc && current_sp->volume_cntrl_mixer_element == target_sp->volume_cntrl_mixer_element && current_vol > cmd_opt.new_vol) {
        if (current_sp->volume_cntrl_mixer_element == target_sp->volume_cntrl_mixer_element &&
                cmd_opt.new_vol != current_vol) {
            //PD_M("PRE setting volume to zero during output element switch: %i\n", cmd_opt.new_vol);
            PD_M("PRE setting volume to zero during output element switch.\n");
            bool ret = set_new_volume(
                    target_sp,
                    0,
                    false,
                    cmd_opt.set_default_vol,
                    false,
                    USE_SEMAPHORE,
                    VOLUME_TYPE);
            if (!ret) return 1;

            // If new_vol is relative we need to calculate new new_vol value
            if (cmd_opt.new_vol < 0 || cmd_opt.inc) {
                cmd_opt.new_vol += current_vol;
            }
            current_vol = 0;
        }

        /* Turn on/off the outputs */
        int err;
        /* Check if target_sp has a dependency with current_sp */
        if (current_sp->mixer_element != target_sp->volume_cntrl_mixer_element) {
            /* If not switch current_sp off */
            PD_M("switching off element: %s\n", current_sp->mixer_element_name);
            err = snd_mixer_selem_set_playback_switch_all(current_sp->mixer_element, false);
            if (err) printf("Error occured when toggling off current_sp mixer_element named: %s\n", current_sp->mixer_element_name);
        }

        /* Switch target's mixer element on */
        err = snd_mixer_selem_set_playback_switch_all(target_sp->mixer_element, true);
        if (err) printf("Error occured when toggling off current_sp mixer_element named: %s\n", current_sp->mixer_element_name);

        /* Print verbose info if no error occured while toggling playback switch */
        if (!err) {
            if (cmd_opt.verbose_level > 0)
                printf("Current profile: %s\n", target_sp->mixer_element_name);
            if (cmd_opt.verbose_level > 1) {
                if (get_mixer_front_panel_switch())
                    print_profile(&FRONT_PANEL, "", stdout);
                else
                    print_profile(&DEFAULT, "", stdout);
            }
        }

        if (err) { printf("Errors occured while on/offing the output.\n"); return err; }

        /* Exit if nothing else to do */
        if ( (cmd_opt.new_vol == INT_MAX && !cmd_opt.toggle_vol) ||
                (current_vol == cmd_opt.new_vol &&
                 current_sp->volume_cntrl_mixer_element == target_sp->volume_cntrl_mixer_element) ) return err;
    } // End of toggle_output

    /* Second do possible volume change */
    /* If new volume given or toggle volume, or set default volume */
    if (cmd_opt.new_vol != INT_MAX ||
            cmd_opt.toggle_vol ||
            cmd_opt.set_default_vol) {

        bool ret = set_new_volume(
                current_sp,
                cmd_opt.new_vol,
                cmd_opt.inc,
                cmd_opt.set_default_vol,
                cmd_opt.toggle_vol,
                USE_SEMAPHORE,
                VOLUME_TYPE);
        if (!ret) return 1;
    } else {

        /* default action: get % volumes */
        long int percent_vol = 0;
        /* Get given profile volume range */
        long int min, max;
        snd_mixer_selem_get_playback_volume_range(current_sp->volume_cntrl_mixer_element, &min, &max);
        PD_M("Current volume range is [%li, %li]\n", min, max);

        get_vol(current_sp->volume_cntrl_mixer_element, VOLUME_TYPE, &percent_vol);
        PD_M("Got volume from mixer element: %li\n", percent_vol);

        printf("%li", percent_vol);
        if (cmd_opt.verbose_level > 0)
            printf(" Front panel: %s",
                    get_mixer_front_panel_switch() ? "on" : "off");
        printf("\n");
        if (cmd_opt.verbose_level > 1) {
            if (get_mixer_front_panel_switch())
                print_profile(&FRONT_PANEL, "", stdout);
            else
                print_profile(&DEFAULT, "", stdout);
        }
    }

    return 0;
}

