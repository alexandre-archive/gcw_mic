#ifndef APLAY_H
#define APLAY_H 1

#include <pthread.h>
#include <alsa/asoundlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "constants.hpp"
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

#define CH_MONO   1
#define CH_STEREO 2

static snd_pcm_sframes_t (*readi_func)(snd_pcm_t *handle, void *buffer, snd_pcm_uframes_t size);
static snd_pcm_sframes_t (*writei_func)(snd_pcm_t *handle, const void *buffer, snd_pcm_uframes_t size);
static snd_pcm_sframes_t (*readn_func)(snd_pcm_t *handle, void **bufs, snd_pcm_uframes_t size);
static snd_pcm_sframes_t (*writen_func)(snd_pcm_t *handle, void **bufs, snd_pcm_uframes_t size);

#ifndef SND_PCM_ABORT
    static inline int snd_pcm_abort(snd_pcm_t *pcm) { return snd_pcm_nonblock(pcm, 2); }
#endif

#define APLAY_SECOND 176384

static struct
{
    snd_pcm_format_t format;
    unsigned int channels;
    unsigned int rate;
} hwparams, rhwparams;

void pcm_capture(const char* file_name, FileType type, FileFormat format, Channel channel, Rate rate, int duration);
void pcm_play(const char* file_name);
void pcm_stop();

#endif