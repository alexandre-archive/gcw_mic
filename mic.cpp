#include "alsawrapper.h"
#include "mic.h"

Mic::Mic()
{
    is_running = false;
    configure_mixer();
}

Mic::~Mic()
{
}

void Mic::record(std::string file_name)
{
    if (is_running) return;
    this->file_name = file_name;
    is_running = true;
    alsawrapper_init(CMD_RECORD, TYPE_WAVE, FMT_CD, VU_STEREO, 2, 96000, 0, false, const_cast<char*>(this->file_name.c_str()));
    alsawrapper_start();
}

void Mic::play(std::string file_name)
{
    if (is_running) return;
    this->file_name = file_name;
    is_running = true;
    alsawrapper_init(CMD_PLAY, TYPE_WAVE, FMT_CD, VU_STEREO, 2, 96000, 0, false, const_cast<char*>(this->file_name.c_str()));
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

void Mic::set_on_terminate_event(void (*event)())
{
   alsawrapper_on_terminate(event);
}

void Mic::set_on_vu_change_event(void (*event)(signed int, signed int))
{
    alsawrapper_on_vu_change(event);
}

void Mic::set_mic_volume(long i)
{

}

void Mic::set_speaker_volume(long i)
{
    alsawrapper_set_volume(i);
}

long Mic::get_mic_volume()
{

}

long Mic::get_speaker_volume()
{

}
