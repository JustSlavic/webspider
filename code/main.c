#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <base.h>
#include <integer.h>
#include <string.h>


const char payload[] =
"HTTP/1.0 200 OK\n"
"Content-Length: 29\n"
"Content-Type: text/html\n"
"\n"
"<h1>Slava samiy krutoi</h1>\n"
"\n";


#define IP4_ANY 0
#define IP4(x, y, z, w) ((((uint8) w) << 24) | (((uint8) z) << 16) | (((uint8) y) << 8) | ((uint8) x))


const int32 backlog_size = 32;


int main()
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket > -1)
    {
        printf("Socket created! %d\n", server_socket);

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_port   = int16__change_endianness(8080);
        address.sin_addr.s_addr = IP4_ANY;
        int bind_result = bind(server_socket, (const struct sockaddr *) &address, sizeof(address));
        if (bind_result > -1)
        {
            printf("Bind correct!\n");

            int listen_result = listen(server_socket, backlog_size);
            if (listen_result > -1)
            {
                printf("Listen successful!\n");

                while(true)
                {
                    struct sockaddr accepted_address;
                    socklen_t accepted_address_size = sizeof(accepted_address);
                    int accepted_socket = accept(server_socket, &accepted_address, &accepted_address_size);
                    if (accepted_socket > -1)
                    {
                        printf("Connection accepted!\n");

                        char buffer[1024];
                        memset(buffer, 0, sizeof(buffer));

                        int bytes_received = recv(accepted_socket, buffer, sizeof(buffer), 0);
                        if (bytes_received > -1)
                        {
                            printf("Received %d bytes!\n", bytes_received);
                            printf("Message: %.*s\n", bytes_received, buffer);

                            int bytes_sent = send(accepted_socket, payload, strlen(payload), 0);
                            if (bytes_sent > -1)
                            {
                                printf("Message: %.*s\n", (int) strlen(payload), payload);
                                printf("Payload sent! (%d/%d bytes)\n", bytes_sent, (int)strlen(payload));

                                close(accepted_socket);
                            }
                        }
                    }
                }
            }
        }

        close(server_socket);
    }

    return 0;
}
