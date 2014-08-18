#include "alsawrapper.h"
#include "mic.h"

Mic::Mic()
{
    is_running = false;
}

Mic::~Mic()
{
}

void Mic::record(std::string file_name)
{
    if (is_running) return;
    this->file_name = file_name;
    is_running = true;
    alsawrapper_init(CMD_RECORD, TYPE_WAVE, FMT_DAT, VU_STEREO, 2, 48000, 0, false, const_cast<char*>(this->file_name.c_str()));
    alsawrapper_start();
}

void Mic::play(std::string file_name)
{
    if (is_running) return;
    this->file_name = file_name;
    is_running = true;
    alsawrapper_init(CMD_PLAY, TYPE_WAVE, FMT_DAT, VU_STEREO, 2, 48000, 0, false, const_cast<char*>(this->file_name.c_str()));
    alsawrapper_start();
}

void Mic::pause()
{
    if (!is_running) return;
    is_running = false;
    alsawrapper_stop();
}

void Mic::stop()
{
    if (!is_running) return;
    is_running = false;
    alsawrapper_stop();
}

void set_on_terminate_event(void *event)
{
   
}
