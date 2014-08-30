#include "mic.h"

Mic::Mic()
{
    is_running = false;
}

Mic::~Mic()
{
}

void Mic::capture(std::string file_name, FileType type, FileFormat format,
                  Channel channel, Rate rate, int duration)
{
    if (is_running) return;

    this->file_name = file_name;
    is_running = true;
}

void Mic::play(std::string file_name)
{
    if (is_running) return;

    this->file_name = file_name;
    is_running = true;
}

void Mic::pause()
{
    if (!is_running) return;
    is_running = false;
}

void Mic::stop()
{
    if (!is_running) return;
    is_running = false;
}

void Mic::set_on_terminate_event(void (*event)())
{
}

void Mic::set_on_vu_change_event(void (*event)(signed int, signed int))
{
}