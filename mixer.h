#include <alsa/asoundlib.h>
#include <functional>

#define CARD_NAME "default"

/* GCW sources */
typedef enum { PCM, LINE_IN, MIC } source;
typedef enum { CAPTURE, PLAYBACK } mixer_direction;

static long convert_volume_space(long value, long min, long max, long old_min, long old_max)
{
    if (value > old_max)
    {
        value = old_max;
    }
    else if (value < old_min)
    {
        value = old_min;
    }

    return (((value - old_min) * (max - min)) / (old_max - old_min)) + min;
}

class Mixer
{
private:
    void with_mixer(std::function<void(snd_mixer_t*, snd_mixer_selem_id_t*)> func);
    void init();
public:
    Mixer();
    ~Mixer();
    long get_volume(char* source, mixer_direction direction);
    void switch_value(char* source, int value, mixer_direction direction);
    void set_enum(char* source, int value);
    void set_volume(char* source, long vol, mixer_direction direction);
    void set_mic_volume(long i);
    void set_speaker_volume(long i);
    long get_mic_volume();
    long get_speaker_volume();
    void set_direction(mixer_direction direction);
};