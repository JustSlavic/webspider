#include <base.h>
#include <integer.h>
#include <memory.h>
#include <string_view.hpp>
#include <string_builder.hpp>
#include <memory_allocator.hpp>
#include <util.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>

#define IP4(x, y, z, w) ((((uint8) w) << 24) | (((uint8) z) << 16) | (((uint8) y) << 8) | ((uint8) x))
#define IP4_LOCALHOST IP4(127, 0, 0, 1)


bool32 cstrings__equal(char const *s1, char const *s2)
{
    while (true)
    {
        if (*s1++ != *s2++) return false;
        if (*s1 == 0 || *s2 == 0) break;
    }
    return true;
}


struct cli_arguments
{
    char const *filename;
    char const *hostname;
};


int main(int argc, char **argv)
{
    cli_arguments args = {};

    int arg_index = 1;
    while (arg_index < argc)
    {
        if (cstrings__equal("-f", argv[arg_index]))
        {
            if ((arg_index + 1) < argc)
            {
                args.filename = argv[arg_index + 1];
                arg_index += 2;
            }
            else
            {
                printf("Error: should specify FILENAME after key '-f'\n");
                arg_index += 1;
            }
        }
        else if (cstrings__equal("--file", argv[arg_index]))
        {
            if ((arg_index + 1) < argc)
            {
                arg_index += 2;
            }
            else
            {
                printf("Error: should specify FILENAME after key '--file'\n");
                arg_index += 1;
            }
        }
        else
        {
            args.hostname = argv[arg_index];
            arg_index += 1;
        }
    }

    memory_buffer payload = {};
    if (args.filename)
    {
        payload = load_file(mallocator(), args.filename);
    }
    else
    {
        payload = mallocator().allocate_buffer(KILOBYTES(1));

        auto sb = string_builder::from(payload);
        sb.append("GET / HTTP/1.1\n"
                  "User-Agent: kurl/0.0.0\n"
                  "Accept: */*\n");
    }

    struct sockaddr_in address = {};
    if (args.hostname)
    {
        struct addrinfo hints = {};
        hints.ai_family = AF_INET;       // don't care IPv4 or IPv6
        hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

        struct addrinfo *servinfo;  // will point to the results
        int getaddrinfo_result = getaddrinfo(args.hostname, "80", &hints, &servinfo);
        UNUSED(getaddrinfo_result);

        struct addrinfo *p;
        for(p = servinfo; p != NULL; p = p->ai_next)
        {
            if (p->ai_family == AF_INET)
            {
                memory__copy(&address, p->ai_addr, sizeof(struct sockaddr_in));
                break;
            }
        }

        freeaddrinfo(servinfo);
    }
    else
    {
        printf("Error: hostname is not specified\n");
        return EXIT_FAILURE;
    }

    int webspider_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (webspider_fd < 0)
    {
        printf("errno: %d\n", errno);
    }
    else
    {
        int connect_result = connect(webspider_fd, (struct sockaddr const *) &address, sizeof(address));
        if (connect_result < 0)
        {
            printf("Could not connect to the webspider (errno: %d - \"%s\")\n", errno, strerror(errno));
        }
        else
        {
            int bytes_sent = send(webspider_fd, payload.data, payload.size - 1, 0);
            if (bytes_sent < 0)
            {
                printf("Could not send payload (errno: %d - \"%s\")\n", errno, strerror(errno));
            }
            else
            {
                char buffer[KILOBYTES(16)] = {};
                int bytes_read = recv(webspider_fd, buffer, KILOBYTES(16), 0);
                printf("%.*s\n", bytes_read, buffer);
            }
            close(connect_result);
        }
    }

    return 0;
}


#include <memory_allocator.cpp>
#include <string_builder.cpp>
#include <util.cpp>
#include <memory_bucket.cpp>
