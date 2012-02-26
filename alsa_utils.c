#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>

#include "alsa_utils.h"
#include "avolt.conf"

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


// TODO get profile struct as parameter
/* Gets mixer front panels switch value (on/off).
 * Returns true for "on" and false for "off". */
bool get_mixer_front_panel_switch()
{
    return is_mixer_elem_playback_switch_on(FRONT_PANEL.mixer_element);
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


