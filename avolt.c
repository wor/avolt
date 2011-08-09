/* © 2010-2011 Esa S. Määttä <esa maatta at iki fi>
 * See LICENSE file for license details. */

/* Simple program to set/get/toggle alsa (Master) volume.
 *
 * TODO: save current volume and restore it if front panel toggling
 * fails.
 * TODO: check that front panel volume toggling works. 0<-->front panel default.
 * */

/* compile with:
 * [gcc|clang] $(pkg-config --cflags --libs alsa) -std=c99 avolt.c -o avolt
 */

/* TODO: check const correctness */

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>   /* access */
#include <limits.h>
#include <stdbool.h>

/* For semaphores to prevent swamping alsa with multiple calls */
#include <fcntl.h>    /* Defines O_* constants */
#include <semaphore.h>

#include "avolt.conf"
#include "wutil.h"

/* Command line options */
struct cmd_options
{
    bool set_default_vol; // Set the default volume
    int new_vol; // Set volume to this
    unsigned int toggle_vol; // Toggle volume 0 <-> default_toggle_vol
    bool toggle_output; // Toggle output
    bool inc; // Do we increase volume
    int verbose_level; // Verbosity level
};


/* Function declarations */
static void get_vol(snd_mixer_elem_t* elem, long int* vol);
static void get_vol_0_100(
        snd_mixer_elem_t* elem,
        long int const* const min,
        long int const* const max,
        long int* percent_vol);
static snd_mixer_elem_t* get_elem(snd_mixer_t* handle, char const* name);
static snd_mixer_t* get_handle(void);
static void change_range(
        long int* num,
        int const r_f_min,
        int const r_f_max,
        int const r_t_min,
        int const r_t_max,
        bool relative);
static void set_vol(
        snd_mixer_elem_t* elem,
        long int new_vol,
        bool const change_range);
static void toggle_volume(
        struct sound_profile* sp,
        long int const new_vol,
        long int const min,
        bool change_to_native_range);
static void get_vol_from_arg(const char* arg, int* new_vol, bool* inc);
bool check_semaphore(sem_t** sem);
static bool read_cmd_line_options(
        const int argc,
        const char** argv,
        struct cmd_options* cmd_opt);
static bool get_mixer_front_panel_switch();
static void print_profile(
        struct sound_profile const* profile,
        char const* indent,
        FILE* output);
static void print_config(FILE* output);
static void init_sound_profiles(snd_mixer_t* handle);


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


/* Gets mixer volume without changing range */
void get_vol(snd_mixer_elem_t* elem, long int* vol)
{
    long int a, b;
    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &a);
    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, &b);

    *vol = a >= b ? a : b;
}


/* get volume in % (0-100) range */
void get_vol_0_100(
        snd_mixer_elem_t* elem,
        long int const* const min,
        long int const* const max,
        long int* percent_vol)
{
        get_vol(elem, percent_vol);

        if (!min || !max) {
            long int min, max;
            snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
            change_range(percent_vol, min, max, 0, 100, false);
        } else {
            change_range(percent_vol, *min, *max, 0, 100, false);
        }
}


/* set volume as in range % (0-100) or in native range if change_range is false */
void set_vol(snd_mixer_elem_t* elem, long int new_vol, bool const change_range)
{
    int err = 0;
    long int min, max;

    pd("set_vol: new_vol: %i, change_range: %i\n", new_vol, change_range);
    /* check input range */
    if (change_range) {
        if (new_vol < 0) {
            new_vol = 0;
        } else if (new_vol > 100) {
            fprintf(stderr, "avolt ERROR: max volume exceeded > 100: %li\n", new_vol);
            return;
        }
    } else {
        err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        if (err < 0) {
            return;
        }
        if (new_vol > max || new_vol < min) {
            fprintf(stderr, "avolt ERROR: new volume (%li) was not in range: %li <--> %li\n", new_vol, min, max);
            return;
        }
    }

    if (change_range) {
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        new_vol = min + (max - min) * new_vol / 100;
    }
    err = snd_mixer_selem_set_playback_volume_all(elem, new_vol);
    if (err != 0) {
        fprintf(stderr, "avolt ERROR: snd mixer set playback volume failed.\n");
        return;
    }
}


/* changes range, from range -> to range
 * relative: if num is increase or decrease relative to r_f_min.
 * TODO: if change is made to smaller range, round to resolution borders */
void change_range(
        long int* num,
        int const r_f_min,
        int const r_f_max,
        int const r_t_min,
        int const r_t_max,
        bool relative)
{
    /* Check that given number is in given from range */
    bool was_negative = false;
    if (*num < 0 && relative) {
        was_negative = true;
        *num = -*num;
    }
    assert((*num >= r_f_min && *num <= r_f_max) || (relative) || "Not in from range!");
    pd("change_range: [%i, %i] -> [%i, %i]\n", r_f_min, r_f_max, r_t_min,
            r_t_max);

    // shift
    *num = *num - r_f_min;

    // get multiplier
    float mul = (float)(r_t_max - r_t_min)/(float)(r_f_max - r_f_min);

    // multiply and shift to the new range
    float f = ((float)*num * mul) + r_t_min;
    *num = (int)f;

    if (relative) {
        *num = *num - r_t_min;
        if (was_negative) {
            *num = -*num;
        }
    }
    assert(*num >= r_t_min && *num <= r_t_max || (relative) || "Not in from range!");
}


/* Checks semaphore preventing swamping alsa with multiple avolt instances */
bool check_semaphore(sem_t** sem)
{
    if (!*sem) {
        /* Note: the final permission depend on the umask (open(2)) */
        *sem = sem_open("avolt", O_CREAT, 0660, 1);
        if (*sem == SEM_FAILED) {
            fprintf(stderr, "Avolt ERROR: Semaphore opening failed.\n");
            fprintf(stderr, "%s\n", strerror(errno));
            return false;
        }
        if (sem_wait(*sem) == -1) {
            fprintf(stderr, "Avolt ERROR: Semaphore waiting (decrementing) failed.\n");
            fprintf(stderr, "%s\n", strerror(errno));
            return false;
        }
    }
    else {
        if (sem_post(*sem) == -1) {
            fprintf(stderr, "Avolt ERROR: Semaphore posting (incrementing) failed.\n");
            fprintf(stderr, "%s\n", strerror(errno));
            return false;
        }
        sem_close(*sem);
    }

    return true;
}


/* volume toggler between 0 <--> element default volume */
void toggle_volume(
        struct sound_profile* sp,
        long int const new_vol,
        long int const min, // min volume in native range
        bool change_to_native_range)
{
    long int current_vol;
    get_vol(sp->volume_cntrl_mixer_element, &current_vol);
    if (current_vol == min) {
        if (new_vol > 0 && new_vol != INT_MAX)
            set_vol(sp->volume_cntrl_mixer_element, new_vol, change_to_native_range);
        else
            set_vol(sp->volume_cntrl_mixer_element, sp->default_volume, true);
    }
    else {
        set_vol(sp->volume_cntrl_mixer_element, 0, true);
    }
    return;
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
            profile->confirm_exeeding_volume_limit
           );
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
            "Use semaphore named '%s' to enable concurrent volume "
            "setting preventing.\n", SEMAPHORE_NAME);
}


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
        } else {
            get_vol_from_arg(argv[i], &cmd_opt->new_vol, &cmd_opt->inc);
            if (strcmp(argv[i], "0") != 0 &&
                    !(cmd_opt->new_vol != 0
                        && cmd_opt->new_vol != INT_MAX
                        && cmd_opt->new_vol != INT_MIN)) {
                fprintf(stderr, "avolt - v%s: %s %s\n",
                        VERSION, argv[0], input_help);
                //snd_mixer_t* handle = get_handle();
                //init_sound_profiles(handle);
                print_config(stderr);
                return false;
            }
        }
    }
    return true;
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


/* Sets new volume, expects new_vol to be within [0,100] range. */
bool set_new_volume(
        struct sound_profile* sp,
        long int new_vol,
        bool relative_inc,
        bool set_default_vol,
        bool toggle_vol)
{
    /* XXX: Checking new_vol limits */
    if (relative_inc) {
        if (new_vol < 0 || new_vol > 100) {
            fprintf(stderr, "Cannot set volume which is not in range [0,100]: %li\n", new_vol);
            return false;
        }
    } else if ((new_vol < -100 || new_vol > 100) && !set_default_vol) {
        fprintf(stderr, "Cannot set volume which is not in range [-100,100]: %li\n", new_vol);
        return false;
    }
    /* **************************** */

    sem_t *sem = NULL; /* Semaphore which is used if USE_SEMAPHORE is true */
    if (USE_SEMAPHORE && !check_semaphore(&sem)) return false;

    /* Get given profile volume range */
    long int min, max;
    snd_mixer_selem_get_playback_volume_range(sp->volume_cntrl_mixer_element, &min, &max);

    /* Change new volume to native range */
    pd("set_new_volume: new vol [-100,100] or relative: %li\n", new_vol);

    if (set_default_vol) {
        // Set default volume, default volume is in [0-100] range
        const bool change_range = true;
        set_vol(sp->volume_cntrl_mixer_element, sp->default_volume, change_range);
    } else if (toggle_vol) {
        // toggle volume
        change_range(&new_vol, 0, 100, min, max, false);
        const bool change_range = false;
        toggle_volume(sp, new_vol, min, change_range);
    } else {
        /* Change absolute and relative volumes */

        /* First check if relative volume */
        if (relative_inc || new_vol < 0) {
            pd("Relative vol change...\n");
            if (new_vol != 0) {
                long int current_vol = -1;
                pd("Changing new_vol %li range to [%i, %i]\n", new_vol, min,
                        max);
                if (new_vol > 0) {
                    change_range(&new_vol, 0, 100, min, max, true);
                } else {
                    change_range(&new_vol, 0, 100, min, max, true);
                }
                pd("After chaning range new_vol: %li\n", new_vol);

                const bool change_range = false;
                get_vol(sp->volume_cntrl_mixer_element, &current_vol);
                //get_vol_0_100(sp->volume_cntrl_mixer_element, &min, &max, &current_vol);
                set_vol(sp->volume_cntrl_mixer_element, current_vol + new_vol, change_range);
            }
        } else {
            const bool change_range = true;
            set_vol(sp->volume_cntrl_mixer_element, new_vol, change_range);
        }
    }

    if (USE_SEMAPHORE && !check_semaphore(&sem)) return false;
    return true;
}

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
    //snd_mixer_selem_get_playback_volume_range(current_sp->volume_cntrl_mixer_element, &min, &max);

    /* First do possible output profile change */
    if (cmd_opt.toggle_output) {
        pd("Toggling the output.\n");

        struct sound_profile* target_sp = get_target_sound_profile(current_sp);

        long int current_percent_vol = -1;
        get_vol_0_100(current_sp->volume_cntrl_mixer_element, NULL, NULL, &current_percent_vol);

        /* Check if default volume is to be set */
        if (target_sp->set_default_volume &&
                cmd_opt.new_vol == INT_MAX &&
                target_sp->default_volume != current_percent_vol) {
            pd("Setting the default volume.\n");
            cmd_opt.new_vol = target_sp->default_volume;
        }

        if (cmd_opt.new_vol == INT_MAX &&
                target_sp->default_volume != current_percent_vol &&
                !target_sp->set_default_volume) {
            int adjustment = target_sp->default_volume - current_sp->default_volume;
            pd("Setting relative volume: adjustment %i, to def %i, from def %i.\n", adjustment,
                    target_sp->default_volume, current_sp->default_volume);
            if (adjustment > 0) cmd_opt.inc = true;
            if (adjustment != 0) cmd_opt.new_vol = adjustment;
        }

        /* Check volume limit if setting new volume */
        else if (target_sp->confirm_exeeding_volume_limit &&
                cmd_opt.new_vol > target_sp->soft_limit_volume) {
            printf("Are you sure you want to set the main volume to %i? [N/y]: ",
                    cmd_opt.new_vol);
            if (fgetc(stdin) != 'y')
                cmd_opt.new_vol = target_sp->default_volume;
        }

        /* Set volume now if early setting needed to avoid volume spikes */
        if (current_percent_vol > cmd_opt.new_vol &&
                current_sp->volume_cntrl_mixer_element == target_sp->volume_cntrl_mixer_element) {

            printf("DEBUG: PRE setting volume: %i\n", cmd_opt.new_vol);
            // TODO: adjustment volume calculation wrong!
            bool ret = set_new_volume(
                    target_sp,
                    cmd_opt.new_vol,
                    cmd_opt.inc,
                    cmd_opt.set_default_vol,
                    cmd_opt.toggle_vol);
            if (!ret) return 1;
            cmd_opt.new_vol = INT_MAX;
        }

        /* Turn on/off the outputs */
        int err;
        // Check if target_sp has a dependency with current_sp
        if (current_sp->mixer_element != target_sp->volume_cntrl_mixer_element) {
            // If not switch current_sp off
            printf("DEBUG: switching off element: %s\n", current_sp->mixer_element_name);
            err = snd_mixer_selem_set_playback_switch_all(current_sp->mixer_element, false);
            if (err) printf("Error occured when toggling off current_sp mixer_element named: %s\n", current_sp->mixer_element_name);
        }

        // Switch target's mixer element on
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
        if (cmd_opt.new_vol == INT_MAX && !cmd_opt.toggle_vol) return err;
    }

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
                cmd_opt.toggle_vol);
        if (!ret) return 1;
    } else {

        /* default action: get % volumes */
        long int percent_vol = 0;
        /* Get given profile volume range */
        long int min, max;
        snd_mixer_selem_get_playback_volume_range(current_sp->volume_cntrl_mixer_element, &min, &max);

        get_vol(current_sp->volume_cntrl_mixer_element, &percent_vol);
        change_range(&percent_vol, min, max, 0, 100, false);

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

