// Based
#include <base.h>
#include <integer.h>
#include <memory.h>
#include <memory_allocator.h>
#include <string_builder.h>

// *nix
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// Stdlib
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

// Project-specific
#include "async_queue.h"


const char payload_template[] =
"HTTP/1.0 200 OK\n"
"Content-Length: %lld\n"
"Content-Type: text/html\n"
"\n";

const char payload_500[] =
"HTTP/1.0 500 Internal Server Error\n"
"Content-Length: 237\n"
"Content-Type: text/html\n"
"\n"
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"    <meta charset=\"utf-8\">\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
"    <title>500 - Internal Server Error</title>\n"
"</head>\n"
"<body>\n"
"500 - Internal Server Error\n"
"</body>\n"
"</html>\n"
;


#define BACKLOG_SIZE 32
#define LOG_FILE_MAX_SIZE MEGABYTES(1)

#define IP4_ANY 0
#define IP4(x, y, z, w) ((((uint8) w) << 24) | (((uint8) z) << 16) | (((uint8) y) << 8) | ((uint8) x))
#define IP4_LOCALHOST IP4(127, 0, 0, 1)

struct logger
{
    char const *filename;
    string_builder sb;
};

void logger__flush(struct logger *logger);

#if DEBUG
#define LOG(LOGGER, FORMAT, ...) \
    do { \
        UNUSED(LOGGER); \
        time_t t = time(NULL); \
        struct tm tm = *localtime(&t); \
        printf("[%d-%02d-%02d %02d:%02d:%02d] ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec); \
        printf(FORMAT VA_ARGS(__VA_ARGS__)); \
    } while (0)
#define LOG_UNTRUSTED(LOGGER, BUFFER, SIZE) \
    do { \
        UNUSED(LOGGER); \
        time_t t = time(NULL); \
        struct tm tm = *localtime(&t); \
        printf("[%d-%02d-%02d %02d:%02d:%02d]\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec); \
        for (int i = 0; i < (SIZE); i++) \
        { \
            char c = (BUFFER)[i]; \
            if ((('0' <= c) && (c <= '9')) || \
                (('a' <= c) && (c <= 'z')) || \
                (('A' <= c) && (c <= 'Z')) || \
                 (c == '.') || (c == ',')  || \
                 (c == ':') || (c == ';')  || \
                 (c == '!') || (c == '?')  || \
                 (c == '@') || (c == '#')  || \
                 (c == '$') || (c == '%')  || \
                 (c == '^') || (c == '&')  || \
                 (c == '*') || (c == '~')  || \
                 (c == '(') || (c == ')')  || \
                 (c == '<') || (c == '>')  || \
                 (c == '[') || (c == ']')  || \
                 (c == '{') || (c == '}')  || \
                 (c == '-') || (c == '+')  || \
                 (c == '/') || (c == '\\') || \
                 (c == '"') || (c == '\'') || \
                 (c == '`') || (c == '=')  || \
                 (c == ' ') || (c == '\n') || \
                 (c == '\r')|| (c == '\t'))   \
            { printf("%c", c); } else { printf("\\0x%x", c); } \
        } \
    } while (0)
#else
#define LOG(LOGGER, FORMAT, ...) \
    do { \
        time_t t = time(NULL); \
        struct tm tm = *localtime(&t); \
        string_builder__append_format(&(LOGGER)->sb, "[%d-%02d-%02d %02d:%02d:%02d] ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec); \
        string_builder__append_format(&(LOGGER)->sb, FORMAT VA_ARGS(__VA_ARGS__)); \
        if ((LOGGER)->sb.used > ((LOGGER)->sb.memory.size - KILOBYTES(1))) logger__flush(LOGGER); \
    } while (0)
#define LOG_UNTRUSTED(LOGGER, BUFFER, SIZE) \
    do { \
        time_t t = time(NULL); \
        struct tm tm = *localtime(&t); \
        string_builder__append_format(&(LOGGER)->sb, "[%d-%02d-%02d %02d:%02d:%02d]\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec); \
        for (int i = 0; i < (SIZE); i++) \
        { \
            char c = (BUFFER)[i]; \
            if ((('0' <= c) && (c <= '9')) || \
                (('a' <= c) && (c <= 'z')) || \
                (('A' <= c) && (c <= 'Z')) || \
                 (c == '.') || (c == ',')  || \
                 (c == ':') || (c == ';')  || \
                 (c == '!') || (c == '?')  || \
                 (c == '@') || (c == '#')  || \
                 (c == '$') || (c == '%')  || \
                 (c == '^') || (c == '&')  || \
                 (c == '*') || (c == '~')  || \
                 (c == '(') || (c == ')')  || \
                 (c == '<') || (c == '>')  || \
                 (c == '[') || (c == ']')  || \
                 (c == '{') || (c == '}')  || \
                 (c == '-') || (c == '+')  || \
                 (c == '/') || (c == '\\') || \
                 (c == '"') || (c == '\'') || \
                 (c == '`') || (c == '=')  || \
                 (c == ' ') || (c == '\n') || \
                 (c == '\r')|| (c == '\t'))   \
            { string_builder__append_format(&(LOGGER)->sb, "%c", c); } else { string_builder__append_format(&(LOGGER)->sb, "\\0x%x", c); } \
        } \
        if ((LOGGER)->sb.used > ((LOGGER)->sb.memory.size - KILOBYTES(1))) logger__flush(LOGGER); \
    } while (0)
#endif


memory_block load_file(memory_allocator allocator, char const *filename);
int make_socket_nonblocking(int fd);
int accept_connection(struct async_context *context, memory_allocator arena, struct logger *logger, int server_socket);
int accepted_socket_ready_to_read(struct async_context *context, memory_allocator arena, struct logger *logger, int accepted_socket);
int send_payload(memory_allocator arena, struct logger *logger, int accepted_socket);
memory_block make_http_response(memory_allocator allocator, string_builder *sb);



int main()
{
    usize memory_size = MEGABYTES(5);
    void *memory = malloc(memory_size);
    memory__set(memory, 0, memory_size);

    memory_block global_memory = { .memory = memory, .size = memory_size };
    memory_allocator global_arena = make_memory_arena(global_memory);
    memory_allocator connection_arena = allocate_memory_arena(global_arena, MEGABYTES(1));

    struct logger logger = {
        .filename = "log.txt",
        .sb = {
            .memory = ALLOCATE_BUFFER(global_arena, MEGABYTES(1)),
            .used = 0,
        },
    };

    struct async_context *context = create_async_context();
    if (context == NULL)
    {
        LOG(&logger, "Error async queue\n");
        return EXIT_FAILURE;
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        LOG(&logger, "Error socket (errno: %d - \"%s\")\n", errno, strerror(errno));
        return EXIT_FAILURE;
    }
    else
    {
        int nonblock_result = make_socket_nonblocking(server_socket);
        if (nonblock_result < 0)
        {
            LOG(&logger, "Error: make_socket_nonblocking\n");
        }
        else
        {
            struct sockaddr_in address = {
                .sin_family      = AF_INET,
                .sin_port        = uint16__change_endianness(80),
                .sin_addr.s_addr = IP4_ANY,
            };

            int bind_result = bind(server_socket, (const struct sockaddr *) &address, sizeof(address));
            if (bind_result < 0)
            {
                LOG(&logger, "Error bind (errno: %d - \"%s\")\n", errno, strerror(errno));
            }
            else
            {
                int listen_result = listen(server_socket, BACKLOG_SIZE);
                if (listen_result < 0)
                {
                    LOG(&logger, "Error listen (errno: %d - \"%s\")\n", errno, strerror(errno));
                }
                else
                {
                    int register_result = register_socket_to_read(context, server_socket, SOCKET_EVENT__INCOMING_CONNECTION);
                    if (register_result < 0)
                    {
                        if (register_result == -2)
                            LOG(&logger, "Error coult not add to the kqueue because all %d slots in the array are occupied\n", MAX_EVENTS);
                        if (register_result == -1)
                            LOG(&logger, "Error kregister_accepted_socket_result (errno: %d - \"%s\")\n", errno, strerror(errno));
                    }
                    else
                    {
                        while(true)
                        {
                            struct socket_event_data *event = wait_for_new_events(context);
                            if (event->type == SOCKET_EVENT__INCOMING_CONNECTION)
                            {
                                LOG(&logger, "Incoming connection event...\n");
                                int accept_connection_result = accept_connection(context, connection_arena, &logger, server_socket);
                                if (accept_connection_result < 0)
                                {
                                    LOG(&logger, "Could not accept connection (errno: %d - \"%s\")\n", errno, strerror(errno));
                                }
                            }
                            else if (event->type == SOCKET_EVENT__INCOMING_MESSAGE)
                            {
                                memory_arena__reset(connection_arena);

                                LOG(&logger, "Incoming message event (socket %d)...\n", event->socket_fd);
                                int accept_read_result = accepted_socket_ready_to_read(context, connection_arena, &logger, event->socket_fd);
                                if (accept_read_result < 0)
                                {
                                    LOG(&logger, "Could not read from the socket (errno: %d - \"%s\")\n", errno, strerror(errno));
                                }

                                memory__set(event, 0, sizeof(struct socket_event_data));
                                logger__flush(&logger);
                            }
                        }
                    }
                }
            }
        }

        close(server_socket);
    }

    destroy_async_context(context);
    return 0;
}

memory_block load_file(memory_allocator allocator, char const *filename)
{
    memory_block result = {};

    int fd = open(filename, O_NOFOLLOW | O_RDONLY, 0);
    if (fd < 0)
    {
        return result;
    }

    struct stat st;
    int ec = fstat(fd, &st);
    if (ec < 0)
    {
        return result;
    }

    memory_block block = ALLOCATE_BUFFER(allocator, st.st_size);
    uint32 bytes_read = read(fd, block.memory, st.st_size);

    if (bytes_read < st.st_size)
    {
        DEALLOCATE(allocator, block);
        return result;
    }

    result = block;
    return result;
}

int make_socket_nonblocking(int fd)
{
    int result = 0;

    int server_socket_flags = fcntl(fd, F_GETFL, 0);
    if (server_socket_flags < 0)
    {
        printf("Error fcntl get fd flags\n");
        result = -1;
    }
    else
    {
        int set_flags_result = fcntl(fd, F_SETFL, server_socket_flags|O_NONBLOCK);
        if (set_flags_result < 0)
        {
            printf("Error fcntl set fd flags\n");
            result = -1;
        }
    }

    return result;
}

int accept_connection(struct async_context *context, memory_allocator arena, struct logger *logger, int server_socket)
{
    int result = 0;

    struct sockaddr accepted_address;
    socklen_t accepted_address_size = sizeof(accepted_address);

    int accepted_socket = accept(server_socket, &accepted_address, &accepted_address_size);
    if (accepted_socket < 0)
    {
        LOG(logger, "Error accept (errno: %d - \"%s\")\n", errno, strerror(errno));
        result = -1;
    }
    else
    {
        LOG(logger, "Accepted connection (socket: %d) from %d.%d.%d.%d:%d\n",
            accepted_socket,
            (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr      ) & 0xff,
            (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr >> 8 ) & 0xff,
            (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr >> 16) & 0xff,
            (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr >> 24) & 0xff,
            uint16__change_endianness(((struct sockaddr_in *) &accepted_address)->sin_port));

        int nonblock_accepted_socket_result = make_socket_nonblocking(accepted_socket);
        if (nonblock_accepted_socket_result < 0)
        {
            LOG(logger, "Error make_socket_nonblocking (errno: %d - \"%s\")\n", errno, strerror(errno));
            result = -1;
        }
        else
        {
            memory_block buffer = ALLOCATE_BUFFER(arena, KILOBYTES(16));

            int bytes_received = recv(accepted_socket, buffer.memory, buffer.size - 1, 0);
            if (bytes_received < 0)
            {
                LOG(logger, "recv returned -1, (errno: %d - \"%s\")\n", errno, strerror(errno));

                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    LOG(logger, "errno=EAGAIN or EWOULDBLOCK... adding to the kqueue\n");

                    int register_result = register_socket_to_read(context, accepted_socket, SOCKET_EVENT__INCOMING_MESSAGE);
                    if (register_result < 0)
                    {
                        if (register_result == -2)
                            LOG(logger, "Error coult not add to the kqueue because all %d slots in the array are occupied\n", MAX_EVENTS);
                        if (register_result == -1)
                            LOG(logger, "Error kregister_accepted_socket_result (errno: %d - \"%s\")\n", errno, strerror(errno));

                        result = -1;
                        LOG(logger, "Closing incoming connection...\n");
                        close(accepted_socket);
                    }
                    else
                    {
                        LOG(logger, "Added to async queue\n");
                    }
                }
                else
                {
                    LOG(logger, "Error recv (errno: %d - \"%s\")\n", errno, strerror(errno));
                    result = -1;
                }
            }
            else
            {
                LOG(logger, "Successfully read %d bytes immediately!\n", bytes_received);
                LOG_UNTRUSTED(logger, buffer.memory, bytes_received);

                send_payload(arena, logger, accepted_socket);

                LOG(logger, "Closing incoming connection...\n");
                close(accepted_socket);
            }
        }
    }

    return result;
}

int accepted_socket_ready_to_read(struct async_context *context, memory_allocator arena, struct logger *logger, int accepted_socket)
{
    int result = 0;

    memory_block buffer = ALLOCATE_BUFFER(arena, KILOBYTES(16));

    int bytes_received = recv(accepted_socket, buffer.memory, buffer.size - 1, 0);
    if (bytes_received < 0)
    {
        LOG(logger, "recv returned %d (errno: %d - \"%s\")\n", bytes_received, errno, strerror(errno));

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            LOG(logger, "errno=EAGAIN or EWOULDBLOCK... return to wait???\n");
        }
        else
        {
            result = -1;
        }
    }
    else if (bytes_received == 0)
    {
        LOG(logger, "Read 0 bytes, that means EOF, peer closed the connection (set slot to 0)\n");
    }
    else
    {
        LOG(logger, "Successfully read %d bytes after the event!\n", bytes_received);
        LOG_UNTRUSTED(logger, buffer.memory, bytes_received);

        send_payload(arena, logger, accepted_socket);
    }

    LOG(logger, "Closing incoming connection...\n");
    close(accepted_socket);

    return result;
}

int send_payload(memory_allocator arena, struct logger *logger, int accepted_socket)
{
    int result = 0;
    string_builder sb = {
        .memory = ALLOCATE_BUFFER(arena, KILOBYTES(512)),
        .used = 0
    };
    memory_block payload = make_http_response(arena, &sb);

    int bytes_sent = send(accepted_socket, payload.memory, payload.size, 0);
    if (bytes_sent < 0)
    {
        LOG(logger, "Could not send anything back (errno: %d - \"%s\")\n", errno, strerror(errno));
        result = -1;
    }
    else
    {
        LOG(logger, "Sent back %d bytes of http\n", bytes_sent);
    }

    return result;
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
        printf("Error write logger file (errno: %d - \"%s\")\n", errno, strerror(errno));
    }
    string_builder__reset(&logger->sb);
}

memory_block make_http_response(memory_allocator allocator, string_builder *sb)
{
    memory_block file = load_file(allocator, "../www/index.html");
    if (file.memory != NULL)
    {
        string_builder__append_format(sb, payload_template, file.size);
        string_builder__append_buffer(sb, file);
    }

    return string_builder__get_string(sb);
}


#include <memory_allocator.c>
#include <string_builder.c>

#if OS_MAC || OS_FREEBSD
#include "async_queue_kqueue.c"
#elif OS_LINUX
#include "async_queue_epoll.c"
#endif



