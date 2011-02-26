/* © 2010-2011 Esa S. Määttä <esa maatta at iki fi>
 * See LICENSE file for license details. */

/* Simple program to set/get/toggle alsa Master volume */

/* compile with:
 * [gcc|clang] $(pkg-config --cflags --libs alsa) -std=c99 get_master_vol.c -o avolt
 */

/* TODO: check const correctness */

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h> /* access */
#include <limits.h>
#include <stdbool.h>

#define VERSION "0.2.4"
#define DEFAULT_VOL 32
#define LOCK_FILE "/tmp/.avolt.lock"

/* When toggling front panel off, set volume to default if no new volume given.
 * */
const bool SET_DEFAULT_VOL_WHEN_FP_OFF = true;

/* Use lock file to prevent concurrent calls. */
const bool USE_LOCK_FILE = true;

/* Function declarations. */
inline void get_vol(snd_mixer_elem_t* elem, long int* vol);
inline void get_vol_0_100(
        snd_mixer_elem_t* elem,
        long int const* const min,
        long int const* const max,
        long int* percent_vol);
snd_mixer_elem_t* get_elem(snd_mixer_t* handle, char const* name);
snd_mixer_t* get_handle(void);
void change_range(
        long int* num,
        int const r_f_min,
        int const r_f_max,
        int const r_t_min,
        int const r_t_max);
void set_vol(snd_mixer_elem_t* elem, long int new_vol, bool const change_range);
void toggle_volume(
        snd_mixer_elem_t* elem,
        long int const new_vol,
        long int const min);
void get_vol_from_arg(const char* arg, int* new_vol, bool* inc);
inline void delete_lock_file(void);
inline int check_lock_file(void);


/* get alsa handle */
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


/* get mixer elem with given name from the handle */
snd_mixer_elem_t* get_elem(snd_mixer_t* handle, char const* name)
{
    snd_mixer_elem_t* elem = NULL;

    /* get snd_mixer_elem_t pointer, corresponding Master */
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


/* gets mixer volume without changing range */
inline void get_vol(snd_mixer_elem_t* elem, long int* vol)
{
    long int a, b;
    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &a);
    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, &b);

    *vol = a >= b ? a : b;
}


/* get volume in % (0-100) range */
inline void get_vol_0_100(
        snd_mixer_elem_t* elem,
        long int const* const min,
        long int const* const max,
        long int* percent_vol)
{
        get_vol(elem, percent_vol);
        change_range(percent_vol, *min, *max, 0, 100);
}


/* set volume as in range % (0-100) or in native range if change_range is false */
void set_vol(snd_mixer_elem_t* elem, long int new_vol, bool const change_range)
{
    int err = 0;
    long int min, max;

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
 * TODO: if change is made to smaller range, round to resolution borders */
void change_range(
        long int* num,
        int const r_f_min,
        int const r_f_max,
        int const r_t_min,
        int const r_t_max)
{
    // shift
    *num = *num - r_f_min;

    // get multiplier
    float mul = (float)(r_t_max - r_t_min)/(float)(r_f_max - r_f_min);

    // multiply and shift to the new range
    float f = ((float)*num * mul) + r_t_min;
    *num = (int)f;
}


/* checks if lock file exists and exits if it does */
inline int check_lock_file(void)
{
    if(access(LOCK_FILE, F_OK) == 0) {
        return 0;
    } else {
        fclose(fopen(LOCK_FILE, "w"));
        return -1;
    }
}


/* volume toggler between 0 <--> DEFAULT_VOL */
void toggle_volume(
        snd_mixer_elem_t* elem,
        long int const new_vol,
        long int const min)
{
    long int current_vol;
    get_vol(elem, &current_vol);
    if (current_vol == min) {
        if (new_vol > 0 && new_vol != INT_MAX)
            set_vol(elem, new_vol, true);
        else
            set_vol(elem, DEFAULT_VOL, true);
    }
    else {
        set_vol(elem, 0, true);
    }
    return;
}


/* deletes lock file */
inline void delete_lock_file(void)
{
    remove(LOCK_FILE);
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


/*****************************************************************************
 * Main function
 * */
int main(const int argc, const char* argv[])
{
    const char* input_help = "[[-s] [+|-]<volume>]] [-t] [-tf]"
        "\n\n"
        "Option help:\n"
        "s:\tSet volume.\n"
        "t:\tToggle volume.\n"
        "tf:\tToggle front panel.\n";
    int new_vol = INT_MAX; // Set volume to this
    unsigned int toggle = 0; // Toggle volume 0 <-> default_toggle_vol
    bool toggle_fp = false; // Toggle front panel
    bool inc = false; // Do we increase volume

    /* read parameters */
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-s") == 0) && (i+1 < argc)) {
            get_vol_from_arg(argv[++i], &new_vol, &inc);
        } else if (strcmp(argv[i], "-t") == 0) {
            toggle = 1;
        } else if (strcmp(argv[i], "-tf") == 0) {
            toggle_fp = true;
        } else {
            get_vol_from_arg(argv[i], &new_vol, &inc);
            if (strcmp(argv[i], "0") != 0 &&
                    !(new_vol != 0 && new_vol != INT_MAX && new_vol != INT_MIN)) {
                fprintf(stderr, "avolt - v" VERSION ": %s %s\n",
                        argv[0], input_help);
                return 1;
            }
        }
    }

    /* current volume range */
    long int min, max;

    snd_mixer_t* handle = get_handle();
    snd_mixer_elem_t* elem = get_elem(handle, "Master");
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);

    /* current % volume */
    long int percent_vol = -1;

    /* Toggle the front panel */
    if (toggle_fp) {
        snd_mixer_elem_t* front_panel_elem = get_elem(handle, "Front Panel");
        assert(snd_mixer_selem_has_playback_switch(front_panel_elem) == 1);

        int switch_value = -1;
        snd_mixer_selem_get_playback_switch(front_panel_elem, SND_MIXER_SCHN_FRONT_LEFT, &switch_value);
        int err = snd_mixer_selem_set_playback_switch_all(front_panel_elem, !switch_value);

        /* Set default volume if no new volume given and toggled off front
         * panel. Also only if current volume higher than default volume.
         * */
        if (SET_DEFAULT_VOL_WHEN_FP_OFF &&
                new_vol == INT_MAX &&
                switch_value) {
            get_vol_0_100(elem, &min, &max, &percent_vol);
            if (percent_vol > DEFAULT_VOL) new_vol = DEFAULT_VOL;
        }

        /* Exit if nothing else to do */
        if (new_vol == INT_MAX && !toggle) return err;
    }

    /* If new volume given or toggle volume */
    if (new_vol != INT_MAX || toggle) {
        if (USE_LOCK_FILE && check_lock_file() == 0) return 0;

        if (toggle) {
            toggle_volume(elem, new_vol, min);
        } else {
            /* change absolut and relative volumes */
            /* first check if relative volume */
            if (inc || new_vol < 0) {
                if (new_vol != 0) {
                    long int current_vol = -1;
                    get_vol(elem, &current_vol);
                    set_vol(elem, current_vol + new_vol, false);
                }
            } else {
                set_vol(elem, new_vol, true);
            }
        }

        if (USE_LOCK_FILE) delete_lock_file();
    } else {
        /* default action: get % volumes */
        if (percent_vol < 0) {
            get_vol(elem, &percent_vol);
            change_range(&percent_vol, min, max, 0, 100);
        }
        printf("%li\n", percent_vol);
    }

    return 0;
}

