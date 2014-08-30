#ifndef MIXER_H
#define MIXER_H 1

#include <alsa/asoundlib.h>
#include <functional>
#include <string>

#include "utils.hpp"

#define CARD_NAME "default"

/* GCW sources */
enum Source { PCM, LINE_IN, MIC };
enum Direction { CAPTURE, PLAYBACK };

class Mixer
{
private:
    void with_mixer(std::function<void(snd_mixer_t*, snd_mixer_selem_id_t*)> func);
    void init();
public:
    Mixer();
    ~Mixer();
    long get_volume(std::string source, Direction direction);
    void switch_value(std::string source, int value, Direction direction);
    void set_enum(std::string source, int value);
    void set_volume(std::string source, long vol, Direction direction);
    void set_mic_volume(long i);
    void set_speaker_volume(long i);
    long get_mic_volume();
    long get_speaker_volume();
    void set_direction(Direction direction);
};

#endif