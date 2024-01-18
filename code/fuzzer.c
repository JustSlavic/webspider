#include <base.h>
#include <integer.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define IP4(x, y, z, w) ((((uint8) w) << 24) | (((uint8) z) << 16) | (((uint8) y) << 8) | ((uint8) x))
#define IP4_LOCALHOST IP4(127, 0, 0, 1)


int main()
{
    int webspider_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (webspider_fd < 0)
    {
        printf("errno: %d\n", errno);
    }
    else
    {
        struct sockaddr_in address = {
            .sin_family      = AF_INET,
            .sin_port        = uint16__change_endianness(80),
            .sin_addr.s_addr = 0,
        };

        int connect_result = connect(webspider_fd, (struct sockaddr const *) &address, sizeof(address));
        if (connect_result < 0)
        {
            printf("Could not connect to the webspider (errno: %d - \"%s\")\n", errno, strerror(errno));
        }
        else
        {
            int bytes_sent = send(webspider_fd, "", 0, 0); // @note minus null terminator
            if (bytes_sent < 0)
            {
                printf("Could not send bytes across socket (errno: %d - \"%s\")\n", errno, strerror(errno));
            }
            else
            {
                printf("Sent 0 bytes\n");
                char buffer[1024*4];
                int bytes_read = recv(webspider_fd, buffer, sizeof(buffer), 0);
                if (bytes_read < 0)
                {
                    printf("Could not receive bytes from socket (errno: %d - \"%s\")\n", errno, strerror(errno));
                }
                else
                {
                    printf("%.*s\n", bytes_read, buffer);
                }
            }
        }
    }

    return 0;
}