#include "config.hpp"
#include "utils.hpp"

Config::Config()
{
    file_name = "rec_%d-%m-%Y_%H-%M-%S";
    file_type = FileType::AU;
    file_format = FileFormat::DAT;
    channel = Channel::STEREO;
    rate = Rate::R_44100;
}

Config::~Config()
{

}

void Config::load()
{

}

void Config::save()
{

}

std::string Config::get_file_name()
{
    return file_name;
}

std::string Config::get_new_file_name()
{
    return fmt_current_time(file_name) + get_ext(file_type);
}

FileType Config::get_file_type()
{
    return file_type;
}

FileFormat Config::get_file_format()
{
    return file_format;
}

Channel Config::get_channel()
{
    return channel;
}

Rate Config::get_rate()
{
    return rate;
}

void Config::set_file_name(std::string s)
{
    file_name = s;
}

void Config::set_file_type(FileType f)
{
    file_type = f;
}

void Config::set_file_format(FileFormat f)
{
    file_format = f;
}

void Config::set_channel(Channel c)
{
    channel = c;
}

void Config::set_rate(Rate r)
{
    rate = r;
}
