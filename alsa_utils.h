#ifndef ALSA_UTILS_H_INCLUDED
#define ALSA_UTILS_H_INCLUDED

#include <alsa/asoundlib.h>
#include <stdbool.h>


snd_mixer_elem_t* get_elem(snd_mixer_t* handle, char const* name);

snd_mixer_t* get_handle(void);

bool is_mixer_elem_playback_switch_on(snd_mixer_elem_t* elem);

void list_mixer_elements(snd_mixer_t* handle);

#endif
