#include <string>

enum Level {
    VERBOSE, //lowest priority
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL,
    SILENT, // highest priority, on which nothing is ever printed
};

static inline std::string str_level(Level l)
{
    std::string levels[] = { "VERBOSE", "DEBUG", "INFO", "WARNING", "ERROR", "FATAL", "SILENT" };
    return levels[l];
}

void log(Level level, std::string str, ...)
{

}