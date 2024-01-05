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

#define IP4_ANY 0
#define IP4(x, y, z, w) ((((uint8) w) << 24) | (((uint8) z) << 16) | (((uint8) y) << 8) | ((uint8) x))
#define IP4_LOCALHOST IP4(127, 0, 0, 1)


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

int send_payload(int accepted_socket)
{
    int result = 0;

    int bytes_sent = send(accepted_socket, payload_500, sizeof(payload_500), 0);
    if (bytes_sent < 0)
    {
        printf("Could not send anything back (errno: %d - \"%s\")\n", errno, strerror(errno));
        result = -1;
    }
    else
    {
        printf("Sent back %d bytes of http\n", bytes_sent);
    }

    return result;
}

int accept_connection(struct async_context *context, int server_socket)
{
    int result = 0;

    struct sockaddr accepted_address;
    socklen_t accepted_address_size = sizeof(accepted_address);

    int accepted_socket = accept(server_socket, &accepted_address, &accepted_address_size);
    if (accepted_socket < 0)
    {
        printf("Error accept (errno: %d - \"%s\")\n", errno, strerror(errno));
        result = -1;
    }
    else
    {
        printf("Accepted connection (socket: %d) from %d.%d.%d.%d:%d\n",
            accepted_socket,
            (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr      ) & 0xff,
            (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr >> 8 ) & 0xff,
            (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr >> 16) & 0xff,
            (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr >> 24) & 0xff,
            uint16__change_endianness(((struct sockaddr_in *) &accepted_address)->sin_port));

        int nonblock_accepted_socket_result = make_socket_nonblocking(accepted_socket);
        if (nonblock_accepted_socket_result < 0)
        {
            printf("Error make_socket_nonblocking (errno: %d - \"%s\")\n", errno, strerror(errno));
            result = -1;
        }
        else
        {
            char buffer[1024];
            memory__set(buffer, 0, sizeof(buffer));

            int bytes_received = recv(accepted_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received < 0)
            {
                printf("recv returned -1, (errno: %d - \"%s\")\n", errno, strerror(errno));

                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    printf("errno=EAGAIN or EWOULDBLOCK... adding to the kqueue\n");

                    int register_result = register_socket_to_read(context, accepted_socket, SOCKET_EVENT__INCOMING_MESSAGE);
                    if (register_result < 0)
                    {
                        if (register_result == -2)
                            printf("Error coult not add to the kqueue because all %lu slots in the array are occupied\n", ARRAY_COUNT(context->registered_events));
                        if (register_result == -1)
                            printf("Error kregister_accepted_socket_result (errno: %d - \"%s\")\n", errno, strerror(errno));

                        result = -1;
                        printf("Closing incoming connection...\n");
                        close(accepted_socket);
                    }
                    else
                    {
                        printf("Added to kqueue\n");
                    }
                }
                else
                {
                    printf("Error recv (errno: %d - \"%s\")\n", errno, strerror(errno));
                    result = -1;
                }
            }
            else
            {
                printf("Successfully read %d bytes immediately!\n", bytes_received);
                printf("\n%.*s\n", bytes_received, buffer);

                send_payload(accepted_socket);

                printf("Closing incoming connection...\n");
                close(accepted_socket);
            }
        }
    }

    return result;
}


int accepted_socket_ready_to_read(struct async_context *context, int accepted_socket)
{
    int result = 0;

    char buffer[1024];
    memory__set(buffer, 0, sizeof(buffer));

    bool again = false;

    int bytes_received = recv(accepted_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0)
    {
        printf("recv returned %d (errno: %d - \"%s\")\n", bytes_received, errno, strerror(errno));

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            printf("errno=EAGAIN or EWOULDBLOCK... return to wait???\n");
            again = true;
        }
        else
        {
            result = -1;
        }
    }
    else if (bytes_received == 0)
    {
        printf("Read 0 bytes, that means EOF, peer closed the connection (set slot to 0)\n");
    }
    else
    {
        printf("Successfully read %d bytes after the event!\n", bytes_received);
        printf("\n%.*s\n", bytes_received, buffer);
        printf("Not gonna read anymore anyway\n");

        send_payload(accepted_socket);
    }

    if (again)
    {
        printf("Returning without closing connection???\n");
    }
    else
    {
        printf("Closing incoming connection...\n");
        close(accepted_socket);
    }

    return result;
}


int main()
{
    int accepted_socket_list[BACKLOG_SIZE];
    memory__set(accepted_socket_list, 0, sizeof(accepted_socket_list));

    struct async_context context;
    int create_async_context_result = create_async_context(&context);
    if (create_async_context_result < 0)
    {
        printf("Error async queue\n");
        return EXIT_FAILURE;
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        printf("Error socket (errno: %d - \"%s\")\n", errno, strerror(errno));
        return EXIT_FAILURE;
    }
    else
    {
        int nonblock_result = make_socket_nonblocking(server_socket);
        if (nonblock_result < 0)
        {
            printf("Error: make_socket_nonblocking\n");
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
                printf("Error bind (errno: %d - \"%s\")\n", errno, strerror(errno));
            }
            else
            {
                int listen_result = listen(server_socket, BACKLOG_SIZE);
                if (listen_result < 0)
                {
                    printf("Error listen (errno: %d - \"%s\")\n", errno, strerror(errno));
                }
                else
                {
                    int register_result = register_socket_to_read(&context, server_socket, SOCKET_EVENT__INCOMING_CONNECTION);
                    if (register_result < 0)
                    {
                        if (register_result == -2)
                            printf("Error coult not add to the kqueue because all %lu slots in the array are occupied\n", ARRAY_COUNT(context.registered_events));
                        if (register_result == -1)
                            printf("Error kregister_accepted_socket_result (errno: %d - \"%s\")\n", errno, strerror(errno));
                    }
                    else
                    {
                        while(true)
                        {
                            struct socket_event_data *event = wait_for_new_events(&context);
                            if (event->type == SOCKET_EVENT__INCOMING_CONNECTION)
                            {
                                printf("Incoming connection event...\n");
                                int accept_connection_result = accept_connection(&context, server_socket);
                                if (accept_connection_result < 0)
                                {
                                    printf("Could not accept connection (errno: %d - \"%s\")\n", errno, strerror(errno));
                                }
                            }
                            else if (event->type == SOCKET_EVENT__INCOMING_MESSAGE)
                            {
                                printf("Incoming message event (socket %d)...\n", event->socket_fd);
                                int accept_read_result = accepted_socket_ready_to_read(&context, event->socket_fd);
                                if (accept_read_result < 0)
                                {
                                    printf("Could not read from the socket (errno: %d - \"%s\")\n", errno, strerror(errno));
                                }
                            }
                        }
                    }
                }
            }
        }

        close(server_socket);
    }

    close(context.queue_fd);
    return 0;
}


#if OS_MAC || OS_FREEBSD
#include "async_queue_kqueue.c"
#elif OS_LINUX
#endif



