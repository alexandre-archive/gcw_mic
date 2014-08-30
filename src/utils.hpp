#ifndef UTILS_H
#define UTILS_H 1

#include <cstdlib>
#include <dirent.h>
#include <iostream>
#include <time.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>

#include "log.h"

#define stringfy(value) #value

inline std::string fmt_current_time(std::string fmt = "%d-%m-%Y_%H-%M-%S")
{
    time_t rawtime;
    struct tm *timeinfo;
    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, 80, fmt.c_str(), timeinfo);
    std::string str(buffer);

    return str;
}

/*
    WORKAROUND for GCW toolchain.
    http://www.cplusplus.com/forum/general/109469/
*/
template <typename T>
inline std::string to_string(T value)
{
    std::ostringstream os ;
    os << value ;
    return os.str();
}

inline void create_app_dir(std::string path)
{
    DIR *dir = opendir(path.c_str());

    if (dir)
    {
        closedir(dir);
    }
    else
    {
        if (mkdir(path.c_str(), 0777) != 0)
        {
            log(FATAL, "Cannot create application directory.");
        }
    }
}

inline long convert_space(long value, long min, long max, long old_min, long old_max)
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

#endif