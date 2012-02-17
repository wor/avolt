/* For semaphores to prevent swamping alsa with multiple calls */
#include <fcntl.h>    /* Defines O_* constants */
#include <semaphore.h>
#include <limits.h>   /* INT_MAX and so on */
#include <math.h>

#include "volume_change.h"
#include "volume_mapping.h"
#include "wutil.h"

// TODO: define module internal funcs here
void toggle_volume(
        struct sound_profile* sp,
        long int const new_vol,
        enum Volume_type volume_type);
void set_vol(
        snd_mixer_elem_t* elem,
        enum Volume_type volume_type,
        long int new_vol,
        int round_direction);

/* Gets mixer volume with given type, if left and right channel volume differ,
 * then gives the larger one.
 * In case of an error returns "-1". */
void get_vol(snd_mixer_elem_t* elem, enum Volume_type volume_type, long int* vol)
{
    long int l, r;
    if (volume_type == hardware) {
        // TODO: check return value (int) for errors
        snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &l);
        snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, &r);
    }
    else if (volume_type == decibels) {
        // TODO: check return value (int) for errors
        snd_mixer_selem_get_playback_dB(elem, SND_MIXER_SCHN_FRONT_LEFT, &l);
        snd_mixer_selem_get_playback_dB(elem, SND_MIXER_SCHN_FRONT_RIGHT, &r);
    }
    else if (volume_type == alsa_percentage) {
        double l_norm = get_normalized_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT);
        double r_norm = get_normalized_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT);
        PD_M("Got alsa_percentage volumes: %g, %g\n", l_norm, r_norm);
        l = lround(l_norm*100);
        r = lround(r_norm*100);
    }
    else if (volume_type == hardware_percentage) {
        long int min, max;
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &l);
        snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, &r);

        long int vol_to_percentage = l >= r ? l : r;
        change_range(&vol_to_percentage, min, max, 0, 100, false);
        *vol = vol_to_percentage;
        return;
    }
    else {
        // Error
        l = -1;
        r = -1;
    }

    *vol = l >= r ? l : r;
    PD_M("get_vol returns: %li\n", *vol);
}


/* Set volume with given volume_type.
 * round_direction: >0 to round up, <0 to round down, 0 to use default lrint
 *                  rounding direction (see fsetround(3)).*/
void set_vol(
        snd_mixer_elem_t* elem,
        enum Volume_type volume_type,
        long int new_vol,
        int round_direction)
{
    // TODO: possibly add new_vol range check
    int err = 0;

    if (volume_type == hardware) {
        err = snd_mixer_selem_set_playback_volume_all(elem, new_vol);
    }
    else if (volume_type == decibels) {
        err = snd_mixer_selem_set_playback_dB_all(elem, new_vol, round_direction);
    }
    else if (volume_type == alsa_percentage) {
        double new_vol_norm = (double) new_vol / 100;
        PD_M("Setting alsa_percentage volume: %g\n", new_vol_norm);
        err = set_normalized_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, new_vol_norm, round_direction);
        if (err == 0) {
            err = set_normalized_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, new_vol_norm, round_direction);
        }
    }
    else if (volume_type == hardware_percentage) {
        long int min, max;
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        change_range(&new_vol, 0, 100, min, max, false);

        err = snd_mixer_selem_set_playback_volume_all(elem, new_vol);
    }
    else {
        fprintf(stderr, "avolt ERROR: set_vol: Unknown volume_type '%i'.\n", volume_type);
        err = -1;
    }

    if (err != 0) {
        fprintf(stderr, "avolt ERROR: snd mixer set playback volume failed with new vol '%li' and volume type '%i'.\n", new_vol, volume_type);
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
        bool relative) // TODO: change name to num_relative_to_r_f_min
{
    /* Check that given number is in given from range */
    bool was_negative = false;
    if (*num < 0 && relative) {
        was_negative = true;
        *num = -*num;
    }
    assert((*num >= r_f_min && *num <= r_f_max) || (relative) || "Not in from range!");
    PD_M("change_range: [%i, %i] -> [%i, %i]\n", r_f_min, r_f_max, r_t_min,
            r_t_max);

    // shift
    *num = *num - r_f_min;
    PD_M("change_range: shifted given vol: %li\n", *num);

    // get multiplier
    float mul = (float)(r_t_max - r_t_min)/(float)(r_f_max - r_f_min);
    PD_M("change_range: Calculated multiplier: %f\n", mul);

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


/* Volume toggler between 0 <--> element default volume.
 * Now excepts new_vol to be in default hardware range of the sound card in use. */
void toggle_volume(
        struct sound_profile* sp,
        long int const new_vol, // If INT_MAX or negative then toggle to sound
                                // profiles default volume
        enum Volume_type volume_type)
{
    // TODO: USE "avolt" SEMAPHORE

    long int current_vol;
    get_vol(sp->volume_cntrl_mixer_element, hardware, &current_vol);
    long int min, _;
    snd_mixer_selem_get_playback_volume_range(sp->volume_cntrl_mixer_element, &min, &_);

    // If current volume is lowest possible
    if (current_vol == min) {
        if (new_vol > 0 && new_vol != INT_MAX)
            set_vol(sp->volume_cntrl_mixer_element, volume_type, new_vol, 0);
        else
            set_vol(sp->volume_cntrl_mixer_element, sp->volume_type, sp->default_volume, 0);
    }
    else {
        // Else zero current volume
        set_vol(sp->volume_cntrl_mixer_element, hardware_percentage, 0, 0);
    }
    return;
}


/* Sets new volume, expects new_vol to be within [0,100] range. */
bool set_new_volume(
        struct sound_profile* sp,
        long int new_vol,
        bool relative_inc,
        bool set_default_vol,
        bool toggle_vol,
        bool use_semaphore,
        enum Volume_type volume_type)
{
    /* XXX: Checking new_vol limits */
    if (relative_inc) {
        if (new_vol < 0 || new_vol > 100) {
            fprintf(stderr, "Cannot set volume which is not in range [0,100]: %li\n", new_vol);
            return false;
        }
    } else if ((new_vol < -100 || new_vol > 100) && !set_default_vol && !toggle_vol) {
        fprintf(stderr, "Cannot set volume which is not in range [-100,100]: %li\n", new_vol);
        return false;
    }
    /* **************************** */

    sem_t *sem = NULL; /* Semaphore which is used if USE_SEMAPHORE is true */
    if (use_semaphore && !check_semaphore(&sem)) return false;

    /* Change new volume to native range */
    PD_M("set_new_volume: new vol [-100,100], relative or toggling: %li\n", new_vol);

    if (set_default_vol) {
        // Set default volume
        PD_M("set_new_volume: setting default vol..\n");
        set_vol(sp->volume_cntrl_mixer_element, sp->volume_type, sp->default_volume, 0);
    } else if (toggle_vol) {
        // toggle volume
        PD_M("set_new_volume: toggling..\n");
        toggle_volume(sp, new_vol, volume_type);
    } else {
        /* Change absolute and relative volumes */

        /* First check if relative volume */
        if (relative_inc || new_vol < 0) {
            if (new_vol != 0) {
                PD_M("set_new_volume: Relative vol change...\n");
                long int current_vol = 0;
                // Change volume relative to current volume
                get_vol(sp->volume_cntrl_mixer_element, volume_type, &current_vol);
                // By setting the round direction we always guarantee that
                // some change happens.
                int round_direction = new_vol < 0 ? -1 : 1;
                set_vol(sp->volume_cntrl_mixer_element, volume_type, current_vol + new_vol, round_direction);
            }
        } else {
            // Change absolute volume
            PD_M("set_new_volume: Changing absolute volume: %li\n", new_vol);
            set_vol(sp->volume_cntrl_mixer_element, volume_type, new_vol, 0);
        }
    }

    if (use_semaphore && !check_semaphore(&sem)) return false;
    return true;
}

