#ifndef LOG_H
#define LOG_H

typedef enum {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL,
} level;

#define log(level, ...) do {                                                         \
    fprintf(stderr, "[%s] %s:%d at %s: ", #level, __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, __VA_ARGS__);                                                    \
    putc('\n', stderr);                                                              \
    if (level == FATAL) exit(1);                                                     \
} while (0)

#endif