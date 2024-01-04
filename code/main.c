#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <base.h>
#include <integer.h>
#include <memory.h>
#include <memory_allocator.h>
#include <string_builder.h>
#include <string.h>
#include <errno.h>
#include <time.h>


const char payload_template[] =
"HTTP/1.0 200 OK\n"
"Content-Length: %lld\n"
"Content-Type: text/html\n"
"\n";

const char payload_500_template[] =
"HTTP/1.0 500 Internal Server Error\n"
"Content-Length: 252\n"
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

#define LOG_FILE_MAX_SIZE MEGABYTES(1)

#define IP4_ANY 0
#define IP4(x, y, z, w) ((((uint8) w) << 24) | (((uint8) z) << 16) | (((uint8) y) << 8) | ((uint8) x))
#define IP4_LOCALHOST IP4(127, 0, 0, 1)


const int32 backlog_size = 32;


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


struct logger
{
    char const *filename;
    string_builder sb;
};

#define LOG(FORMAT, ...) \
    printf(FORMAT VA_ARGS(__VA_ARGS__))
// do { \
//     time_t t = time(NULL); \
//     struct tm tm = *localtime(&t); \
//     string_builder__append_format(&logger.sb, "[%d-%02d-%02d %02d:%02d:%02d] ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec); \
//     string_builder__append_format(&logger.sb, FORMAT VA_ARGS(__VA_ARGS__)); \
//     if (logger.sb.used > (logger.sb.memory.size - KILOBYTES(1))) \
//         logger__flush(&logger); \
// } while (0)


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
    string_builder__reset(&logger->sb);
}


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

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket > -1)
    {
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_port   = uint16__change_endianness(80);
        address.sin_addr.s_addr = IP4_ANY;
        int bind_result = bind(server_socket, (const struct sockaddr *) &address, sizeof(address));
        if (bind_result > -1)
        {
            int listen_result = listen(server_socket, backlog_size);
            if (listen_result > -1)
            {
                while(true)
                {
                    memory_arena__reset(connection_arena);

                    struct sockaddr accepted_address;
                    socklen_t accepted_address_size = sizeof(accepted_address);
                    int accepted_socket = accept(server_socket, &accepted_address, &accepted_address_size);
                    if (accepted_socket > -1)
                    {
                        memory_block buffer = ALLOCATE_BUFFER(connection_arena, KILOBYTES(32));

                        int bytes_received = recv(accepted_socket, buffer.memory, buffer.size - 1, 0);
                        if (bytes_received > -1)
                        {
                            LOG("Received %d bytes from %d.%d.%d.%d:%d\n", bytes_received,
                                (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr      ) & 0xff,
                                (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr >> 8 ) & 0xff,
                                (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr >> 16) & 0xff,
                                (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr >> 24) & 0xff,
                                uint16__change_endianness(((struct sockaddr_in *) &accepted_address)->sin_port));
                            if (bytes_received > 0)
                            {
                                LOG("\n%.*s\n", (int) buffer.size, buffer.memory);
                            }

                            string_builder sb =
                            {
                                .memory = ALLOCATE_BUFFER(connection_arena, KILOBYTES(512)),
                                .used = 0
                            };

                            memory_block payload = make_http_response(connection_arena, &sb);

                            if (payload.memory == NULL)
                            {
                                payload.memory = (byte *) payload_500_template;
                                payload.size = ARRAY_COUNT(payload_500_template);

                                LOG("Error 500 Internal Server Error: Could not make payload, returning 500 instead!\n");
                            }

                            int bytes_sent = send(accepted_socket, payload.memory, payload.size, 0);
                            LOG("Sent %d bytes\n", bytes_sent);
                        }
                        else
                        {
                            LOG("Could not receive any bytes from connection socket %d (errno: %d)\n", accepted_socket, errno);
                        }

                        close(accepted_socket);
                        logger__flush(&logger);
                    }
                    else
                    {
                        LOG("Could not accept connection on socket %d (errno: %d)\n", server_socket, errno);
                    }
                }
            }
            else
            {
                LOG("Could not listen socket %d (errno: %d)\n", server_socket, errno);
            }
        }
        else
        {
            LOG("Could not bind socket %d to an address %d.%d.%d.%d:%d (errno: %d)\n",
                server_socket,
                (address.sin_addr.s_addr      ) & 0xff,
                (address.sin_addr.s_addr >> 8 ) & 0xff,
                (address.sin_addr.s_addr >> 16) & 0xff,
                (address.sin_addr.s_addr >> 24) & 0xff,
                int16__change_endianness(address.sin_port),
                errno
                );
        }

        close(server_socket);
    }
    else
    {
        LOG("Could not create socket (errno: %d)\n", errno);
        // @todo: diagnostics (read errno)
    }

    logger__flush(&logger);

    return 0;
}

#include <memory_allocator.c>
#include <string_builder.c>

