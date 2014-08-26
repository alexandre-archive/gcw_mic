/*
 *  aplay.c - plays and records
 *
 *      CREATIVE LABS CHANNEL-files
 *      Microsoft WAVE-files
 *      SPARC AUDIO .AU-files
 *      Raw Data
 *
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *  Based on vplay program by Michael Beck
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#define _GNU_SOURCE

#include <alsa/asoundlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "formats.h"
#include "log.h"

#ifndef LLONG_MAX
    #define LLONG_MAX 9223372036854775807LL
#endif

#ifndef le16toh
    #include <asm/byteorder.h>
    #define le16toh(x) __le16_to_cpu(x)
    #define be16toh(x) __be16_to_cpu(x)
    #define le32toh(x) __le32_to_cpu(x)
    #define be32toh(x) __be32_to_cpu(x)
#endif

#define DEFAULT_FORMAT SND_PCM_FORMAT_U8
#define DEFAULT_SPEED  8000

#define FORMAT_DEFAULT -1
#define FORMAT_RAW      0
#define FORMAT_VOC      1
#define FORMAT_WAVE     2
#define FORMAT_AU       3

#ifndef SND_PCM_ABORT
    static inline int snd_pcm_abort(snd_pcm_t *pcm) { return snd_pcm_nonblock(pcm, 2); }
#endif

/* global data */

static snd_pcm_sframes_t(*readi_func)(snd_pcm_t *handle, void *buffer, snd_pcm_uframes_t size);
static snd_pcm_sframes_t(*writei_func)(snd_pcm_t *handle, const void *buffer, snd_pcm_uframes_t size);
static snd_pcm_sframes_t(*readn_func)(snd_pcm_t *handle, void **bufs, snd_pcm_uframes_t size);
static snd_pcm_sframes_t(*writen_func)(snd_pcm_t *handle, void **bufs, snd_pcm_uframes_t size);

enum
{
    VUMETER_NONE,
    VUMETER_MONO,
    VUMETER_STEREO
};

static struct
{
    snd_pcm_format_t format;
    unsigned int channels;
    unsigned int rate;
} hwparams, rhwparams;

static char *command;
static snd_pcm_t *handle;
static int timelimit = 0;
static int quiet_mode = 0;
static int file_type = FORMAT_DEFAULT;
static snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
static int interleaved = 1;
static int nonblock = 0;
static int in_aborting = 0;
static u_char *audiobuf = NULL;
static unsigned period_time = 0;
static unsigned buffer_time = 0;
static snd_pcm_uframes_t chunk_size = 0;
static snd_pcm_uframes_t period_frames = 0;
static snd_pcm_uframes_t buffer_frames = 0;
static int avail_min = -1;
static int start_delay = 0;
static int stop_delay = 0;
static int monotonic = 0;
static int can_pause = 0;
static int fatal_errors = 0;
static int vumeter = VUMETER_NONE;
static int buffer_pos = 0;
static size_t bits_per_sample, bits_per_frame;
static size_t chunk_bytes;
static snd_output_t *plog;
static int fd = -1;
static off64_t pbrec_count = LLONG_MAX, fdcount;

/* needed prototypes */

static void playback(char *filename);
static void capture(char *filename);

static void begin_voc(int fd, size_t count);
static void end_voc(int fd);
static void begin_wave(int fd, size_t count);
static void end_wave(int fd);
static void begin_au(int fd, size_t count);
static void end_au(int fd);

static const struct fmt_capture
{
    void (*start) (int fd, size_t count);
    void (*end) (int fd);
    char *what;
    long long max_filesize;
} fmt_rec_table[] = {
    { NULL,        NULL,    "raw data",    LLONG_MAX },
    { begin_voc,  end_voc,  "VOC",         16000000LL },
    /* FIXME: can WAV handle exactly 2GB or less than it? */
    { begin_wave, end_wave, "WAVE",        2147483648LL },
    { begin_au,   end_au,   "Sparc Audio", LLONG_MAX }
};

/*
 *  Subroutine to clean up before exit.
 */
static void prg_exit(int code)
{
    if (handle)
        snd_pcm_close(handle);

    exit(code);
}

static void signal_handler(int sig)
{
    if (in_aborting)
        return;

    in_aborting = 1;

    if (!quiet_mode)
        fprintf(stderr, "Stopping...\n");

    if (handle)
    {
        snd_pcm_abort(handle);
    }
}

/*
 * Safe read (for pipes)
 */
static ssize_t safe_read(int fdx, void *buf, size_t count)
{
    ssize_t result = 0, res;

    while (count > 0 && !in_aborting)
    {
        if ((res = read(fdx, buf, count)) == 0)
            break;

        if (res < 0)
            return result > 0 ? result : res;

        count -= res;
        result += res;
        buf = (char *) buf + res;
    }

    return result;
}

/*
 * Test, if it is a .VOC file and return >=0 if ok (this is the length of rest)
 *                                       < 0 if not 
 */
static int test_vocfile(void *buffer)
{
    VocHeader *vp = buffer;

    if (!memcmp(vp->magic, VOC_MAGIC_STRING, 20))
    {
        if (LE_SHORT(vp->version) != (0x1233 - LE_SHORT(vp->coded_ver)))
            return -2; /* coded version mismatch */

        return LE_SHORT(vp->headerlen) - sizeof (VocHeader); /* 0 mostly */
    }

    return -1; /* magic string fail */
}

/*
 * helper for test_wavefile
 */
static size_t test_wavefile_read(int fdx, u_char *buffer, size_t *size, size_t reqsize, int line)
{
    if (*size >= reqsize)
        return *size;

    if ((size_t) safe_read(fdx, buffer + *size, reqsize - *size) != reqsize - *size)
    {
        log(FATAL, "read error (called from line %i)", line);
        prg_exit(EXIT_FAILURE);
    }

    return *size = reqsize;
}

#define check_wavefile_space(buffer, lenx, blimit) \
    if (lenx > blimit) { \
        blimit = lenx; \
        if ((buffer = realloc(buffer, blimit)) == NULL) { \
            log(FATAL, "not enough memory");        \
            prg_exit(EXIT_FAILURE);  \
        } \
    }

/*
 * test, if it's a .WAV file, > 0 if ok (and set the speed, stereo etc.)
 *                            == 0 if not
 * Value returned is bytes to be discarded.
 */
static ssize_t test_wavefile(int fd, u_char *_buffer, size_t size)
{
    WaveHeader *h = (WaveHeader *) _buffer;
    u_char *buffer = NULL;
    size_t blimit = 0;
    WaveFmtBody *f;
    WaveChunkHeader *c;
    u_int type, len;
    unsigned short format, channels;
    int big_endian, native_format;

    if (size < sizeof (WaveHeader))
        return -1;

    if (h->magic == WAV_RIFF)
        big_endian = 0;
    else if (h->magic == WAV_RIFX)
        big_endian = 1;
    else
        return -1;

    if (h->type != WAV_WAVE)
        return -1;

    if (size > sizeof (WaveHeader))
    {
        check_wavefile_space(buffer, size - sizeof (WaveHeader), blimit);
        memcpy(buffer, _buffer + sizeof (WaveHeader), size - sizeof (WaveHeader));
    }

    size -= sizeof (WaveHeader);

    while (1)
    {
        check_wavefile_space(buffer, sizeof (WaveChunkHeader), blimit);
        test_wavefile_read(fd, buffer, &size, sizeof (WaveChunkHeader), __LINE__);
        c = (WaveChunkHeader*) buffer;
        type = c->type;
        len = TO_CPU_INT(c->length, big_endian);
        len += len % 2;

        if (size > sizeof (WaveChunkHeader))
            memmove(buffer, buffer + sizeof (WaveChunkHeader), size - sizeof (WaveChunkHeader));

        size -= sizeof (WaveChunkHeader);

        if (type == WAV_FMT)
            break;

        check_wavefile_space(buffer, len, blimit);
        test_wavefile_read(fd, buffer, &size, len, __LINE__);

        if (size > len)
            memmove(buffer, buffer + len, size - len);

        size -= len;
    }

    if (len < sizeof (WaveFmtBody))
    {
        log(FATAL, "unknown length of 'fmt ' chunk (read %u, should be %u at least)", len, (u_int)sizeof (WaveFmtBody));
        prg_exit(EXIT_FAILURE);
    }

    check_wavefile_space(buffer, len, blimit);
    test_wavefile_read(fd, buffer, &size, len, __LINE__);
    f = (WaveFmtBody*) buffer;
    format = TO_CPU_SHORT(f->format, big_endian);

    if (format == WAV_FMT_EXTENSIBLE)
    {
        WaveFmtExtensibleBody *fe = (WaveFmtExtensibleBody*) buffer;

        if (len < sizeof (WaveFmtExtensibleBody))
        {
            log(FATAL, "unknown length of extensible 'fmt ' chunk (read %u, should be %u at least)", len, (u_int)sizeof (WaveFmtExtensibleBody));
            prg_exit(EXIT_FAILURE);
        }

        if (memcmp(fe->guid_tag, WAV_GUID_TAG, 14) != 0)
        {
            log(FATAL, "wrong format tag in extensible 'fmt ' chunk");
            prg_exit(EXIT_FAILURE);
        }

        format = TO_CPU_SHORT(fe->guid_format, big_endian);
    }

    if (format != WAV_FMT_PCM &&
            format != WAV_FMT_IEEE_FLOAT)
    {
        log(FATAL, "can't play WAVE-file format 0x%04x which is not PCM or FLOAT encoded", format);
        prg_exit(EXIT_FAILURE);
    }

    channels = TO_CPU_SHORT(f->channels, big_endian);

    if (channels < 1)
    {
        log(FATAL, "can't play WAVE-files with %d tracks", channels);
        prg_exit(EXIT_FAILURE);
    }

    hwparams.channels = channels;

    switch (TO_CPU_SHORT(f->bit_p_spl, big_endian))
    {
        case 8:
            if (hwparams.format != DEFAULT_FORMAT &&
                    hwparams.format != SND_PCM_FORMAT_U8)
                fprintf(stderr, "Warning: format is changed to U8\n");
            hwparams.format = SND_PCM_FORMAT_U8;
            break;
        case 16:
            if (big_endian)
                native_format = SND_PCM_FORMAT_S16_BE;
            else
                native_format = SND_PCM_FORMAT_S16_LE;
            if (hwparams.format != DEFAULT_FORMAT &&
                    hwparams.format != native_format)
                fprintf(stderr, "Warning: format is changed to %s\n",
                    snd_pcm_format_name(native_format));
            hwparams.format = native_format;
            break;
        case 24:
            switch (TO_CPU_SHORT(f->byte_p_spl, big_endian) / hwparams.channels)
            {
                case 3:
                    if (big_endian)
                        native_format = SND_PCM_FORMAT_S24_3BE;
                    else
                        native_format = SND_PCM_FORMAT_S24_3LE;
                    if (hwparams.format != DEFAULT_FORMAT &&
                            hwparams.format != native_format)
                        fprintf(stderr, "Warning: format is changed to %s\n",
                            snd_pcm_format_name(native_format));
                    hwparams.format = native_format;
                    break;
                case 4:
                    if (big_endian)
                        native_format = SND_PCM_FORMAT_S24_BE;
                    else
                        native_format = SND_PCM_FORMAT_S24_LE;
                    if (hwparams.format != DEFAULT_FORMAT &&
                            hwparams.format != native_format)
                        fprintf(stderr, "Warning: format is changed to %s\n",
                            snd_pcm_format_name(native_format));
                    hwparams.format = native_format;
                    break;
                default:
                    log(FATAL, " can't play WAVE-files with sample %d bits in %d bytes wide (%d channels)",
                            TO_CPU_SHORT(f->bit_p_spl, big_endian),
                            TO_CPU_SHORT(f->byte_p_spl, big_endian),
                            hwparams.channels);
                    prg_exit(EXIT_FAILURE);
            }
            break;
        case 32:
            if (format == WAV_FMT_PCM)
            {
                if (big_endian)
                    native_format = SND_PCM_FORMAT_S32_BE;
                else
                    native_format = SND_PCM_FORMAT_S32_LE;
                hwparams.format = native_format;
            }
            else if (format == WAV_FMT_IEEE_FLOAT)
            {
                if (big_endian)
                    native_format = SND_PCM_FORMAT_FLOAT_BE;
                else
                    native_format = SND_PCM_FORMAT_FLOAT_LE;
                hwparams.format = native_format;
            }
            break;
        default:
            log(FATAL, " can't play WAVE-files with sample %d bits wide",
                    TO_CPU_SHORT(f->bit_p_spl, big_endian));
            prg_exit(EXIT_FAILURE);
    }

    hwparams.rate = TO_CPU_INT(f->sample_fq, big_endian);

    if (size > len)
        memmove(buffer, buffer + len, size - len);

    size -= len;

    while (1)
    {
        u_int type, len;

        check_wavefile_space(buffer, sizeof (WaveChunkHeader), blimit);
        test_wavefile_read(fd, buffer, &size, sizeof (WaveChunkHeader), __LINE__);
        c = (WaveChunkHeader*) buffer;
        type = c->type;
        len = TO_CPU_INT(c->length, big_endian);

        if (size > sizeof (WaveChunkHeader))
            memmove(buffer, buffer + sizeof (WaveChunkHeader), size - sizeof (WaveChunkHeader));

        size -= sizeof (WaveChunkHeader);

        if (type == WAV_DATA)
        {
            if (len < pbrec_count && len < 0x7ffffffe)
                pbrec_count = len;

            if (size > 0)
                memcpy(_buffer, buffer, size);

            free(buffer);
            return size;
        }

        len += len % 2;
        check_wavefile_space(buffer, len, blimit);
        test_wavefile_read(fd, buffer, &size, len, __LINE__);

        if (size > len)
            memmove(buffer, buffer + len, size - len);

        size -= len;
    }

    /* shouldn't be reached */
    return -1;
}

/*

 */

static int test_au(int fdx, void *buffer)
{
    AuHeader *ap = buffer;

    if (ap->magic != AU_MAGIC)
        return -1;

    if (BE_INT(ap->hdr_size) > 128 || BE_INT(ap->hdr_size) < 24)
        return -1;

    pbrec_count = BE_INT(ap->data_size);

    switch (BE_INT(ap->encoding))
    {
        case AU_FMT_ULAW:
            if (hwparams.format != DEFAULT_FORMAT && hwparams.format != SND_PCM_FORMAT_MU_LAW)
                fprintf(stderr, "Warning: format is changed to MU_LAW\n");

            hwparams.format = SND_PCM_FORMAT_MU_LAW;
            break;
        case AU_FMT_LIN8:
            if (hwparams.format != DEFAULT_FORMAT && hwparams.format != SND_PCM_FORMAT_U8)
                fprintf(stderr, "Warning: format is changed to U8\n");

            hwparams.format = SND_PCM_FORMAT_U8;
            break;
        case AU_FMT_LIN16:
            if (hwparams.format != DEFAULT_FORMAT && hwparams.format != SND_PCM_FORMAT_S16_BE)
                fprintf(stderr, "Warning: format is changed to S16_BE\n");

            hwparams.format = SND_PCM_FORMAT_S16_BE;
            break;
        default:
            return -1;
    }

    hwparams.rate = BE_INT(ap->sample_rate);

    if (hwparams.rate < 2000 || hwparams.rate > 256000)
        return -1;

    hwparams.channels = BE_INT(ap->channels);

    if (hwparams.channels < 1 || hwparams.channels > 256)
        return -1;

    if ((size_t) safe_read(fdx, buffer + sizeof (AuHeader), BE_INT(ap->hdr_size) - sizeof (AuHeader)) != BE_INT(ap->hdr_size) - sizeof (AuHeader))
    {
        log(FATAL, "read error");
        prg_exit(EXIT_FAILURE);
    }

    return 0;
}

static void set_params(void)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_uframes_t buffer_size, start_threshold, stop_threshold;
    size_t n;
    unsigned int rate;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);
    int err = snd_pcm_hw_params_any(handle, params);

    if (err < 0)
    {
        log(FATAL, "Broken configuration for this PCM: no configurations available");
        prg_exit(EXIT_FAILURE);
    }

    if (interleaved)
    {
        err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    }
    else
    {
        err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_NONINTERLEAVED);
    }

    if (err < 0)
    {
        log(FATAL, "Access type not available");
        prg_exit(EXIT_FAILURE);
    }

    err = snd_pcm_hw_params_set_format(handle, params, hwparams.format);

    if (err < 0)
    {
        log(FATAL, "Sample format non available");
        prg_exit(EXIT_FAILURE);
    }

    err = snd_pcm_hw_params_set_channels(handle, params, hwparams.channels);

    if (err < 0)
    {
        log(FATAL, "Channels count non available");
        prg_exit(EXIT_FAILURE);
    }

    rate = hwparams.rate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &hwparams.rate, 0);
    assert(err >= 0);

    if ((float) rate * 1.05 < hwparams.rate || (float) rate * 0.95 > hwparams.rate)
    {
        if (!quiet_mode)
        {
            char plugex[64];
            const char *pcmname = snd_pcm_name(handle);
            fprintf(stderr, "Warning: rate is not accurate (requested = %iHz, got = %iHz)\n", rate, hwparams.rate);

            if (!pcmname || strchr(snd_pcm_name(handle), ':'))
                *plugex = 0;
            else
                snprintf(plugex, sizeof (plugex), "(-Dplug:%s)", snd_pcm_name(handle));

            fprintf(stderr, "please, try the plug plugin %s\n", plugex);
        }
    }

    rate = hwparams.rate;

    if (buffer_time == 0 && buffer_frames == 0)
    {
        err = snd_pcm_hw_params_get_buffer_time_max(params, &buffer_time, 0);

        assert(err >= 0);

        if (buffer_time > 500000)
            buffer_time = 500000;
    }

    if (period_time == 0 && period_frames == 0)
    {
        if (buffer_time > 0)
            period_time = buffer_time / 4;
        else
            period_frames = buffer_frames / 4;
    }
    if (period_time > 0)
        err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, 0);
    else
        err = snd_pcm_hw_params_set_period_size_near(handle, params, &period_frames, 0);

    assert(err >= 0);

    if (buffer_time > 0)
    {
        err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, 0);
    }
    else
    {
        err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_frames);
    }

    assert(err >= 0);

    monotonic = snd_pcm_hw_params_is_monotonic(params);
    can_pause = snd_pcm_hw_params_can_pause(params);
    err = snd_pcm_hw_params(handle, params);

    if (err < 0)
    {
        log(FATAL, "Unable to install hw params:");
        snd_pcm_hw_params_dump(params, plog);
        prg_exit(EXIT_FAILURE);
    }

    snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);
    snd_pcm_hw_params_get_buffer_size(params, &buffer_size);

    if (chunk_size == buffer_size)
    {
        log(FATAL, "Can't use period equal to buffer size (%lu == %lu)", chunk_size, buffer_size);
        prg_exit(EXIT_FAILURE);
    }

    snd_pcm_sw_params_current(handle, swparams);

    if (avail_min < 0)
        n = chunk_size;
    else
        n = (double) rate * avail_min / 1000000;

    err = snd_pcm_sw_params_set_avail_min(handle, swparams, n);

    /* round up to closest transfer boundary */
    n = buffer_size;

    if (start_delay <= 0)
    {
        start_threshold = n + (double) rate * start_delay / 1000000;
    }
    else
    {
        start_threshold = (double) rate * start_delay / 1000000;
    }

    if (start_threshold < 1)
        start_threshold = 1;

    if (start_threshold > n)
        start_threshold = n;

    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, start_threshold);

    assert(err >= 0);

    if (stop_delay <= 0)
        stop_threshold = buffer_size + (double) rate * stop_delay / 1000000;
    else
        stop_threshold = (double) rate * stop_delay / 1000000;

    err = snd_pcm_sw_params_set_stop_threshold(handle, swparams, stop_threshold);

    assert(err >= 0);

    if (snd_pcm_sw_params(handle, swparams) < 0)
    {
        log(FATAL, "unable to install sw params:");
        snd_pcm_sw_params_dump(swparams, plog);
        prg_exit(EXIT_FAILURE);
    }

    bits_per_sample = snd_pcm_format_physical_width(hwparams.format);
    bits_per_frame = bits_per_sample * hwparams.channels;
    chunk_bytes = chunk_size * bits_per_frame / 8;
    audiobuf = realloc(audiobuf, chunk_bytes);

    if (audiobuf == NULL)
    {
        log(FATAL, "not enough memory");
        prg_exit(EXIT_FAILURE);
    }

    /* stereo VU-meter isn't always available... */
    if (vumeter == VUMETER_STEREO)
    {
        if (hwparams.channels != 2 || !interleaved)
            vumeter = VUMETER_MONO;
    }

    buffer_frames = buffer_size; /* for position test */
}

#ifndef timersub
#define timersub(a, b, result) \
do { \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
    if ((result)->tv_usec < 0) { \
        --(result)->tv_sec; \
        (result)->tv_usec += 1000000; \
    } \
} while (0)
#endif

#ifndef timermsub
#define timermsub(a, b, result) \
do { \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
    (result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
    if ((result)->tv_nsec < 0) { \
        --(result)->tv_sec; \
        (result)->tv_nsec += 1000000000L; \
    } \
} while (0)
#endif

/* I/O error handler */
static void xrun(void)
{
    snd_pcm_status_t *status;
    int res;

    snd_pcm_status_alloca(&status);

    if ((res = snd_pcm_status(handle, status)) < 0)
    {
        log(FATAL, "status error: %s", snd_strerror(res));
        prg_exit(EXIT_FAILURE);
    }

    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN)
    {
        if (fatal_errors)
        {
            log(FATAL, "fatal %s: %s", stream == SND_PCM_STREAM_PLAYBACK ? "underrun" : "overrun", snd_strerror(res));
            prg_exit(EXIT_FAILURE);
        }

        if (monotonic)
        {
#ifdef HAVE_CLOCK_GETTIME
            struct timespec now, diff, tstamp;
            clock_gettime(CLOCK_MONOTONIC, &now);
            snd_pcm_status_get_trigger_htstamp(status, &tstamp);
            timermsub(&now, &tstamp, &diff);
            fprintf(stderr, "%s!!! (at least %.3f ms long)\n",
                    stream == SND_PCM_STREAM_PLAYBACK ? "underrun" : "overrun",
                    diff.tv_sec * 1000 + diff.tv_nsec / 1000000.0);
#else
            fprintf(stderr, "%s !!!\n", "underrun");
#endif
        }
        else
        {
            struct timeval now, diff, tstamp;
            gettimeofday(&now, 0);
            snd_pcm_status_get_trigger_tstamp(status, &tstamp);
            timersub(&now, &tstamp, &diff);
            fprintf(stderr, "%s!!! (at least %.3f ms long)\n",
                    stream == SND_PCM_STREAM_PLAYBACK ? "underrun" : "overrun",
                    diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
        }

        if ((res = snd_pcm_prepare(handle)) < 0)
        {
            log(FATAL, "xrun: prepare error: %s", snd_strerror(res));
            prg_exit(EXIT_FAILURE);
        }

        return; /* ok, data should be accepted again */
    }

    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING)
    {
        if (stream == SND_PCM_STREAM_CAPTURE)
        {
            fprintf(stderr, "capture stream format change? attempting recover...\n");

            if ((res = snd_pcm_prepare(handle)) < 0)
            {
                log(FATAL, "xrun(DRAINING): prepare error: %s", snd_strerror(res));
                prg_exit(EXIT_FAILURE);
            }

            return;
        }
    }

    log(FATAL, "read/write error, state = %s", snd_pcm_state_name(snd_pcm_status_get_state(status)));
    prg_exit(EXIT_FAILURE);
}

/* I/O suspend handler */
static void suspend(void)
{
    int res;

    if (!quiet_mode)
        fprintf(stderr, "Suspended. Trying resume. ");

    fflush(stderr);

    while ((res = snd_pcm_resume(handle)) == -EAGAIN)
        sleep(1); /* wait until suspend flag is released */

    if (res < 0)
    {
        if (!quiet_mode)
            fprintf(stderr, "Failed. Restarting stream. ");

        fflush(stderr);

        if ((res = snd_pcm_prepare(handle)) < 0)
        {
            log(FATAL, "suspend: prepare error: %s", snd_strerror(res));
            prg_exit(EXIT_FAILURE);
        }
    }

    if (!quiet_mode)
        fprintf(stderr, "Done.\n");
}

static void print_vu_meter_mono(int perc, int maxperc)
{
    const int bar_length = 50;
    char line[80];
    int val;

    for (val = 0; val <= perc * bar_length / 100 && val < bar_length; val++)
        line[val] = '#';

    for (; val <= maxperc * bar_length / 100 && val < bar_length; val++)
        line[val] = ' ';

    line[val] = '+';

    for (++val; val <= bar_length; val++)
        line[val] = ' ';

    if (maxperc > 99)
        sprintf(line + val, "| MAX");
    else
        sprintf(line + val, "| %02i%%", maxperc);

    fputs(line, stderr);

    if (perc > 100)
        fprintf(stderr, " !clip  ");
}

static void print_vu_meter_stereo(int *perc, int *maxperc)
{
    const int bar_length = 35;
    char line[80];
    int c;

    memset(line, ' ', sizeof (line) - 1);
    line[bar_length + 3] = '|';

    for (c = 0; c < 2; c++)
    {
        int p = perc[c] * bar_length / 100;
        char tmp[4];

        if (p > bar_length)
            p = bar_length;

        if (c)
            memset(line + bar_length + 6 + 1, '#', p);
        else
            memset(line + bar_length - p - 1, '#', p);

        p = maxperc[c] * bar_length / 100;

        if (p > bar_length)
            p = bar_length;

        if (c)
            line[bar_length + 6 + 1 + p] = '+';
        else
            line[bar_length - p - 1] = '+';

        if (maxperc[c] > 99)
            sprintf(tmp, "MAX");
        else
            sprintf(tmp, "%02d%%", maxperc[c]);

        if (c)
            memcpy(line + bar_length + 3 + 1, tmp, 3);
        else
            memcpy(line + bar_length, tmp, 3);
    }

    line[bar_length * 2 + 6 + 2] = 0;
    fputs(line, stderr);
}

static void print_vu_meter(signed int *perc, signed int *maxperc)
{
    if (vumeter == VUMETER_STEREO)
        print_vu_meter_stereo(perc, maxperc);
    else
        print_vu_meter_mono(*perc, *maxperc);
}

/* peak handler */
static void compute_max_peak(u_char *data, size_t count)
{
    signed int val, max, perc[2], max_peak[2];
    static int run = 0;
    size_t ocount = count;
    int format_little_endian = snd_pcm_format_little_endian(hwparams.format);
    int ichans, c;

    if (vumeter == VUMETER_STEREO)
        ichans = 2;
    else
        ichans = 1;

    memset(max_peak, 0, sizeof (max_peak));

    switch (bits_per_sample)
    {
        case 8:
        {
            signed char *valp = (signed char *) data;
            signed char mask = snd_pcm_format_silence(hwparams.format);
            c = 0;

            while (count-- > 0)
            {
                val = *valp++ ^ mask;
                val = abs(val);
                if (max_peak[c] < val)
                    max_peak[c] = val;
                if (vumeter == VUMETER_STEREO)
                    c = !c;
            }
            break;
        }
        case 16:
        {
            signed short *valp = (signed short *) data;
            signed short mask = snd_pcm_format_silence_16(hwparams.format);
            signed short sval;

            count /= 2;
            c = 0;

            while (count-- > 0)
            {
                if (format_little_endian)
                    sval = le16toh(*valp);
                else
                    sval = be16toh(*valp);
                sval = abs(sval) ^ mask;
                if (max_peak[c] < sval)
                    max_peak[c] = sval;
                valp++;
                if (vumeter == VUMETER_STEREO)
                    c = !c;
            }
            break;
        }
        case 24:
        {
            unsigned char *valp = data;
            signed int mask = snd_pcm_format_silence_32(hwparams.format);

            count /= 3;
            c = 0;
            while (count-- > 0)
            {
                if (format_little_endian)
                {
                    val = valp[0] | (valp[1] << 8) | (valp[2] << 16);
                }
                else
                {
                    val = (valp[0] << 16) | (valp[1] << 8) | valp[2];
                }
                /* Correct signed bit in 32-bit value */
                if (val & (1 << (bits_per_sample - 1)))
                {
                    val |= 0xff << 24; /* Negate upper bits too */
                }
                val = abs(val) ^ mask;
                if (max_peak[c] < val)
                    max_peak[c] = val;
                valp += 3;
                if (vumeter == VUMETER_STEREO)
                    c = !c;
            }
            break;
        }
        case 32:
        {
            signed int *valp = (signed int *) data;
            signed int mask = snd_pcm_format_silence_32(hwparams.format);

            count /= 4;
            c = 0;
            while (count-- > 0)
            {
                if (format_little_endian)
                    val = le32toh(*valp);
                else
                    val = be32toh(*valp);
                val = abs(val) ^ mask;
                if (max_peak[c] < val)
                    max_peak[c] = val;
                valp++;
                if (vumeter == VUMETER_STEREO)
                    c = !c;
            }
            break;
        }
        default:
            if (run == 0)
            {
                fprintf(stderr, "Unsupported bit size %d.\n", (int) bits_per_sample);
                run = 1;
            }
            return;
    }

    max = 1 << (bits_per_sample - 1);

    if (max <= 0)
        max = 0x7fffffff;

    for (c = 0; c < ichans; c++)
    {
        if (bits_per_sample > 16)
            perc[c] = max_peak[c] / (max / 100);
        else
            perc[c] = max_peak[c] * 100 / max;
    }

    if (interleaved)
    {
        static int maxperc[2];
        static time_t t = 0;
        const time_t tt = time(NULL);

        if (tt > t)
        {
            t = tt;
            maxperc[0] = 0;
            maxperc[1] = 0;
        }

        for (c = 0; c < ichans; c++)
            if (perc[c] > maxperc[c])
                maxperc[c] = perc[c];

        putc('\r', stderr);
        print_vu_meter(perc, maxperc);
        fflush(stderr);
    }
}

/*
 *  write function
 */
static ssize_t pcm_write(u_char *data, size_t count)
{
    ssize_t r;
    ssize_t result = 0;

    if (count < chunk_size)
    {
        snd_pcm_format_set_silence(hwparams.format, data + count * bits_per_frame / 8, (chunk_size - count) * hwparams.channels);
        count = chunk_size;
    }

    while (count > 0 && !in_aborting)
    {
        r = writei_func(handle, data, count);

        if (r == -EAGAIN || (r >= 0 && (size_t) r < count))
        {
            snd_pcm_wait(handle, 100);
        }
        else if (r == -EPIPE)
        {
            xrun();
        }
        else if (r == -ESTRPIPE)
        {
            suspend();
        }
        else if (r < 0)
        {
            log(FATAL, "write error: %s", snd_strerror(r));
            prg_exit(EXIT_FAILURE);
        }
        if (r > 0)
        {
            if (vumeter)
                compute_max_peak(data, r * hwparams.channels);
            result += r;
            count -= r;
            data += r * bits_per_frame / 8;
        }
    }
    return result;
}

/*
 *  read function
 */
static ssize_t pcm_read(u_char *data, size_t rcount)
{
    ssize_t r;
    size_t result = 0;
    size_t count = rcount;

    if (count != chunk_size)
    {
        count = chunk_size;
    }

    while (count > 0 && !in_aborting)
    {
        r = readi_func(handle, data, count);

        if (r == -EAGAIN || (r >= 0 && (size_t) r < count))
        {
            snd_pcm_wait(handle, 100);
        }
        else if (r == -EPIPE)
        {
            xrun();
        }
        else if (r == -ESTRPIPE)
        {
            suspend();
        }
        else if (r < 0)
        {
            log(FATAL, "read error: %s", snd_strerror(r));
            prg_exit(EXIT_FAILURE);
        }

        if (r > 0)
        {
            if (vumeter)
                compute_max_peak(data, r * hwparams.channels);
            result += r;
            count -= r;
            data += r * bits_per_frame / 8;
        }
    }
    return result;
}

/*
 *  ok, let's play a .voc file
 */

static ssize_t voc_pcm_write(u_char *data, size_t count)
{
    ssize_t result = count, r;
    size_t size;

    while (count > 0)
    {
        size = count;

        if (size > chunk_bytes - buffer_pos)
            size = chunk_bytes - buffer_pos;

        memcpy(audiobuf + buffer_pos, data, size);
        data += size;
        count -= size;
        buffer_pos += size;

        if ((size_t) buffer_pos == chunk_bytes)
        {
            if ((size_t) (r = pcm_write(audiobuf, chunk_size)) != chunk_size)
                return r;
            buffer_pos = 0;
        }
    }
    return result;
}

static void voc_write_silence(unsigned x)
{
    unsigned l;
    u_char *buf;

    buf = (u_char *) malloc(chunk_bytes);

    if (buf == NULL)
    {
        log(FATAL, "can't allocate buffer for silence");
        return; /* not fatal error */
    }

    snd_pcm_format_set_silence(hwparams.format, buf, chunk_size * hwparams.channels);

    while (x > 0 && !in_aborting)
    {
        l = x;
        if (l > chunk_size)
            l = chunk_size;
        if (voc_pcm_write(buf, l) != (ssize_t) l)
        {
            log(FATAL, "write error");
            prg_exit(EXIT_FAILURE);
        }
        x -= l;
    }

    free(buf);
}

static void voc_pcm_flush(void)
{
    if (buffer_pos > 0)
    {
        size_t b;
        if (snd_pcm_format_set_silence(hwparams.format, audiobuf + buffer_pos, chunk_bytes - buffer_pos * 8 / bits_per_sample) < 0)
            fprintf(stderr, "voc_pcm_flush - silence error");
        b = chunk_size;
        if (pcm_write(audiobuf, b) != (ssize_t) b)
            log(FATAL, "voc_pcm_flush error");
    }
    snd_pcm_nonblock(handle, 0);
    snd_pcm_drain(handle);
    snd_pcm_nonblock(handle, nonblock);
}

static void voc_play(int fd, int ofs, char *name)
{
    int l;
    VocBlockType *bp;
    VocVoiceData *vd;
    VocExtBlock *eb;
    size_t nextblock, in_buffer;
    u_char *data, *buf;
    char was_extended = 0, output = 0;
    u_short *sp, repeat = 0;
    off64_t filepos = 0;

#define COUNT(x)    nextblock -= x; in_buffer -= x; data += x
#define COUNT1(x)   in_buffer -= x; data += x

    data = buf = (u_char *) malloc(64 * 1024);
    buffer_pos = 0;
    if (data == NULL)
    {
        log(FATAL, "malloc error");
        prg_exit(EXIT_FAILURE);
    }
    if (!quiet_mode)
    {
        fprintf(stderr, "Playing Creative Labs Channel file '%s'...\n", name);
    }
    /* first we waste the rest of header, ugly but we don't need seek */
    while (ofs > (ssize_t) chunk_bytes)
    {
        if ((size_t) safe_read(fd, buf, chunk_bytes) != chunk_bytes)
        {
            log(FATAL, "read error");
            prg_exit(EXIT_FAILURE);
        }
        ofs -= chunk_bytes;
    }
    if (ofs)
    {
        if (safe_read(fd, buf, ofs) != ofs)
        {
            log(FATAL, "read error");
            prg_exit(EXIT_FAILURE);
        }
    }
    hwparams.format = DEFAULT_FORMAT;
    hwparams.channels = 1;
    hwparams.rate = DEFAULT_SPEED;
    set_params();

    in_buffer = nextblock = 0;
    while (!in_aborting)
    {
Fill_the_buffer: /* need this for repeat */
        if (in_buffer < 32)
        {
            /* move the rest of buffer to pos 0 and fill the buf up */
            if (in_buffer)
                memcpy(buf, data, in_buffer);
            data = buf;
            if ((l = safe_read(fd, buf + in_buffer, chunk_bytes - in_buffer)) > 0)
                in_buffer += l;
            else if (!in_buffer)
            {
                /* the file is truncated, so simulate 'Terminator' 
                   and reduce the datablock for safe landing */
                nextblock = buf[0] = 0;
                if (l == -1)
                {
                    perror(name);
                    prg_exit(EXIT_FAILURE);
                }
            }
        }
        while (!nextblock)
        { /* this is a new block */
            if (in_buffer < sizeof (VocBlockType))
                goto __end;
            bp = (VocBlockType *) data;
            COUNT1(sizeof (VocBlockType));
            nextblock = VOC_DATALEN(bp);
            if (output && !quiet_mode)
                fprintf(stderr, "\n"); /* write /n after ASCII-out */
            output = 0;
            switch (bp->type)
            {
                case 0:
                    return; /* VOC-file stop */
                case 1:
                    vd = (VocVoiceData *) data;
                    COUNT1(sizeof (VocVoiceData));
                    /* we need a SYNC, before we can set new SPEED, STEREO ... */

                    if (!was_extended)
                    {
                        hwparams.rate = (int) (vd->tc);
                        hwparams.rate = 1000000 / (256 - hwparams.rate);
                        if (vd->pack)
                        { /* /dev/dsp can't it */
                            log(FATAL, "can't play packed .voc files");
                            return;
                        }
                        if (hwparams.channels == 2) /* if we are in Stereo-Mode, switch back */
                            hwparams.channels = 1;
                    }
                    else
                    { /* there was extended block */
                        hwparams.channels = 2;
                        was_extended = 0;
                    }
                    set_params();
                    break;
                case 2: /* nothing to do, pure data */
                    break;
                case 3: /* a silence block, no data, only a count */
                    sp = (u_short *) data;
                    COUNT1(sizeof (u_short));
                    hwparams.rate = (int) (*data);
                    COUNT1(1);
                    hwparams.rate = 1000000 / (256 - hwparams.rate);
                    set_params();
                    voc_write_silence(*sp);
                    break;
                case 4: /* a marker for syncronisation, no effect */
                    sp = (u_short *) data;
                    COUNT1(sizeof (u_short));
                    break;
                case 5: /* ASCII text, we copy to stderr */
                    output = 1;
                    break;
                case 6: /* repeat marker, says repeatcount */
                    /* my specs don't say it: maybe this can be recursive, but
                       I don't think somebody use it */
                    repeat = *(u_short *) data;
                    COUNT1(sizeof (u_short));
                    if (filepos >= 0)
                    { /* if < 0, one seek fails, why test another */
                        if ((filepos = lseek64(fd, 0, 1)) < 0)
                        {
                            log(FATAL, "can't play loops; %s isn't seekable\n", name);
                            repeat = 0;
                        }
                        else
                        {
                            filepos -= in_buffer; /* set filepos after repeat */
                        }
                    }
                    else
                    {
                        repeat = 0;
                    }
                    break;
                case 7: /* ok, lets repeat that be rewinding tape */
                    if (repeat)
                    {
                        if (repeat != 0xFFFF)
                        {
                            --repeat;
                        }
                        lseek64(fd, filepos, 0);
                        in_buffer = 0; /* clear the buffer */
                        goto Fill_the_buffer;
                    }
                    break;
                case 8: /* the extension to play Stereo, I have SB 1.0 :-( */
                    was_extended = 1;
                    eb = (VocExtBlock *) data;
                    COUNT1(sizeof (VocExtBlock));
                    hwparams.rate = (int) (eb->tc);
                    hwparams.rate = 256000000L / (65536 - hwparams.rate);
                    hwparams.channels = eb->mode == VOC_MODE_STEREO ? 2 : 1;
                    if (hwparams.channels == 2)
                        hwparams.rate = hwparams.rate >> 1;
                    if (eb->pack)
                    { /* /dev/dsp can't it */
                        log(FATAL, "can't play packed .voc files");
                        return;
                    }

                    break;
                default:
                    log(FATAL, "unknown blocktype %d. terminate.", bp->type);
                    return;
            } /* switch (bp->type) */
        } /* while (! nextblock)  */
        /* put nextblock data bytes to dsp */
        l = in_buffer;
        if (nextblock < (size_t) l)
            l = nextblock;
        if (l)
        {
            if (output && !quiet_mode)
            {
                if (write(2, data, l) != l)
                { /* to stderr */
                    log(FATAL, "write error");
                    prg_exit(EXIT_FAILURE);
                }
            }
            else
            {
                if (voc_pcm_write(data, l) != l)
                {
                    log(FATAL, "write error");
                    prg_exit(EXIT_FAILURE);
                }
            }
            COUNT(l);
        }
    } /* while(1) */
__end:
    voc_pcm_flush();
    free(buf);
}
/* that was a big one, perhaps somebody split it :-) */

/* setting the globals for playing raw data */
static void init_raw_data(void)
{
    hwparams = rhwparams;
}

/* calculate the data count to read from/to dsp */
static off64_t calc_count(void)
{
    off64_t count;

    if (timelimit == 0)
    {
        count = pbrec_count;
    }
    else
    {
        count = snd_pcm_format_size(hwparams.format, hwparams.rate * hwparams.channels);
        count *= (off64_t) timelimit;
    }

    return count < pbrec_count ? count : pbrec_count;
}

/* write a .VOC-header */
static void begin_voc(int fd, size_t cnt)
{
    VocHeader vh;
    VocBlockType bt;
    VocVoiceData vd;
    VocExtBlock eb;

    memcpy(vh.magic, VOC_MAGIC_STRING, 20);
    vh.headerlen = LE_SHORT(sizeof (VocHeader));
    vh.version = LE_SHORT(VOC_ACTUAL_VERSION);
    vh.coded_ver = LE_SHORT(0x1233 - VOC_ACTUAL_VERSION);

    if (write(fd, &vh, sizeof (VocHeader)) != sizeof (VocHeader))
    {
        log(FATAL, "write error");
        prg_exit(EXIT_FAILURE);
    }
    if (hwparams.channels > 1)
    {
        /* write an extended block */
        bt.type = 8;
        bt.datalen = 4;
        bt.datalen_m = bt.datalen_h = 0;
        if (write(fd, &bt, sizeof (VocBlockType)) != sizeof (VocBlockType))
        {
            log(FATAL, "write error");
            prg_exit(EXIT_FAILURE);
        }
        eb.tc = LE_SHORT(65536 - 256000000L / (hwparams.rate << 1));
        eb.pack = 0;
        eb.mode = 1;
        if (write(fd, &eb, sizeof (VocExtBlock)) != sizeof (VocExtBlock))
        {
            log(FATAL, "write error");
            prg_exit(EXIT_FAILURE);
        }
    }
    bt.type = 1;
    cnt += sizeof (VocVoiceData); /* Channel_data block follows */
    bt.datalen = (u_char) (cnt & 0xFF);
    bt.datalen_m = (u_char) ((cnt & 0xFF00) >> 8);
    bt.datalen_h = (u_char) ((cnt & 0xFF0000) >> 16);
    if (write(fd, &bt, sizeof (VocBlockType)) != sizeof (VocBlockType))
    {
        log(FATAL, "write error");
        prg_exit(EXIT_FAILURE);
    }
    vd.tc = (u_char) (256 - (1000000 / hwparams.rate));
    vd.pack = 0;
    if (write(fd, &vd, sizeof (VocVoiceData)) != sizeof (VocVoiceData))
    {
        log(FATAL, "write error");
        prg_exit(EXIT_FAILURE);
    }
}

/* write a WAVE-header */
static void begin_wave(int fd, size_t cnt)
{
    WaveHeader h;
    WaveFmtBody f;
    WaveChunkHeader cf, cd;
    int bits;
    u_int tmp;
    u_short tmp2;

    /* WAVE cannot handle greater than 32bit (signed?) int */
    if (cnt == (size_t) - 2)
        cnt = 0x7fffff00;

    bits = 8;
    switch ((unsigned long) hwparams.format)
    {
        case SND_PCM_FORMAT_U8:
            bits = 8;
            break;
        case SND_PCM_FORMAT_S16_LE:
            bits = 16;
            break;
        case SND_PCM_FORMAT_S32_LE:
        case SND_PCM_FORMAT_FLOAT_LE:
            bits = 32;
            break;
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S24_3LE:
            bits = 24;
            break;
        default:
            log(FATAL, "Wave doesn't support %s format...", snd_pcm_format_name(hwparams.format));
            prg_exit(EXIT_FAILURE);
    }
    h.magic = WAV_RIFF;
    tmp = cnt + sizeof (WaveHeader) + sizeof (WaveChunkHeader) + sizeof (WaveFmtBody) + sizeof (WaveChunkHeader) - 8;
    h.length = LE_INT(tmp);
    h.type = WAV_WAVE;

    cf.type = WAV_FMT;
    cf.length = LE_INT(16);

    if (hwparams.format == SND_PCM_FORMAT_FLOAT_LE)
        f.format = LE_SHORT(WAV_FMT_IEEE_FLOAT);
    else
        f.format = LE_SHORT(WAV_FMT_PCM);
    f.channels = LE_SHORT(hwparams.channels);
    f.sample_fq = LE_INT(hwparams.rate);

    tmp2 = hwparams.channels * snd_pcm_format_physical_width(hwparams.format) / 8;
    f.byte_p_spl = LE_SHORT(tmp2);
    tmp = (u_int) tmp2 * hwparams.rate;

    f.byte_p_sec = LE_INT(tmp);
    f.bit_p_spl = LE_SHORT(bits);

    cd.type = WAV_DATA;
    cd.length = LE_INT(cnt);

    if (write(fd, &h, sizeof (WaveHeader)) != sizeof (WaveHeader) ||
            write(fd, &cf, sizeof (WaveChunkHeader)) != sizeof (WaveChunkHeader) ||
            write(fd, &f, sizeof (WaveFmtBody)) != sizeof (WaveFmtBody) ||
            write(fd, &cd, sizeof (WaveChunkHeader)) != sizeof (WaveChunkHeader))
    {
        log(FATAL, "write error");
        prg_exit(EXIT_FAILURE);
    }
}

/* write a Au-header */
static void begin_au(int fd, size_t cnt)
{
    AuHeader ah;

    ah.magic = AU_MAGIC;
    ah.hdr_size = BE_INT(24);
    ah.data_size = BE_INT(cnt);
    switch ((unsigned long) hwparams.format)
    {
        case SND_PCM_FORMAT_MU_LAW:
            ah.encoding = BE_INT(AU_FMT_ULAW);
            break;
        case SND_PCM_FORMAT_U8:
            ah.encoding = BE_INT(AU_FMT_LIN8);
            break;
        case SND_PCM_FORMAT_S16_BE:
            ah.encoding = BE_INT(AU_FMT_LIN16);
            break;
        default:
            log(FATAL, "Sparc Audio doesn't support %s format...", snd_pcm_format_name(hwparams.format));
            prg_exit(EXIT_FAILURE);
    }
    ah.sample_rate = BE_INT(hwparams.rate);
    ah.channels = BE_INT(hwparams.channels);
    if (write(fd, &ah, sizeof (AuHeader)) != sizeof (AuHeader))
    {
        log(FATAL, "write error");
        prg_exit(EXIT_FAILURE);
    }
}

/* closing .VOC */
static void end_voc(int fd)
{
    off64_t length_seek;
    VocBlockType bt;
    size_t cnt;
    char dummy = 0; /* Write a Terminator */

    if (write(fd, &dummy, 1) != 1)
    {
        log(FATAL, "write error");
        prg_exit(EXIT_FAILURE);
    }
    length_seek = sizeof (VocHeader);
    if (hwparams.channels > 1)
        length_seek += sizeof (VocBlockType) + sizeof (VocExtBlock);
    bt.type = 1;
    cnt = fdcount;
    cnt += sizeof (VocVoiceData); /* Channel_data block follows */
    if (cnt > 0x00ffffff)
        cnt = 0x00ffffff;
    bt.datalen = (u_char) (cnt & 0xFF);
    bt.datalen_m = (u_char) ((cnt & 0xFF00) >> 8);
    bt.datalen_h = (u_char) ((cnt & 0xFF0000) >> 16);
    if (lseek64(fd, length_seek, SEEK_SET) == length_seek)
        write(fd, &bt, sizeof (VocBlockType));
    if (fd != 1)
        close(fd);
}

static void end_wave(int fd)
{ /* only close output */
    WaveChunkHeader cd;
    off64_t length_seek;
    off64_t filelen;
    u_int rifflen;

    length_seek = sizeof (WaveHeader) +
            sizeof (WaveChunkHeader) +
            sizeof (WaveFmtBody);
    cd.type = WAV_DATA;
    cd.length = fdcount > 0x7fffffff ? LE_INT(0x7fffffff) : LE_INT(fdcount);
    filelen = fdcount + 2 * sizeof (WaveChunkHeader) + sizeof (WaveFmtBody) + 4;
    rifflen = filelen > 0x7fffffff ? LE_INT(0x7fffffff) : LE_INT(filelen);
    if (lseek64(fd, 4, SEEK_SET) == 4)
        write(fd, &rifflen, 4);
    if (lseek64(fd, length_seek, SEEK_SET) == length_seek)
        write(fd, &cd, sizeof (WaveChunkHeader));
    if (fd != 1)
        close(fd);
}

static void end_au(int fd)
{ /* only close output */
    AuHeader ah;
    off64_t length_seek;

    length_seek = (char *) &ah.data_size - (char *) &ah;
    ah.data_size = fdcount > 0xffffffff ? 0xffffffff : BE_INT(fdcount);
    if (lseek64(fd, length_seek, SEEK_SET) == length_seek)
        write(fd, &ah.data_size, sizeof (ah.data_size));
    if (fd != 1)
        close(fd);
}

static void header(int rtype, char *name)
{
    if (!quiet_mode)
    {
        if (!name)
            name = (stream == SND_PCM_STREAM_PLAYBACK) ? "stdout" : "stdin";
        fprintf(stderr, "%s %s '%s' : ",
                (stream == SND_PCM_STREAM_PLAYBACK) ? "Playing" : "Recording",
                fmt_rec_table[rtype].what,
                name);
        fprintf(stderr, "%s, ", snd_pcm_format_description(hwparams.format));
        fprintf(stderr, "Rate %d Hz, ", hwparams.rate);
        if (hwparams.channels == 1)
            fprintf(stderr, "Mono");
        else if (hwparams.channels == 2)
            fprintf(stderr, "Stereo");
        else
            fprintf(stderr, "Channels %i", hwparams.channels);
        fprintf(stderr, "\n");
    }
}

/* playing raw data */

static void playback_go(int fd, size_t loaded, off64_t count, int rtype, char *name)
{
    int l, r;
    off64_t written = 0;
    off64_t c;

    header(rtype, name);
    set_params();

    while (loaded > chunk_bytes && written < count && !in_aborting)
    {
        if (pcm_write(audiobuf + written, chunk_size) <= 0)
            return;
        written += chunk_bytes;
        loaded -= chunk_bytes;
    }

    if (written > 0 && loaded > 0)
        memmove(audiobuf, audiobuf + written, loaded);

    l = loaded;

    while (written < count && !in_aborting)
    {
        do
        {
            c = count - written;
            if (c > chunk_bytes)
                c = chunk_bytes;
            c -= l;

            if (c == 0)
                break;

            r = safe_read(fd, audiobuf + l, c);

            if (r < 0)
            {
                perror(name);
                prg_exit(EXIT_FAILURE);
            }

            fdcount += r;

            if (r == 0)
                break;

            l += r;
        }

        while ((size_t) l < chunk_bytes);
        l = l * 8 / bits_per_frame;
        r = pcm_write(audiobuf, l);

        if (r != l)
            break;

        r = r * bits_per_frame / 8;
        written += r;
        l = 0;
    }

    snd_pcm_nonblock(handle, 0);
    snd_pcm_drain(handle);
    snd_pcm_nonblock(handle, nonblock);
}

/*
 *  let's play or capture it (capture_type says VOC/WAVE/raw)
 */
static void playback(char *name)
{
    int ofs;
    size_t dta;
    ssize_t dtawave;

    pbrec_count = LLONG_MAX;
    fdcount = 0;

    if (!name || !strcmp(name, "-"))
    {
        fd = fileno(stdin);
        name = "stdin";
    }
    else
    {
        if ((fd = open(name, O_RDONLY, 0)) == -1)
        {
            perror(name);
            prg_exit(EXIT_FAILURE);
        }
    }

    /* read the file header */
    dta = sizeof (AuHeader);

    if ((size_t) safe_read(fd, audiobuf, dta) != dta)
    {
        log(FATAL, "read error");
        prg_exit(EXIT_FAILURE);
    }

    if (test_au(fd, audiobuf) >= 0)
    {
        rhwparams.format = hwparams.format;
        pbrec_count = calc_count();
        playback_go(fd, 0, pbrec_count, FORMAT_AU, name);
        goto __end;
    }

    dta = sizeof (VocHeader);

    if ((size_t) safe_read(fd, audiobuf + sizeof (AuHeader),
            dta - sizeof (AuHeader)) != dta - sizeof (AuHeader))
    {
        log(FATAL, "read error");
        prg_exit(EXIT_FAILURE);
    }

    if ((ofs = test_vocfile(audiobuf)) >= 0)
    {
        pbrec_count = calc_count();
        voc_play(fd, ofs, name);
        goto __end;
    }

    /* read bytes for WAVE-header */
    if ((dtawave = test_wavefile(fd, audiobuf, dta)) >= 0)
    {
        pbrec_count = calc_count();
        playback_go(fd, dtawave, pbrec_count, FORMAT_WAVE, name);
    }
    else
    {
        /* should be raw data */
        init_raw_data();
        pbrec_count = calc_count();
        playback_go(fd, dta, pbrec_count, FORMAT_RAW, name);
    }

__end:
    if (fd != 0)
        close(fd);
}

/**
 * mystrftime
 *
 *   Variant of strftime(3) that supports additional format
 *   specifiers in the format string.
 *
 * Parameters:
 *
 *   s    - destination string
 *   max    - max number of bytes to write
 *   userformat - format string
 *   tm  - time information
 *   filenumber - the number of the file, starting at 1
 *
 * Returns: number of bytes written to the string s
 */
size_t mystrftime(char *s, size_t max, const char *userformat,
        const struct tm *tm, const int filenumber)
{
    char formatstring[PATH_MAX] = "";
    char tempstring[PATH_MAX] = "";
    char *format, *tempstr;
    const char *pos_userformat;

    format = formatstring;

    /* if mystrftime is called with userformat = NULL we return a zero length string */
    if (userformat == NULL)
    {
        *s = '\0';
        return 0;
    }

    for (pos_userformat = userformat; *pos_userformat; ++pos_userformat)
    {
        if (*pos_userformat == '%')
        {
            tempstr = tempstring;
            tempstr[0] = '\0';
            switch (*++pos_userformat)
            {

                case '\0': // end of string
                    --pos_userformat;
                    break;

                case 'v': // file number 
                    sprintf(tempstr, "%02d", filenumber);
                    break;

                default: // All other codes will be handled by strftime
                    *format++ = '%';
                    *format++ = *pos_userformat;
                    continue;
            }

            /* If a format specifier was found and used, copy the result. */
            if (tempstr[0])
            {
                while ((*format = *tempstr++) != '\0')
                    ++format;
                continue;
            }
        }

        /* For any other character than % we simply copy the character */
        *format++ = *pos_userformat;
    }

    *format = '\0';
    format = formatstring;
    return strftime(s, max, format, tm);
}

static int new_capture_file(char *name, char *namebuf, size_t namelen,
        int filecount)
{
    char *s;
    char buf[PATH_MAX + 1];
    time_t t;
    struct tm *tmp;

    /* get a copy of the original filename */
    strncpy(buf, name, sizeof (buf));

    /* separate extension from filename */
    s = buf + strlen(buf);
    while (s > buf && *s != '.' && *s != '/')
        --s;
    if (*s == '.')
        *s++ = 0;
    else if (*s == '/')
        s = buf + strlen(buf);

    /* upon first jump to this if block rename the first file */
    if (filecount == 1)
    {
        if (*s)
            snprintf(namebuf, namelen, "%s-01.%s", buf, s);
        else
            snprintf(namebuf, namelen, "%s-01", buf);
        remove(namebuf);
        rename(name, namebuf);
        filecount = 2;
    }

    /* name of the current file */
    if (*s)
        snprintf(namebuf, namelen, "%s-%02i.%s", buf, filecount, s);
    else
        snprintf(namebuf, namelen, "%s-%02i", buf, filecount);

    return filecount;
}

/**
 * create_path
 *
 *   This function creates a file path, like mkdir -p. 
 *
 * Parameters:
 *
 *   path - the path to create
 *
 * Returns: 0 on success, -1 on failure
 * On failure, a message has been printed to stderr.
 */
int create_path(const char *path)
{
    char *start;
    mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

    if (path[0] == '/')
        start = strchr(path + 1, '/');
    else
        start = strchr(path, '/');

    while (start)
    {
        char *buffer = strdup(path);
        buffer[start - path] = 0x00;

        if (mkdir(buffer, mode) == -1 && errno != EEXIST)
        {
            fprintf(stderr, "Problem creating directory %s", buffer);
            perror(" ");
            free(buffer);
            return -1;
        }
        free(buffer);
        start = strchr(start + 1, '/');
    }
    return 0;
}

static int safe_open(const char *name)
{
    int fd;

    fd = open(name, O_WRONLY | O_CREAT, 0644);
    if (fd == -1)
    {
        if (errno != ENOENT)
            return -1;
        if (create_path(name) == 0)
            fd = open(name, O_WRONLY | O_CREAT, 0644);
    }
    return fd;
}

static void capture(char *orig_name)
{
    int filecount = 0; /* number of files written */
    char *name = orig_name; /* current filename */
    char namebuf[PATH_MAX + 1];
    off64_t count, rest; /* number of bytes to capture */

    /* get number of bytes to capture */

    count = calc_count();
    if (count == 0)
        count = LLONG_MAX;

    /* WAVE-file should be even (I'm not sure), but wasting one byte
       isn't a problem (this can only be in 8 bit mono) */
    if (count < LLONG_MAX)
        count += count % 2;
    else
        count -= count % 2;

    /* display verbose output to console */
    header(file_type, name);

    /* setup sound hardware */
    set_params();

    do
    {
        /* open a file to write */
        /* upon the second file we start the numbering scheme */
        if (filecount)
        {
            filecount = new_capture_file(orig_name, namebuf, sizeof (namebuf), filecount);
            name = namebuf;
        }

        /* open a new file */
        remove(name);
        fd = safe_open(name);

        if (fd < 0)
        {
            perror(name);
            prg_exit(EXIT_FAILURE);
        }
        filecount++;

        rest = count;

        if (rest > fmt_rec_table[file_type].max_filesize)
            rest = fmt_rec_table[file_type].max_filesize;

        /* setup sample header */
        if (fmt_rec_table[file_type].start)
            fmt_rec_table[file_type].start(fd, rest);

        /* capture */
        fdcount = 0;

        while (rest > 0 && !in_aborting)
        {
            size_t c = (rest <= (off64_t) chunk_bytes) ? (size_t) rest : chunk_bytes;
            size_t f = c * 8 / bits_per_frame;

            if (pcm_read(audiobuf, f) != f)
                break;

            if (write(fd, audiobuf, c) != c)
            {
                perror(name);
                prg_exit(EXIT_FAILURE);
            }

            count -= c;
            rest -= c;
            fdcount += c;
        }

        /* finish sample container */
        if (fmt_rec_table[file_type].end)
        {
            fmt_rec_table[file_type].end(fd);
            fd = -1;
        }

        if (in_aborting)
            break;

        /* repeat the loop when format is raw without timelimit or
         * requested counts of data are recorded
         */
    }
    while ((file_type == FORMAT_RAW && !timelimit) || count > 0);
}
