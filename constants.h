#ifndef CONSTANTS_H
#define CONSTANTS_H 1

#include <string>

enum class FileType { AU, RAW, SPARC, VOC, WAV };

enum class FileFormat { CD, CDR, DAT };

enum class Channel { MONO, STEREO };

enum class Rate
{
    R_8000,
    R_11025,
    R_16000,
    R_22050,
    R_32000,
    R_44056,
    R_44100,
    R_47250,
    R_48000,
    R_50000,
    R_50400,
    R_88200,
    R_96000,
    R_176400,
    R_192000
};

#endif