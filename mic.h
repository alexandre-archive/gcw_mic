#ifndef MIC_H
#define MIC_H 1

#include <string>
#include "constants.h"

class Mic
{
private:
    bool        is_running;
    std::string file_name;
public:
    Mic();
    ~Mic();
    void capture(std::string file_name, FileType type, FileFormat format,
                 Channel channel, Rate rate, int duration = 0);
    void play(std::string file_name);
    void pause();
    void stop();
    void set_on_terminate_event(void (*event)());
    void set_on_vu_change_event(void (*event)(signed int, signed int));
};

#endif