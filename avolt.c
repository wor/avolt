/* © 2010 Esa S. Määttä <esa maatta at iki fi>
 * See LICENSE file for license details. */

/* Simple program to set/get/toggle alsa Master volume */

/* compile with:
 * [gcc|clang] $(pkg-config --cflags --libs alsa) -std=c99 get_master_vol.c -o volget
 */


#include <alsa/asoundlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h> /* access */

#define VERSION "0.2"
#define DEFAULT_VOL 32
#define LOCK_FILE "/tmp/.avolt.lock"
#define ERROR_VOL 99999
#define TRUE 0
#define FALSE -1

snd_mixer_t* get_handle(void);
snd_mixer_elem_t* get_elem(snd_mixer_t* handle);
long int get_vol(snd_mixer_elem_t* elem);
void set_vol(snd_mixer_elem_t* elem, long int new_vol, int change_range);
void change_range(long int* num, int r_f_min, int r_f_max, int r_t_min, int r_t_max);
int check_lock_file(void);
void delete_lock_file(void);
void set_vol_relative(snd_mixer_elem_t* elem, long int vol_change);

/* get alsa handle */
snd_mixer_t* get_handle() {
    snd_mixer_t* handle = NULL;

    int ret_val = snd_mixer_open(&handle, 0);
    assert(ret_val >= 0);

    snd_mixer_attach(handle, "default");
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    return handle;
}


/* get mixer elem from the handle */
snd_mixer_elem_t* get_elem(snd_mixer_t* handle) {
    snd_mixer_elem_t* elem = NULL;

    /* get snd_mixer_elem_t pointer, corresponding Master */
    snd_mixer_elem_t* var = snd_mixer_first_elem(handle);
    while (var != NULL) {
        if (strcasecmp("Master", snd_mixer_selem_get_name(var)) == 0) {
            elem = var;
            break;
        }
        var = snd_mixer_elem_next(var);
    }

    assert(elem);
    return elem;
}


/* gets mixer volume without range chaning */
long int get_vol(snd_mixer_elem_t* elem) {
    long int a, b;
    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &a);
    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, &b);

    long int louder_channel_vol = a >= b ? a : b;

    return louder_channel_vol;
}


/* set volume as in range 0-100 or native range if change_range is FALSE */
void set_vol(snd_mixer_elem_t* elem, long int new_vol, int change_range) {
    int err = 0;
    long int min, max;

    /* check input range */
    if (change_range == TRUE) {
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

    if (change_range == TRUE) {
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        new_vol = min + (max - min) * new_vol / 100;
    }
    err = snd_mixer_selem_set_playback_volume_all(elem, new_vol);
    if (err != 0) {
        fprintf(stderr, "avolt ERROR: snd mixer set playback volume failed.\n");
        return;
    }
}


/* set changes int range, from range -> to range */
void change_range(long int* num, int r_f_min, int r_f_max, int r_t_min, int r_t_max) {
    // shift
    *num = *num - r_f_min;

    // get multiplier
    float mul = (float)(r_t_max - r_t_min)/(float)(r_f_max - r_f_min);

    // multiply and shift to new range
    float f = ((float)*num * mul) + r_t_min;
    *num = (int)f;
}


/* checks if lock file exists and exits if it does */
int check_lock_file(void) {
    if(access(LOCK_FILE, F_OK) == 0) {
        return 0;
    } else {
        fclose(fopen(LOCK_FILE, "w"));
        return -1;
    }
}


/* deletes lock file */
void delete_lock_file(void) {
    remove(LOCK_FILE);
    return;
}


/*****************
 * MAIN function */
int main(int argc, char* argv[])
{
    const int default_toggle_vol = DEFAULT_VOL;
    int new_vol = ERROR_VOL; // set volume to this
    unsigned int toggle = 0; // toggle volume 0 <-> default_toggle_vol
    int inc = 0; // do we increase volume
    long int min, max;

    /* read parameters */
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-s") == 0) && (i+1 < argc)) {
            if (strncmp(argv[i+1], "+", 1) == 0) {
                inc = 1;
                new_vol = atoi(argv[i+1]+1);
            } else if (strncmp(argv[i+1], "-", 1) == 0) {
                new_vol = atoi(argv[i+1]+1)*-1;
            } else {
                new_vol = atoi(argv[i+1]);
            }
            i++;
        } else if (strcmp(argv[i], "-t") == 0) {
            toggle = 1;
        } else {
            fprintf(stderr, "avolt - v" VERSION ": %s [-s [+|-]<volume>] [-t]\n",
                    argv[0]);
            return 1;
        }
    }

    snd_mixer_t* handle = get_handle();
    snd_mixer_elem_t* elem = get_elem(handle);

    /* block which changes absolut and relative volumes */
    if (!toggle && new_vol != ERROR_VOL) {
        if (check_lock_file() == 0) {
            return 0;
        }
        /* check if relative volume */
        if (inc != 0 || new_vol < 0) {
            if (new_vol == 0) {
                return 0;
            }
            set_vol(elem, get_vol(elem) + new_vol, FALSE);
        } else {
            set_vol(elem, new_vol, TRUE);
        }
        delete_lock_file();
        return 0;
    }

    /* volume toggler between 0<-->default_toggle_vol */
    if (toggle) {
        if (get_vol(elem) == 0) {
            if (new_vol > 0)
                set_vol(elem, new_vol, TRUE);
            else
                set_vol(elem, default_toggle_vol, TRUE);
        }
        else {
            set_vol(elem, 0, FALSE);
        }
        return 0;
    }

    /* default action: get % volumes */
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    long int percent_vol = get_vol(elem);
    change_range(&percent_vol, min, max, 0, 100);
    printf("%li\n", percent_vol);

    return 0;
}

