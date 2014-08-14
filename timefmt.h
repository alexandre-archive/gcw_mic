#include <time.h>
#include <string>

std::string current_time(std::string fmt = "%d-%m-%Y_%H-%M-%S")
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