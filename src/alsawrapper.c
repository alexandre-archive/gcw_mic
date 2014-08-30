#include "aplay.c"
#include "alsawrapper.h"

#include <pthread.h>

char* file_name;
pthread_t th;
void (*on_terminate_event)() = 0;

void alsawrapper_init(char* command, char* type, char* file_format,
                 int channels, int rate, int duration,
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
        stream = SND_PCM_STREAM_CAPTURE;
        file_type = FORMAT_WAVE;
        start_delay = 1;
    }
    else if (strstr(command, "aplay"))
    {
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

    duration = 10;

    if (duration > 0)
        timelimit = duration;

    if (separate_channels)
        interleaved = 0;

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

void* run(void*)
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
    //pthread_cancel(th);
}

void alsawrapper_on_terminate(void (*event)())
{
   on_terminate_event = event;
}

void alsawrapper_on_vu_change(void (*event)(signed int, signed int))
{
    on_vu_change_event = event;
}