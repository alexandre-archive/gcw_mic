#include <string>

class Mic
{
private:
    bool        is_running;
    std::string file_name;
public:
    Mic();
    ~Mic();
    void record(std::string file_name);
    void play(std::string file_name);
    void pause();
    void stop();
    void set_mic_volume(long i);
    void set_speaker_volume(long i);
    long get_mic_volume();
    long get_speaker_volume();
    void set_on_terminate_event(void (*event)());
};
