#include "mixer.h"

Mixer::Mixer()
{
    init();
}

Mixer::~Mixer()
{
    // restore old values.
}

void Mixer::init()
{
#ifdef __MIPSEL__
    set_enum("Headphone Source", PCM);
    set_volume("PCM", 100, PLAYBACK);
    set_volume("PCM", 100, CAPTURE);
    set_volume("Mic", 100, CAPTURE);
    set_volume("Line In Bypass", 0, CAPTURE);
    switch_value("Mic", true, CAPTURE);
#endif
}

void Mixer::with_mixer(std::function<void(snd_mixer_t*, snd_mixer_selem_id_t*)> func)
{
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, CARD_NAME);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);

    func(handle, sid);

    snd_mixer_free(handle);
    snd_mixer_detach(handle, CARD_NAME);
    snd_mixer_close(handle);
}

void Mixer::set_volume(std::string source, long vol, Direction direction)
{
    auto snd_set_mixer_volume = [&] (snd_mixer_t *handle, snd_mixer_selem_id_t *sid)
    {
        long min, max;
        snd_mixer_elem_t *elem;

        snd_mixer_selem_id_set_name(sid, source.c_str());
        elem = snd_mixer_find_selem(handle, sid);

        if (direction == PLAYBACK)
        {
            snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
            snd_mixer_selem_set_playback_volume_all(elem, convert_space(vol, min, max, 0, 100));
        }
        else if (direction == CAPTURE)
        {
            snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
            snd_mixer_selem_set_capture_volume_all(elem, max);
        }
    };

    with_mixer(snd_set_mixer_volume);
}

void Mixer::set_enum(std::string source, int value)
{
    auto snd_mixer_set_enum = [&] (snd_mixer_t *handle, snd_mixer_selem_id_t *sid)
    {
        snd_mixer_elem_t *elem;

        snd_mixer_selem_id_set_name(sid, source.c_str());
        elem = snd_mixer_find_selem(handle, sid);
        snd_mixer_selem_set_enum_item(elem, SND_MIXER_SCHN_FRONT_LEFT, value);
    };

    with_mixer(snd_mixer_set_enum);
}

void Mixer::switch_value(std::string source, int value, Direction direction)
{
    auto snd_mixer_switch = [&] (snd_mixer_t *handle, snd_mixer_selem_id_t *sid)
    {
        snd_mixer_elem_t *elem;

        snd_mixer_selem_id_set_name(sid, source.c_str());
        elem = snd_mixer_find_selem(handle, sid);

        if (direction == PLAYBACK)
        {
            snd_mixer_selem_set_playback_switch_all(elem, value);
        }
        else if (direction == CAPTURE)
        {
            snd_mixer_selem_set_capture_switch_all(elem, value);
        }
    };

    with_mixer(snd_mixer_switch);
}

long Mixer::get_volume(std::string source, Direction direction)
{
    long vol = 0L;

    auto snd_get_mixer_volume = [&] (snd_mixer_t *handle, snd_mixer_selem_id_t *sid)
    {
        long min, max;
        snd_mixer_elem_t *elem;

        snd_mixer_selem_id_set_name(sid, source.c_str());
        elem = snd_mixer_find_selem(handle, sid);

        if (direction == PLAYBACK)
        {
            snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
            snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &vol);
        }
        else if (direction == CAPTURE)
        {
            snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
            snd_mixer_selem_get_capture_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &vol);
        }

        vol = convert_space(vol, 0, 100, min, max);
    };

    with_mixer(snd_get_mixer_volume);

    return vol;
}

void Mixer::set_mic_volume(long i)
{

}

void Mixer::set_speaker_volume(long i)
{
#ifdef __MIPSEL__
    set_volume("Headphone", i, PLAYBACK);
    set_volume("Speakers", i, PLAYBACK);
#else
    set_volume("Master", i, PLAYBACK);
#endif
}

long Mixer::get_mic_volume()
{
    return -1;
}

long Mixer::get_speaker_volume()
{
#ifdef __MIPSEL__
    return get_volume("Headphone", PLAYBACK);
#else
    return get_volume("Master", PLAYBACK);
#endif
}

void Mixer::set_direction(Direction direction)
{
#ifdef __MIPSEL__
    if (direction == CAPTURE)
    {
        set_enum("Line Out Source", MIC);
    }
    else if (direction == PLAYBACK)
    {
        set_enum("Line Out Source", PCM);
    }
#endif
}
