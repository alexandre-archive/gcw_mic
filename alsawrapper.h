#include "log.h"

#define CMD_PLAY   "aplay"
#define CMD_RECORD "arecord"

#define TYPE_AU    "au"
#define TYPE_RAW   "raw"
#define TYPE_SPARC "sparc"
#define TYPE_VOC   "voc"
#define TYPE_WAVE  "wav"

#define FMT_CD  "cd"
#define FMT_CDR "cdr"
#define FMT_DAT "dat"

/* GCW sources */
typedef enum { PCM, LINE_IN, MIC } source;

#ifndef __cplusplus
    typedef enum { false, true } bool;
#endif

#ifdef __cplusplus
    extern "C" {
#endif

void configure_mixer();
void alsawrapper_init(char* command, char* type, char* file_format,
                      int channels, int rate, int duration,
                      bool separate_channels, char* file_n);
void alsawrapper_start();
void alsawrapper_stop();
void alsawrapper_on_terminate(void (*event)());
void alsawrapper_on_vu_change(void (*event)(signed int, signed int));
void alsawrapper_set_speaker_volume(long vol);
long alsawrapper_get_speaker_volume();

#ifdef __cplusplus
    }
#endif
