#include "logger.h"

#include <time.h>



bool is_symbol_ok(char c)
{
    return (('0' <= c) && (c <= '9')) ||
           (('a' <= c) && (c <= 'z')) ||
           (('A' <= c) && (c <= 'Z')) ||
            (c == '.') || (c == ',')  ||
            (c == ':') || (c == ';')  ||
            (c == '!') || (c == '?')  ||
            (c == '@') || (c == '#')  ||
            (c == '$') || (c == '%')  ||
            (c == '^') || (c == '&')  ||
            (c == '*') || (c == '~')  ||
            (c == '(') || (c == ')')  ||
            (c == '<') || (c == '>')  ||
            (c == '[') || (c == ']')  ||
            (c == '{') || (c == '}')  ||
            (c == '-') || (c == '+')  ||
            (c == '/') || (c == '\\') ||
            (c == '"') || (c == '\'') ||
            (c == '`') || (c == '=')  ||
            (c == ' ') || (c == '\n') ||
            (c == '\r')|| (c == '\t');
}


void logger__log(struct logger *logger, char const *fmt, ...)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    va_list args;
    va_start(args, fmt);
#if DEBUG
    printf("[%d-%02d-%02d %02d:%02d:%02d] ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    vprintf(fmt, args);
#else
    string_builder__append_format(&logger->sb, "[%d-%02d-%02d %02d:%02d:%02d] ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    string_builder__append_format_va_list(&logger->sb, fmt, args);
    if (logger->sb.used > (logger->sb.memory.size - KILOBYTES(1)))
        logger__flush(logger);
#endif
    va_end(args);
}


void logger__log_untrusted(struct logger *logger, char const *buffer, usize size)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    printf("[%d-%02d-%02d %02d:%02d:%02d] ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    for (usize i = 0; i < size; i++)
    {
        char c = buffer[i];
#if DEBUG
        if (is_symbol_ok(c))
            printf("%c", c);
        else
            printf("\\0x%x", c);
#else
        if (is_symbol_ok(c))
            string_builder__append_format(&logger->sb, "%c", c);
        else
            string_builder__append_format(&logger->sb, "\\0x%x", c);
#endif
    }
    
#if DEBUG
#else
    if (logger->sb.used > (logger->sb.memory.size - KILOBYTES(1)))
        logger__flush(logger);
#endif
}


void logger__flush(struct logger *logger)
{
    int fd = open(logger->filename, O_NOFOLLOW | O_CREAT | O_APPEND | O_RDWR, 0666);
    if (fd < 0)
    {
        return;
    }

    struct stat st;
    int fstat_result = fstat(fd, &st);
    if (fstat_result < 0)
    {
        return;
    }

    if (st.st_size > LOG_FILE_MAX_SIZE)
    {
        close(fd);

        char new_name_buffer[512];
        memory__set(new_name_buffer, 0, sizeof(new_name_buffer));
        memory__copy(new_name_buffer, logger->filename, cstring__size_no0(logger->filename));
        memory__copy(new_name_buffer + cstring__size_no0(logger->filename), ".1", 2);

        rename(logger->filename, new_name_buffer);

        fd = open(logger->filename, O_NOFOLLOW | O_CREAT | O_TRUNC | O_WRONLY, 0666);
        if (fd < 0)
        {
            return;
        }
    }

    memory_block string_to_write = string_builder__get_string(&logger->sb);
    isize bytes_written = write(fd, string_to_write.memory, string_to_write.size);
    if (bytes_written < 0)
    {
        fprintf(stderr, "Error write logger file (errno: %d - \"%s\")\n", errno, strerror(errno));
    }
    string_builder__reset(&logger->sb);
}
