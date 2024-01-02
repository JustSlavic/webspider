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
#include <string.h>


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

#define IP4_ANY 0
#define IP4(x, y, z, w) ((((uint8) w) << 24) | (((uint8) z) << 16) | (((uint8) y) << 8) | ((uint8) x))


const int32 backlog_size = 32;


memory_block load_file(memory_allocator allocator, char const *filename)
{
    memory_block result = {};

    int fd = open(filename, O_NOFOLLOW, O_RDONLY);
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

memory_block make_http_response(memory_allocator allocator)
{
    memory_block payload = memory__empty_block();

    memory_block file = load_file(allocator, "../www/index.html");
    if (file.memory != NULL)
    {
        payload = ALLOCATE_BUFFER(allocator, file.size + 512);
        usize payload_size = 0;
        {
            byte *cursor = payload.memory;
            payload_size += sprintf((char *) cursor, payload_template, file.size);
            memcpy(cursor + payload_size, file.memory, file.size);
            payload_size += file.size;
        }
        payload.size = payload_size;
    }

    return payload;
}

int main()
{
    usize memory_size = MEGABYTES(1);
    void *memory = malloc(memory_size);
    memset(memory, 0, memory_size);

    memory_block global_memory = { .memory = memory, .size = memory_size };
    memory_allocator global_arena = make_memory_arena(global_memory);

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket > -1)
    {
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_port   = int16__change_endianness(80);
        address.sin_addr.s_addr = IP4_ANY;
        int bind_result = bind(server_socket, (const struct sockaddr *) &address, sizeof(address));
        if (bind_result > -1)
        {
            int listen_result = listen(server_socket, backlog_size);
            if (listen_result > -1)
            {
                while(true)
                {
                    struct sockaddr accepted_address;
                    socklen_t accepted_address_size = sizeof(accepted_address);
                    int accepted_socket = accept(server_socket, &accepted_address, &accepted_address_size);
                    if (accepted_socket > -1)
                    {
                        char buffer[1024];
                        memset(buffer, 0, sizeof(buffer));

                        int bytes_received = recv(accepted_socket, buffer, sizeof(buffer), 0);
                        if (bytes_received > -1)
                        {
                            printf("Message:\n%.*s\n", bytes_received, buffer);

                            memory_block payload = make_http_response(global_arena);

                            if (payload.memory == NULL)
                            {
                                printf("COULD NOT LOAD PAYLOAD!!!\n\n");
                                payload.memory = (byte *) payload_500_template;
                                payload.size = ARRAY_COUNT(payload_500_template);
                            }

                            int bytes_sent = send(accepted_socket, payload.memory, payload.size, 0);
                            printf("%d bytes sent:\n%.*s\n", bytes_sent, (int) payload.size, (char *) payload.memory);
                            close(accepted_socket);
                        }
                        else
                        {
                            printf("Could not receive any bytes from connection socket %d", accepted_socket);
                        }
                    }
                    else
                    {
                        printf("Could not accept connection on socket %d", server_socket);
                    }

                    memory_arena__reset(global_arena);
                }
            }
            else
            {
                printf("Could not listen socket %d", server_socket);
            }
        }
        else
        {
            printf("Could not bind socket %d to an address %d.%d.%d.%d:%d\n",
                server_socket,
                (address.sin_addr.s_addr      ) & 0xff,
                (address.sin_addr.s_addr >> 8 ) & 0xff,
                (address.sin_addr.s_addr >> 16) & 0xff,
                (address.sin_addr.s_addr >> 24) & 0xff,
                int16__change_endianness(address.sin_port)
                );
            // @todo: diagnostics (read errno)
        }

        close(server_socket);
    }
    else
    {
        printf("Could not create socket\n");
        // @todo: diagnostics (read errno)
    }

    return 0;
}

#include <memory_allocator.c>

