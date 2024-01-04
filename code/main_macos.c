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

#include <sys/event.h>


const char payload_500[] =
"HTTP/1.0 500 Internal Server Error\n"
"Content-Length: 247\n"
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


#define MAX_EVENTS 10

#define IP4_ANY 0
#define IP4(x, y, z, w) ((((uint8) w) << 24) | (((uint8) z) << 16) | (((uint8) y) << 8) | ((uint8) x))
#define IP4_LOCALHOST IP4(127, 0, 0, 1)

const int32 backlog_size = 32;


int make_socket_nonblocking(int fd)
{
    int server_socket_flags = fcntl(fd, F_GETFL, 0);
    if (server_socket_flags < 0)
    {
        printf("Error fcntl get fd flags\n");
        return -1;
    }
    else
    {
        int set_flags_result = fcntl(fd, F_SETFL, server_socket_flags|O_NONBLOCK);
        if (set_flags_result < 0)
        {
            printf("Error fcntl set fd flags\n");
            return -1;
        }
    }

    return 0;
}


int main()
{
    int accepted_socket_list[64];
    memory__set(accepted_socket_list, 0, sizeof(accepted_socket_list));

    int kqueue_fd = kqueue();
    if (kqueue_fd < 0)
    {
        printf("Error kqueue\n");
        return EXIT_FAILURE;
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        printf("Error socket\n");
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
                printf("Error bind\n");
            }
            else
            {
                int listen_result = listen(server_socket, backlog_size);
                if (listen_result < 0)
                {
                    printf("Error listen\n");
                }
                else
                {
                    // Register socket in the KQueue
                    struct kevent register_server_socket;
                    EV_SET(&register_server_socket, server_socket,
                        /* filter */ EVFILT_READ,
                        /* flags */ EV_ADD | EV_CLEAR,
                        /* fflags */ 0,
                        /* data */ 0,
                        /* udata */ NULL);

                    int kregister_server_socket_result = kevent(kqueue_fd,
                                                                &register_server_socket, 1,
                                                                NULL, 0,
                                                                NULL);
                    if (kregister_server_socket_result < 0)
                    {
                        printf("Error kevent register server_socket\n");
                    }
                    else
                    {
                        // KQueue receive notification
                        while(true)
                        {
                            struct kevent incoming_event;
                            int events_count = kevent(kqueue_fd,
                                                      NULL, 0,
                                                      &incoming_event, 1,
                                                      NULL);
                            if (events_count < 0)
                            {
                                printf("Error kevent incoming_event (errno: %d - \"%s\")\n", errno, strerror(errno));
                            }
                            else if (events_count > 0 && (incoming_event.flags & EV_ERROR))
                            {
                                printf("Error kevent incoming_event returned error event (errno: %d - \"%s\")\n", errno, strerror(errno));
                            }
                            else if (events_count > 0 && incoming_event.udata == NULL)
                            {
                                printf("Incoming event with udata==0\n");

                                struct sockaddr accepted_address;
                                socklen_t accepted_address_size = sizeof(accepted_address);

                                int accepted_socket = accept(server_socket, &accepted_address, &accepted_address_size);
                                if (accepted_socket < 0)
                                {
                                    printf("Error accept (errno: %d - \"%s\")\n", errno, strerror(errno));
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

                                                bool added_to_kqueue = false;
                                                for (int i = 0; i < ARRAY_COUNT(accepted_socket_list); i++)
                                                {
                                                    if (accepted_socket_list[i] == 0)
                                                    {
                                                        printf("Found slot in the array: slot=%d\n", i);

                                                        accepted_socket_list[i] = accepted_socket;
                                                        int *slot_pointer = accepted_socket_list + i;

                                                        struct kevent register_accepted_socket;
                                                        EV_SET(&register_accepted_socket, accepted_socket,
                                                            /* filter */ EVFILT_READ,
                                                            /* flags */ EV_ADD | EV_CLEAR,
                                                            /* fflags */ 0,
                                                            /* data */ 0,
                                                            /* udata */ slot_pointer);
                                                        int kregister_accepted_socket_result = kevent(kqueue_fd,
                                                                                                      &register_accepted_socket, 1,
                                                                                                      NULL, 0,
                                                                                                      NULL);
                                                        if (kregister_accepted_socket_result < 0)
                                                        {
                                                            printf("Error kregister_accepted_socket_result (errno: %d - \"%s\")\n", errno, strerror(errno));
                                                        }
                                                        else
                                                        {
                                                            added_to_kqueue = true;
                                                            break;
                                                        }
                                                    }
                                                }

                                                if (added_to_kqueue)
                                                {
                                                    printf("Added to kqueue\n");
                                                }
                                                else
                                                {
                                                    printf("Error coult not add to the kqueue because all %lu slots in the array are occupied\n", ARRAY_COUNT(accepted_socket_list));

                                                    printf("Closing incoming connection...\n");
                                                    close(accepted_socket);
                                                }
                                            }
                                            else
                                            {
                                                printf("Error recv (errno: %d - \"%s\")\n", errno, strerror(errno));
                                            }
                                        }
                                        else
                                        {
                                            printf("Successfully read %d bytes immediately!\n", bytes_received);
                                            printf("\n%.*s\n", bytes_received, buffer);

                                            int bytes_sent = send(accepted_socket, payload_500, sizeof(payload_500), 0);
                                            if (bytes_sent < 0)
                                            {
                                                printf("Could not send anything back\n");
                                            }
                                            else
                                            {
                                                printf("Sent back %d bytes of http\n", bytes_sent);
                                            }

                                            printf("Closing incoming connection...\n");
                                            close(accepted_socket);
                                        }
                                    }
                                }
                            }
                            else if (events_count > 0 && incoming_event.udata != NULL)
                            {
                                printf("Incoming event with udata==%p (%d)\n", incoming_event.udata, *(int *)incoming_event.udata);
                                int *accepted_socket = (int *) incoming_event.udata;

                                char buffer[1024];
                                memory__set(buffer, 0, sizeof(buffer));

                                bool again = false;

                                int bytes_received = recv(*accepted_socket, buffer, sizeof(buffer) - 1, 0);
                                if (bytes_received < 0)
                                {
                                    printf("recv returned %d (errno: %d - \"%s\")\n", bytes_received, errno, strerror(errno));

                                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                                    {
                                        printf("errno=EAGAIN or EWOULDBLOCK... return to wait???\n");
                                        again = true;
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

                                    int bytes_sent = send(*accepted_socket, payload_500, sizeof(payload_500), 0);
                                    if (bytes_sent < 0)
                                    {
                                        printf("Could not send anything back\n");
                                    }
                                    else
                                    {
                                        printf("Sent back %d bytes of http\n", bytes_sent);
                                    }
                                }

                                if (again)
                                {
                                    printf("Returning without closing connection???\n");
                                }
                                else
                                {
                                    printf("Closing incoming connection...\n");
                                    close(*accepted_socket);
                                    *accepted_socket = 0;
                                }
                            }
                        }
                    }
                }
            }
        }

        close(server_socket);
    }

    close(kqueue_fd);
    return 0;
}
