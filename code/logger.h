#ifndef LOGGER_H
#define LOGGER_H

#include <base.h>


struct logger
{
    char const *filename;
    string_builder sb;
};

void logger__log(struct logger *logger, char const *fmt, ...);
void logger__log_untrusted(struct logger *logger, char const *buffer, usize size);
void logger__flush(struct logger *logger);

#define LOG(FORMAT, ...) logger__log(logger, (FORMAT) VA_ARGS(__VA_ARGS__))
#define LOG_UNTRUSTED(BUFFER, SIZE) logger__log_untrusted(logger, (char const *)(BUFFER), (SIZE))


#endif // LOGGER_H
