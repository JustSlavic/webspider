#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


#define WEBSPIDER_SOCKET_NAME "/tmp/webspider_unix_socket"


int main()
{
    int webspider_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (webspider_fd < 0)
    {
        printf("errno: %d\n", errno);
    }
    else
    {
        struct sockaddr_un name = {
            .sun_family = AF_UNIX,
            .sun_path = WEBSPIDER_SOCKET_NAME,
        };

        int connect_result = connect(webspider_fd, (struct sockaddr const *) &name, sizeof(name));
        if (connect_result < 0)
        {
            printf("Could not connect to the webspider (errno: %d - \"%s\")\n", errno, strerror(errno));
        }
        else
        {
            char msg[] = "report";
            int bytes_sent = send(webspider_fd, msg, sizeof(msg) - 1, 0); // @note minus null terminator
            if (bytes_sent < 0)
            {
                printf("Could not send bytes across UNIX Domain Socket (errno: %d - \"%s\")\n", errno, strerror(errno));
            }
            else
            {
                char buffer[1024*4];
                int bytes_read = recv(webspider_fd, buffer, sizeof(buffer), 0);
                if (bytes_read < 0)
                {
                    printf("Could not receive bytes from UNIX Domain Socket (errno: %d - \"%s\")\n", errno, strerror(errno));
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
