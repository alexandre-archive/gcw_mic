#ifndef CONFIG_H
#define CONFIG_H 1

#include <string>
#include "constants.hpp"

class Config
{
private:
    std::string file_name;
    FileType    file_type;
    FileFormat  file_format;
    Channel     channel;
    Rate        rate;
public:
    Config();
    ~Config();
    void load();
    void save();
    std::string get_file_name();
    std::string get_new_file_name();
    FileType    get_file_type();
    FileFormat  get_file_format();
    Channel     get_channel();
    Rate        get_rate();
    void        set_file_name(std::string s);
    void        set_file_type(FileType f);
    void        set_file_format(FileFormat f);
    void        set_channel(Channel c);
    void        set_rate(Rate r);
};

#endif