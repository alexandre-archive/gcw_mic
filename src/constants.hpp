#ifndef CONSTANTS_H
#define CONSTANTS_H 1

#include <string>

enum class FileType { AU, RAW, SPARC, VOC, WAV };

enum class FileFormat { CD, CDR, DAT };

enum class Channel { MONO, STEREO };

enum class Rate : unsigned int
{
    R_8000   = 8000,
    R_11025  = 11025,
    R_16000  = 16000,
    R_22050  = 22050,
    R_32000  = 32000,
    R_44056  = 44056,
    R_44100  = 44100,
    R_47250  = 47250,
    R_48000  = 48000,
    R_50000  = 50000,
    R_50400  = 50400,
    R_88200  = 88200,
    R_96000  = 96000,
    R_176400 = 176400,
    R_192000 = 192000
};


struct UpdateEventArgs
{
};

struct TerminateEventArgs
{
};

#endif