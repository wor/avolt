#ifndef VOLUME_CHANGE_H_INCLUDED
#define VOLUME_CHANGE_H_INCLUDED

#include <alsa/asoundlib.h>
#include <semaphore.h>

#include "avolt.conf.h"

void get_vol(
        snd_mixer_elem_t* elem,
        enum Volume_type volume_type,
        long int* vol);

bool set_new_volume(
        struct sound_profile* sp,
        long int new_vol,
        bool relative_inc,
        bool set_default_vol,
        bool toggle_vol,
        bool use_semaphore);

void change_range(
        long int* num,
        int const r_f_min,
        int const r_f_max,
        int const r_t_min,
        int const r_t_max,
        bool relative);

bool check_semaphore(sem_t** sem);

#endif
