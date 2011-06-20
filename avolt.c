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

/* Command line options */
struct cmd_options
{
    int new_vol; // Set volume to this
    unsigned int toggle; // Toggle volume 0 <-> default_toggle_vol
    bool toggle_fp; // Toggle front panel
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
        int const r_t_max);
static void set_vol(
        snd_mixer_elem_t* elem,
        long int new_vol,
        bool const change_range);
static void toggle_volume(
        struct mixer_element_conf* element_conf,
        long int const new_vol,
        long int const min);
static void get_vol_from_arg(const char* arg, int* new_vol, bool* inc);
bool check_semaphore(sem_t** sem);
static bool read_cmd_line_options(
        const int argc,
        const char** argv,
        struct cmd_options* cmd_opt);
static bool get_mixer_front_panel_switch();


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

    /* get snd_mixer_elem_t pointer, corresponding MASTER.element_name */
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
        struct mixer_element_conf* element_conf,
        long int const new_vol,
        long int const min)
{
    long int current_vol;
    get_vol(MASTER.element, &current_vol);
    if (current_vol == min) {
        if (new_vol > 0 && new_vol != INT_MAX)
            set_vol(MASTER.element, new_vol, true);
        else
            set_vol(MASTER.element, element_conf->default_volume, true);
    }
    else {
        set_vol(MASTER.element, 0, true);
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


/* Print information to given FILE* about statically set config options. */
void print_config(FILE* output)
{
    fprintf(output, "Static option help:\n");
    fprintf(output,
            "The alsa element to control by default is: %s\n",
            MASTER.element_name);
    fprintf(output,
            "The alsa front panel element to control by default is: %s\n",
            FRONT_PANEL.element_name);

    fprintf(output,
            "Mixer element settings:\n");
    const int mixer_elements_size = sizeof(MIXER_ELEMENTS) /
        sizeof(struct mixer_element_conf*);
    const char* indent = "  ";
    for (int i = 0; i < mixer_elements_size; ++i) {
        fprintf(output,
                "%sName: %s\n"
                "%s%sDefault volume: %i\n"
                "%s%sSoft limit volume: %i\n"
                "%s%sSet default volume: %i\n"
                "%s%sConfirm soft volume limit exceeding: %i\n",
                indent,
                MIXER_ELEMENTS[i]->element_name,
                indent, indent,
                MIXER_ELEMENTS[i]->default_volume,
                indent, indent,
                MIXER_ELEMENTS[i]->soft_limit_volume,
                indent, indent,
                MIXER_ELEMENTS[i]->set_default_volume,
                indent, indent,
                MIXER_ELEMENTS[i]->confirm_exeeding_volume_limit
                );
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
    const char* input_help = "[[-s] [+|-]<volume>]] [-t] [-tf] [-v]"
        "\n\n"
        "Option help:\n"
        "v:\tBe more verbose.\n"
        "s:\tSet volume.\n"
        "t:\tToggle volume.\n"
        "tf:\tToggle front panel.\n";

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-s") == 0) && (i+1 < argc)) {
            get_vol_from_arg(argv[++i], &cmd_opt->new_vol, &cmd_opt->inc);
        } else if (strcmp(argv[i], "-s") == 0) {
            // TODO: set default volume
            ;
        } else if (strcmp(argv[i], "-v") == 0) {
            cmd_opt->verbose_level++;
        } else if (strcmp(argv[i], "-t") == 0) {
            cmd_opt->toggle = 1;
        } else if (strcmp(argv[i], "-tf") == 0) {
            cmd_opt->toggle_fp = true;
        } else {
            get_vol_from_arg(argv[i], &cmd_opt->new_vol, &cmd_opt->inc);
            if (strcmp(argv[i], "0") != 0 &&
                    !(cmd_opt->new_vol != 0
                        && cmd_opt->new_vol != INT_MAX
                        && cmd_opt->new_vol != INT_MIN)) {
                fprintf(stderr, "avolt - v %s: %s %s\n",
                        VERSION, argv[0], input_help);
                print_config(stderr);
                return false;
            }
        }
    }
    return true;
}


/* Gets mixer front panels switch value (on/off).
 * Returns true for "on" and false for "off". */
bool get_mixer_front_panel_switch()
{
    /* XXX: If to be generalized check that element has playback switch:
     * snd_mixer_selem_has_playback_switch(front_panel_elem) */
    int temp_switch = -1;
    snd_mixer_selem_get_playback_switch(FRONT_PANEL.element, SND_MIXER_SCHN_FRONT_LEFT, &temp_switch);
    return temp_switch;
}


/*****************************************************************************
 * Main function
 * */
int main(const int argc, const char* argv[])
{
    /* Init command line options instance */
    struct cmd_options cmd_opt = {
        .new_vol = INT_MAX,
        .toggle = 0,
        .toggle_fp = false,
        .inc = false,
        .verbose_level = 0
    };

    /* Read parameters to cmd_opt */
    if (!read_cmd_line_options(argc, argv, &cmd_opt)) return 1;

    /* Create needed variables */
    snd_mixer_t* handle = get_handle();
    MASTER.element = get_elem(handle, MASTER.element_name);
    FRONT_PANEL.element = get_elem(handle, FRONT_PANEL.element_name);
    long int min, max; /* current volume range */
    snd_mixer_selem_get_playback_volume_range(MASTER.element, &min, &max);
    long int percent_vol = -1; /* current % volume */
    sem_t *sem = NULL; /* Semaphore which is used if USE_SEMAPHORE is true */

    /* Toggle the front panel
     * TODO: set new vol first only if toggling fp off */
    if (cmd_opt.toggle_fp) {

        bool is_switch_on = get_mixer_front_panel_switch();

        /* If toggling off the front panel */
        if (is_switch_on) {
            /* Set default volume if no new volume given and only if current
             * volume is higher than the default volume. (This is done before
             * setting the front panel off to avoid volume spike) */
            if (MASTER.set_default_volume &&
                    cmd_opt.new_vol == INT_MAX) {
                get_vol_0_100(MASTER.element, &min, &max, &percent_vol);
                if (percent_vol > MASTER.default_volume) set_vol(MASTER.element, MASTER.default_volume, true);
            }
            /* Check volume limit if setting new volume */
            else if (MASTER.confirm_exeeding_volume_limit &&
                    cmd_opt.new_vol > MASTER.soft_limit_volume) {
                printf("Are you sure you want to set the main volume to %i? [N/y]: ",
                        cmd_opt.new_vol);
                if (fgetc(stdin) != 'y')
                    cmd_opt.new_vol = INT_MAX;
            }
        }
        else {
            if (FRONT_PANEL.set_default_volume)
                set_vol(MASTER.element, FRONT_PANEL.default_volume, true);
        }

        /* Toggle the front panel */
        int err = snd_mixer_selem_set_playback_switch_all(FRONT_PANEL.element, !is_switch_on);

        /* Exit if nothing else to do */
        if (cmd_opt.new_vol == INT_MAX && !cmd_opt.toggle) return err;
    }

    /* If new volume given or toggle volume */
    if (cmd_opt.new_vol != INT_MAX || cmd_opt.toggle) {
        if (USE_SEMAPHORE && !check_semaphore(&sem)) return 0;

        if (cmd_opt.toggle) {
            toggle_volume(&MASTER, cmd_opt.new_vol, min);
        } else {
            /* change absolute and relative volumes */
            /* first check if relative volume */
            if (cmd_opt.inc || cmd_opt.new_vol < 0) {
                if (cmd_opt.new_vol != 0) {
                    long int current_vol = -1;
                    get_vol(MASTER.element, &current_vol);
                    set_vol(MASTER.element, current_vol + cmd_opt.new_vol, false);
                }
            } else {
                set_vol(MASTER.element, cmd_opt.new_vol, true);
            }
        }

        if (USE_SEMAPHORE && !check_semaphore(&sem)) return 0;
    } else {
        /* default action: get % volumes */
        if (percent_vol < 0) {
            get_vol(MASTER.element, &percent_vol);
            change_range(&percent_vol, min, max, 0, 100);
        }

        printf("%li", percent_vol);
        if (cmd_opt.verbose_level > 0)
            printf(" Front panel: %s",
                    get_mixer_front_panel_switch() ? "on" : "off");
        printf("\n");
    }

    return 0;
}

