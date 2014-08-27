#include "aplay.c"
#include "alsawrapper.h"

#include <pthread.h>

#define CARD_NAME "default"

char* file_name;
pthread_t th;
void (*on_terminate_event)() = 0;

typedef enum { CAPTURE, PLAYBACK } mixer_direction;

long convert_volume_space(long value, long min, long max)
{
    #define MIN_VOL 0
    #define MAX_VOL 100

    if (value > MAX_VOL)
    {
        value = MAX_VOL;
    }
    else if (value < MIN_VOL)
    {
        value = MIN_VOL;
    }

    return (((value - MIN_VOL) * (max - min)) / (MAX_VOL - MIN_VOL)) + min;
}

void with_mixer(void (*func)(snd_mixer_t*, snd_mixer_selem_id_t*))
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

void mixer_set_volume(char* source, long vol, mixer_direction direction)
{
    void snd_set_mixer_volume(snd_mixer_t *handle, snd_mixer_selem_id_t *sid)
    {
        long min, max;
        snd_mixer_elem_t *elem;

        snd_mixer_selem_id_set_name(sid, source);
        elem = snd_mixer_find_selem(handle, sid);

        if (direction == PLAYBACK)
        {
            snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
            snd_mixer_selem_set_playback_volume_all(elem, convert_volume_space(vol, min, max));
        }
        else if (direction == CAPTURE)
        {
            snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
            snd_mixer_selem_set_capture_volume_all(elem, max);
        }
    }

    with_mixer(snd_set_mixer_volume);
}

void mixer_set_enum(char* source, int value)
{
    void snd_mixer_set_enum(snd_mixer_t *handle, snd_mixer_selem_id_t *sid)
    {
        snd_mixer_elem_t *elem;

        snd_mixer_selem_id_set_name(sid, source);
        elem = snd_mixer_find_selem(handle, sid);
        snd_mixer_selem_set_enum_item(elem, SND_MIXER_SCHN_FRONT_LEFT, value);
    }

    with_mixer(snd_mixer_set_enum);
}

void mixer_switch(char* source, int value, mixer_direction direction)
{
    void snd_mixer_switch(snd_mixer_t *handle, snd_mixer_selem_id_t *sid)
    {
        snd_mixer_elem_t *elem;

        snd_mixer_selem_id_set_name(sid, source);
        elem = snd_mixer_find_selem(handle, sid);

        if (direction == PLAYBACK)
        {
            snd_mixer_selem_set_playback_switch_all(elem, value);
        }
        else if (direction == CAPTURE)
        {
            snd_mixer_selem_set_capture_switch_all(elem, value);
        }
    }

    with_mixer(snd_mixer_switch);
}

long mixer_get_volume(char* source, mixer_direction direction)
{
    long vol = 0;

    void snd_get_mixer_volume(snd_mixer_t *handle, snd_mixer_selem_id_t *sid)
    {
        long min, max;
        snd_mixer_elem_t *elem;

        snd_mixer_selem_id_set_name(sid, source);
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

        vol = convert_volume_space(vol, min, max);
    }

    with_mixer(snd_get_mixer_volume);

    return vol;
}

void configure_mixer()
{
#ifdef MIPSEL
    mixer_set_enum("Headphone Source", PCM);
    mixer_set_volume("PCM", 100, PLAYBACK);
    mixer_set_volume("PCM", 100, CAPTURE);
    mixer_set_volume("Mic", 100, CAPTURE);
    mixer_set_volume("Line In Bypass", 0, CAPTURE);
    mixer_switch("Mic", true, CAPTURE);
#endif
}

void alsawrapper_init(char* command, char* type, char* file_format,
                 char vu, int channels, int rate, int duration,
                 bool separate_channels, char* file_n)
{
    in_aborting = 0;

    file_name = file_n;
    char *pcm_name = "default";
    int tmp, err;
    snd_pcm_info_t *info;

    snd_pcm_info_alloca(&info);

    err = snd_output_stdio_attach(&plog, stderr, 0);
    assert(err >= 0);

    file_type = FORMAT_DEFAULT;

    if (strstr(command, "arecord"))
    {
#ifdef MIPSEL
        mixer_set_enum("Line Out Source", MIC);
#endif
        stream = SND_PCM_STREAM_CAPTURE;
        file_type = FORMAT_WAVE;
        start_delay = 1;
    }
    else if (strstr(command, "aplay"))
    {
#ifdef MIPSEL
        mixer_set_enum("Line Out Source", PCM);
#endif
        stream = SND_PCM_STREAM_PLAYBACK;
    }
    else
    {
        log(FATAL, "command should be named either arecord or aplay");
        return;
    }

    chunk_size = -1;

    rhwparams.format = DEFAULT_FORMAT;
    rhwparams.rate = DEFAULT_SPEED;
    rhwparams.channels = 1;

    if (strcasecmp(type, "raw") == 0)
        file_type = FORMAT_RAW;
    else if (strcasecmp(type, "voc") == 0)
        file_type = FORMAT_VOC;
    else if (strcasecmp(type, "wav") == 0)
        file_type = FORMAT_WAVE;
    else if (strcasecmp(type, "au") == 0 || strcasecmp(type, "sparc") == 0)
        file_type = FORMAT_AU;
    else
    {
        log(FATAL, "unrecognized file format %s", type);
        return;
    }

    rhwparams.channels = channels;

    if (rhwparams.channels < 1 || rhwparams.channels > 256)
    {
        log(FATAL, "value %i for channels is invalid", rhwparams.channels);
        return;
    }

    if (strcasecmp(file_format, "cd") == 0 || strcasecmp(file_format, "cdr") == 0)
    {
        if (strcasecmp(file_format, "cdr") == 0)
            rhwparams.format = SND_PCM_FORMAT_S16_BE;
        else
            rhwparams.format = file_type == FORMAT_AU ? SND_PCM_FORMAT_S16_BE : SND_PCM_FORMAT_S16_LE;

        rhwparams.rate = 44100;
        rhwparams.channels = 2;
    }
    else if (strcasecmp(file_format, "dat") == 0)
    {
        rhwparams.format = file_type == FORMAT_AU ? SND_PCM_FORMAT_S16_BE : SND_PCM_FORMAT_S16_LE;
        rhwparams.rate = 48000;
        rhwparams.channels = 2;
    }
    else
    {
        rhwparams.format = snd_pcm_format_value(file_format);

        if (rhwparams.format == SND_PCM_FORMAT_UNKNOWN)
        {
            close_handle();
            log(FATAL, "wrong extended format '%s'", file_format);
        }
    }

    tmp = rate;

    if (tmp < 300)
        tmp *= 1000;

    rhwparams.rate = tmp;

    if (tmp < 2000 || tmp > 192000)
    {
        log(FATAL, "bad speed value %i", tmp);
        return;
    }

    if (duration > 0)
        timelimit = duration;

    if (separate_channels)
        interleaved = 0;

    if (vu == 's')
        vumeter = VUMETER_STEREO;
    else if (vu == 'm')
        vumeter = VUMETER_MONO;
    else
        vumeter = VUMETER_NONE;

    vumeter = VUMETER_NONE;

    nonblock = 0;

    close_handle();
    err = snd_pcm_open(&handle, pcm_name, stream, 0);

    if (err < 0)
    {
        log(FATAL, "audio open error: %s", snd_strerror(err));
    }

    if ((err = snd_pcm_info(handle, info)) < 0)
    {
        log(FATAL, "info error: %s", snd_strerror(err));
    }

    if (nonblock)
    {
        err = snd_pcm_nonblock(handle, 1);

        if (err < 0)
        {
            log(FATAL, "nonblock setting error: %s.", snd_strerror(err));
        }
    }

    chunk_size = 1024;
    hwparams = rhwparams;

    audiobuf = (u_char *)malloc(1024);

    if (audiobuf == NULL)
    {
        log(FATAL, "not enough memory.");
    }

    writei_func = snd_pcm_writei;
    readi_func = snd_pcm_readi;
    writen_func = snd_pcm_writen;
    readn_func = snd_pcm_readn;

    set_params();
}

void* run()
{
    if (stream == SND_PCM_STREAM_PLAYBACK)
    {
        playback(file_name);
    }
    else
    {
        capture(file_name);
    }

    if (on_terminate_event)
    {
        on_terminate_event();
    }

    return NULL;
}

void alsawrapper_start()
{

    if(pthread_create(&th, NULL, run, NULL))
    {
        log(FATAL, "Error creating thread.");
    }
}

void alsawrapper_stop()
{
    abort_handle();
    pthread_cancel(th);
}

void alsawrapper_on_terminate(void (*event)())
{
   on_terminate_event = event;
}

void alsawrapper_on_vu_change(void (*event)(signed int, signed int))
{
    on_vu_change_event = event;
}

void alsawrapper_set_volume(long vol)
{
#ifdef MIPSEL
    mixer_set_volume("Headphone", vol, PLAYBACK);
    mixer_set_volume("Speakers", vol, PLAYBACK);
#else
    mixer_set_volume("Master", vol, PLAYBACK);
#endif
}
