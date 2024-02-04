#include <base.h>
#include <integer.h>
#include <string_view.hpp>
#include <memory_allocator.h>

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
};


memory_block load_file(memory_allocator allocator, char const *filename);


int main(int argc, char **argv)
{
    cli_arguments args = {};

    int arg_index = 1;
    while (arg_index < argc)
    {
        printf("arg[%d] = \"%s\"\n", arg_index, argv[arg_index]);

        if (cstrings__equal("-f", argv[arg_index]))
        {
            if ((arg_index + 1) < argc)
            {
                printf("Arg: -f %s\n", argv[arg_index + 1]);

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
                printf("Arg: --file %s\n", argv[arg_index + 1]);
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
            printf("Warning: do not recognize argument passed: '%s'\n", argv[arg_index]);
            arg_index += 1;
        }
    }

    memory_block payload = {};
    if (args.filename)
    {
        payload = load_file(mallocator(), args.filename);
    }

    int webspider_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (webspider_fd < 0)
    {
        printf("errno: %d\n", errno);
    }
    else
    {
        struct sockaddr_in address;
        address.sin_family      = AF_INET;
        address.sin_port        = uint16__change_endianness(80);
        address.sin_addr.s_addr = 0;

        int connect_result = connect(webspider_fd, (struct sockaddr const *) &address, sizeof(address));
        if (connect_result < 0)
        {
            printf("Could not connect to the webspider (errno: %d - \"%s\")\n", errno, strerror(errno));
        }
        else
        {
            int bytes_sent = send(webspider_fd, payload.memory, payload.size - 1, 0);
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

memory_block load_file(memory_allocator allocator, char const *filename)
{
    memory_block result = {};

    int fd = open(filename, O_RDONLY, 0);
    if (fd < 0)
    {}
    else
    {
        struct stat st;
        int ec = fstat(fd, &st);
        if (ec < 0)
        {}
        else
        {
            memory_block block = ALLOCATE_BUFFER(allocator, st.st_size + 1);
            if (block.memory != NULL)
            {
                uint32 bytes_read = read(fd, block.memory, st.st_size);
                if (bytes_read < st.st_size)
                {
                    DEALLOCATE_BLOCK(allocator, block);
                }
                else
                {
                    result = block;
                }
            }
        }
        close(fd);
    }

    return result;
}


#include <memory_allocator.c>

