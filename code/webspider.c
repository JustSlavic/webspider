#define _POSIX_C_SOURCE 200809L

// Based
#include <base.h>
#include <integer.h>
#include <memory.h>
#include <memory_allocator.h>
#include <string_builder.h>
#include <float32.h>
#include <logger.h>

// *nix
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

// Stdlib
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

// Project-specific
#include "webspider.h"
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

bool is_symbol_ok(char c)
{
    return (('0' <= c) && (c <= '9')) ||
           (('a' <= c) && (c <= 'z')) ||
           (('A' <= c) && (c <= 'Z')) ||
            (c == '.') || (c == ',')  ||
            (c == ':') || (c == ';')  ||
            (c == '!') || (c == '?')  ||
            (c == '@') || (c == '#')  ||
            (c == '$') || (c == '%')  ||
            (c == '^') || (c == '&')  ||
            (c == '*') || (c == '~')  ||
            (c == '(') || (c == ')')  ||
            (c == '<') || (c == '>')  ||
            (c == '[') || (c == ']')  ||
            (c == '{') || (c == '}')  ||
            (c == '-') || (c == '+')  ||
            (c == '/') || (c == '\\') ||
            (c == '"') || (c == '\'') ||
            (c == '`') || (c == '=')  ||
            (c == ' ') || (c == '\n') ||
            (c == '\r')|| (c == '\t');
}

#if DEBUG
#define LOG_UNTRUSTED(BUFFER, SIZE) do { \
    printf("[%s:%d]\n", __FILE__, __LINE__); \
    for (usize i = 0; i < (SIZE); i++) \
    { \
        char c = (BUFFER)[i]; \
        if (is_symbol_ok(c)) \
            printf("%c", c); \
        else \
            printf("\\0x%x", (int) c); \
    } \
    printf("\n"); \
    } while (0)
#else
#define LOG_UNTRUSTED(BUFFER, SIZE) do { \
    time_t t = time(NULL); \
    struct tm tm = *localtime(&t); \
    string_builder__append_format(&logger->sb, "[%d-%02d-%02d %02d:%02d:%02d]\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec); \
    for (usize i = 0; i < (SIZE); i++) \
    { \
        char c = (BUFFER)[i]; \
        if (is_symbol_ok(c)) \
            string_builder__append_format(&logger->sb, "%c", c); \
        else \
            string_builder__append_format(&logger->sb, "\\0x%x", (int) c); \
    } \
    string_builder__append_format(&logger->sb, "\n"); \
    } while (0)
#endif


#define WAIT_TIMEOUT 100
#define BACKLOG_SIZE 32
#define LOG_FILENAME "log.txt"
#define LOG_FILE_MAX_SIZE MEGABYTES(1)
#define INSPECTOR_SOCKET_NAME "/tmp/webspider_unix_socket"

#define IP4_ANY 0
#define IP4(x, y, z, w) ((((uint8) w) << 24) | (((uint8) z) << 16) | (((uint8) y) << 8) | ((uint8) x))
#define IP4_LOCALHOST IP4(127, 0, 0, 1)


memory_block load_file(memory_allocator allocator, char const *filename);
int make_socket_nonblocking(int fd);
int accept_connection_inet(struct webspider *server, int socket_fd);
int accept_connection_unix(struct webspider *server, int socket_fd);
int accepted_inet_socket_ready_to_read(struct webspider *server, int accepted_socket);
int accepted_unix_socket_ready_to_read(struct webspider *server, int accepted_socket);
int accepted_socket_ready_to_write(struct webspider *server, int accepted_socket);
int send_payload(struct webspider *server, int accepted_socket);
memory_block make_http_response(struct webspider *server, memory_allocator allocator, string_builder *sb);
memory_block prepare_report(struct webspider *server);


GLOBAL volatile bool running;
void signal__SIGINT(int dummy) {
    running = false;
}

int main()
{
    signal(SIGINT, signal__SIGINT);

    usize memory_size = MEGABYTES(10);
    void *memory = malloc(memory_size);
    memory__set(memory, 0, memory_size);
    memory_block global_memory = { .memory = memory, .size = memory_size };


    struct webspider server = {
        .socket_fd = 0,
        .async = NULL,

        .webspider_allocator = make_memory_arena(global_memory),
        .connection_allocator = allocate_memory_arena(server.webspider_allocator, MEGABYTES(1)),
    };

    struct logger logger_ = {
        .sb = {
            .memory = ALLOCATE_BUFFER(server.webspider_allocator, MEGABYTES(1)),
            .used = 0,
        },
    };
    server.logger = &logger_;
    LOGGER(&server);

    server.async = create_async_context();
    if (server.async == NULL)
    {
        LOG("Error async queue\n");
        return EXIT_FAILURE;
    }

    server.socket_for_inspector = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server.socket_for_inspector < 0)
    {
        LOG("Could not create socket for interprocess communication, run server without inspector\n");
    }
    else
    {
        int nonblock_result = make_socket_nonblocking(server.socket_fd);
        if (nonblock_result < 0)
        {
            LOG("Error: make_socket_nonblocking\n");
        }
        else
        {
            struct sockaddr_un name = {
                .sun_family = AF_UNIX,
                .sun_path = INSPECTOR_SOCKET_NAME,
            };

            int bind_result = bind(server.socket_for_inspector, (struct sockaddr const *) &name, sizeof(name));
            if (bind_result < 0)
            {
                if (errno == EADDRINUSE)
                {
                    unlink(INSPECTOR_SOCKET_NAME);
                    bind_result = bind(server.socket_for_inspector, (struct sockaddr const *) &name, sizeof(name));
                }
            }

            if (bind_result < 0)
            {
                LOG("Error bind UNIX Domain Socket (errno: %d - \"%s\")\n", errno, strerror(errno));
            }
            else
            {
                int listen_result = listen(server.socket_for_inspector, BACKLOG_SIZE);
                if (listen_result < 0)
                {
                    LOG("Error listen (errno: %d - \"%s\")\n", errno, strerror(errno));
                }
                else
                {
                    int register_result = queue__register(server.async, server.socket_for_inspector,
                        SOCKET_EVENT__INCOMING_CONNECTION | QUEUE_EVENT__UNIX_SOCKET);
                    if (register_result < 0)
                    {
                        if (register_result == -2)
                            LOG("Error coult not add to the kqueue because all %d slots in the array are occupied\n", MAX_EVENTS);
                        if (register_result == -1)
                            LOG("Error kregister_accepted_socket_result (errno: %d - \"%s\")\n", errno, strerror(errno));
                    }
                }
            }
        }
    }

    server.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server.socket_fd < 0)
    {
        LOG("Error socket (errno: %d - \"%s\")\n", errno, strerror(errno));
        return EXIT_FAILURE;
    }
    else
    {
        int nonblock_result = make_socket_nonblocking(server.socket_fd);
        if (nonblock_result < 0)
        {
            LOG("Error: make_socket_nonblocking\n");
        }
        else
        {
            struct sockaddr_in address = {
                .sin_family      = AF_INET,
                .sin_port        = uint16__change_endianness(80),
                .sin_addr.s_addr = IP4_ANY,
            };

            int bind_result = bind(server.socket_fd, (struct sockaddr const *) &address, sizeof(address));
            if (bind_result < 0)
            {
                if (errno == EADDRINUSE)
                {
                    int reuse = 1;
                    setsockopt(server.socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
                    bind_result = bind(server.socket_fd, (struct sockaddr const *) &address, sizeof(address));
                }
            }

            if (bind_result < 0)
            {
                LOG("Error bind Internet Protocol Socket (errno: %d - \"%s\")\n", errno, strerror(errno));
            }
            else
            {
                int listen_result = listen(server.socket_fd, BACKLOG_SIZE);
                if (listen_result < 0)
                {
                    LOG("Error listen (errno: %d - \"%s\")\n", errno, strerror(errno));
                }
                else
                {
                    int register_result = queue__register(server.async, server.socket_fd,
                        SOCKET_EVENT__INCOMING_CONNECTION | QUEUE_EVENT__INET_SOCKET);
                    if (register_result < 0)
                    {
                        if (register_result == -2)
                            LOG("Error coult not add to the kqueue because all %d slots in the array are occupied\n", MAX_EVENTS);
                        if (register_result == -1)
                            LOG("Error kregister_accepted_socket_result (errno: %d - \"%s\")\n", errno, strerror(errno));
                    }
                    else
                    {
                        running = true;
                        while(running)
                        {
                            queue__waiting_result wait_result = wait_for_new_events(server.async, WAIT_TIMEOUT);
                            if (wait_result.error_code < 0)
                            {
                                if (errno != EINTR)
                                {
                                    LOG("Could not receive events (%d) (errno: %d - \"%s\")\n", wait_result.error_code, errno, strerror(errno));
                                }
                                // @todo: pruning
                            }
                            else if (wait_result.event_count > 0)
                            {
                                queue__event_data *event = wait_result.events;
                                if (queue_event__is(event, QUEUE_EVENT__INET_SOCKET))
                                {
                                    if (queue_event__is(event, SOCKET_EVENT__INCOMING_CONNECTION))
                                    {
                                        LOG("Incoming connection event...\n");
                                        int accept_connection_result = accept_connection_inet(&server, event->socket_fd);
                                        if (accept_connection_result < 0)
                                        {
                                            LOG("Could not accept connection (errno: %d - \"%s\")\n", errno, strerror(errno));
                                        }

                                        memory_arena__reset(server.connection_allocator);
                                    }
                                    else if (queue_event__is(event, SOCKET_EVENT__INCOMING_MESSAGE))
                                    {
                                        LOG("Incoming message event (socket %d)\n", event->socket_fd);
                                        int accept_read_result = accepted_inet_socket_ready_to_read(&server, event->socket_fd);
                                        if (accept_read_result < 0)
                                        {
                                            LOG("Could not read from the socket (errno: %d - \"%s\")\n", errno, strerror(errno));
                                        }

                                        LOG("Closing incoming connection\n");
                                        close(event->socket_fd);

                                        memory__set(event, 0, sizeof(queue__event_data));
                                        logger__flush_filename(logger, LOG_FILENAME, LOG_FILE_MAX_SIZE);
                                        memory_arena__reset(server.connection_allocator);
                                    }
                                    else if (queue_event__is(event, SOCKET_EVENT__OUTGOING_MESSAGE))
                                    {
                                        LOG("Outgoing message event (socket %d)\n", event->socket_fd);
                                        int accept_write_result = accepted_socket_ready_to_write(&server, event->socket_fd);
                                        if (accept_write_result < 0)
                                        {
                                            LOG("Could not write to the socket (errno: %d - \"%s\")\n", errno, strerror(errno));
                                        }

                                        LOG("Closing incoming connection\n");
                                        close(event->socket_fd);

                                        memory__set(event, 0, sizeof(queue__event_data));
                                        logger__flush_filename(logger, LOG_FILENAME, LOG_FILE_MAX_SIZE);
                                        memory_arena__reset(server.connection_allocator);
                                    }
                                }
                                else if (queue_event__is(event, QUEUE_EVENT__UNIX_SOCKET))
                                {
                                    if (queue_event__is(event, SOCKET_EVENT__INCOMING_CONNECTION))
                                    {
                                        LOG("Incoming connection event...\n");
                                        int accept_connection_result = accept_connection_unix(&server, event->socket_fd);
                                        if (accept_connection_result < 0)
                                        {
                                            LOG("Could not accept connection (errno: %d - \"%s\")\n", errno, strerror(errno));
                                        }

                                        memory_arena__reset(server.connection_allocator);
                                    }
                                    else if (queue_event__is(event, SOCKET_EVENT__INCOMING_MESSAGE))
                                    {
                                        LOG("Incoming message event (socket %d)\n", event->socket_fd);
                                        int accept_read_result = accepted_unix_socket_ready_to_read(&server, event->socket_fd);
                                        if (accept_read_result < 0)
                                        {
                                            LOG("Could not read from the socket (errno: %d - \"%s\")\n", errno, strerror(errno));
                                        }

                                        LOG("Closing incoming connection\n");
                                        close(event->socket_fd);

                                        memory__set(event, 0, sizeof(queue__event_data));
                                        logger__flush_filename(logger, LOG_FILENAME, LOG_FILE_MAX_SIZE);
                                        memory_arena__reset(server.connection_allocator);

                                        memory_arena__reset(server.connection_allocator);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        LOG("Closing server socket (%d)\n", server.socket_fd);
        close(server.socket_fd);
    }

    unlink(INSPECTOR_SOCKET_NAME);
    if (server.socket_for_inspector > 0)
    {
        close(server.socket_for_inspector);
    }

    destroy_async_context(server.async);
    logger__flush_filename(logger, LOG_FILENAME, LOG_FILE_MAX_SIZE);

    return 0;
}

memory_block load_file(memory_allocator allocator, char const *filename)
{
    memory_block result = {};

    int fd = open(filename, O_RDONLY, 0);
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
    if (block.memory != NULL)
    {
        uint32 bytes_read = read(fd, block.memory, st.st_size);
        if (bytes_read < st.st_size)
        {
            DEALLOCATE(allocator, block);
        }
        else
        {
            result = block;
        }
    }

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

int accept_connection_inet(struct webspider *server, int socket_fd)
{
    LOGGER(server);

    int result = 0;

    struct sockaddr accepted_address;
    socklen_t accepted_address_size = sizeof(accepted_address);

    int accepted_socket = accept(socket_fd, &accepted_address, &accepted_address_size);
    if (accepted_socket < 0)
    {
        LOG("Error accept (errno: %d - \"%s\")\n", errno, strerror(errno));
        result = -1;
    }
    else
    {
        LOG("Accepted connection (socket: %d) from %d.%d.%d.%d:%d\n",
            accepted_socket,
            (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr      ) & 0xff,
            (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr >> 8 ) & 0xff,
            (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr >> 16) & 0xff,
            (((struct sockaddr_in *) &accepted_address)->sin_addr.s_addr >> 24) & 0xff,
            uint16__change_endianness(((struct sockaddr_in *) &accepted_address)->sin_port));

        int nonblock_accepted_socket_result = make_socket_nonblocking(accepted_socket);
        if (nonblock_accepted_socket_result < 0)
        {
            LOG("Error make_socket_nonblocking (errno: %d - \"%s\")\n", errno, strerror(errno));
            result = -1;
        }
        else
        {
            memory_block buffer = ALLOCATE_BUFFER(server->connection_allocator, KILOBYTES(16));
            if (buffer.memory == NULL)
            {
                LOG("Could not allocate 16Kb to place received message");
                LOG("Closing incoming connection\n");
                close(accepted_socket);
            }
            else
            {
                int bytes_received = recv(accepted_socket, buffer.memory, buffer.size - 1, 0);
                if (bytes_received < 0)
                {
                    LOG("recv returned -1, (errno: %d - \"%s\")\n", errno, strerror(errno));

                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        LOG("Register socket %d to the read messages\n", accepted_socket);
                        int register_result = queue__register(server->async, accepted_socket,
                            SOCKET_EVENT__INCOMING_MESSAGE | QUEUE_EVENT__INET_SOCKET);
                        if (register_result < 0)
                        {
                            if (register_result == -2)
                                LOG("Error coult not add to the kqueue because all %d slots in the array are occupied\n", MAX_EVENTS);
                            if (register_result == -1)
                                LOG("Error kregister_accepted_socket_result (errno: %d - \"%s\")\n", errno, strerror(errno));

                            result = -1;
                            LOG("Closing incoming connection\n");
                            close(accepted_socket);
                        }
                        else
                        {
                            LOG("Added to async queue\n");
                        }
                    }
                    else
                    {
                        LOG("Error recv (errno: %d - \"%s\")\n", errno, strerror(errno));
                        result = -1;
                    }
                }
                else
                {
                    LOG("Successfully read %d bytes immediately!\n", bytes_received);
                    LOG_UNTRUSTED(buffer.memory, bytes_received);

                    send_payload(server, accepted_socket);

                    LOG("Closing incoming connection\n");
                    close(accepted_socket);
                }
            }
        }
    }

    return result;
}

int accept_connection_unix(struct webspider *server, int socket_fd)
{
    LOGGER(server);

    int result = 0;

    struct sockaddr accepted_address;
    socklen_t accepted_address_size = sizeof(accepted_address);

    int accepted_socket = accept(socket_fd, &accepted_address, &accepted_address_size);
    if (accepted_socket < 0)
    {
        LOG("Error accept (errno: %d - \"%s\")\n", errno, strerror(errno));
        result = -1;
    }
    else
    {
        LOG("Accepted connection (socket: %d) from \"%.s\"\n",
            accepted_socket, ((struct sockaddr_un *) &accepted_address)->sun_path);

        int nonblock_accepted_socket_result = make_socket_nonblocking(accepted_socket);
        if (nonblock_accepted_socket_result < 0)
        {
            LOG("Error make_socket_nonblocking (errno: %d - \"%s\")\n", errno, strerror(errno));
            result = -1;
        }
        else
        {
            memory_block buffer = ALLOCATE_BUFFER(server->connection_allocator, KILOBYTES(1));
            if (buffer.memory == NULL)
            {
                LOG("Could not allocate 1Kb to place received message");
                LOG("Closing incoming connection\n");
                close(accepted_socket);
            }
            else
            {
                int bytes_received = recv(accepted_socket, buffer.memory, buffer.size - 1, 0);
                if (bytes_received < 0)
                {
                    LOG("recv returned -1, (errno: %d - \"%s\")\n", errno, strerror(errno));

                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        LOG("Register socket %d to the read messages\n", accepted_socket);
                        int register_result = queue__register(server->async, accepted_socket,
                            SOCKET_EVENT__INCOMING_MESSAGE | QUEUE_EVENT__UNIX_SOCKET);
                        if (register_result < 0)
                        {
                            if (register_result == -2)
                                LOG("Error coult not add to the async queue because all %d slots in the array are occupied\n", MAX_EVENTS);
                            if (register_result == -1)
                                LOG("Error queue__register (errno: %d - \"%s\")\n", errno, strerror(errno));

                            result = -1;
                            LOG("Closing incoming connection\n");
                            close(accepted_socket);
                        }
                        else
                        {
                            LOG("Added to async queue\n");
                        }
                    }
                    else
                    {
                        LOG("Error recv (errno: %d - \"%s\")\n", errno, strerror(errno));
                        result = -1;
                    }
                }
                else
                {
                    LOG("Successfully read %d bytes immediately!\n", bytes_received);
                    LOG_UNTRUSTED(buffer.memory, bytes_received);

                    memory_block report = prepare_report(server);
                    if (report.memory != NULL)
                    {
                        int bytes_sent = send(accepted_socket, report.memory, report.size, 0);
                        if (bytes_sent < 0)
                        {
                            LOG("Error send (errno: %d - \"%s\")\n", errno, strerror(errno));
                            result = -1;
                        }
                    }

                    LOG("Closing incoming connection\n");
                    close(accepted_socket);
                }
            }
        }
    }

    return result;
}

int accepted_inet_socket_ready_to_read(struct webspider *server, int accepted_socket)
{
    LOGGER(server);

    int result = 0;

    memory_block buffer = ALLOCATE_BUFFER(server->connection_allocator, KILOBYTES(16));
    if (buffer.memory == NULL)
    {
        LOG("Could not allocate 16Kb to place received message");
    }
    else
    {
        int bytes_received = recv(accepted_socket, buffer.memory, buffer.size - 1, 0);
        if (bytes_received < 0)
        {
            LOG("recv returned %d (errno: %d - \"%s\")\n", bytes_received, errno, strerror(errno));
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                LOG("errno=EAGAIN or EWOULDBLOCK...\n");
            }
            else
            {
                result = -1;
            }
        }
        else if (bytes_received == 0)
        {
            LOG("Read 0 bytes, that means EOF, peer closed the connection (set slot to 0)\n");
        }
        else
        {
            LOG("Successfully read %d bytes after the event!\n", bytes_received);
            LOG_UNTRUSTED(buffer.memory, bytes_received);

            send_payload(server, accepted_socket);
        }
    }

    return result;
}

int accepted_unix_socket_ready_to_read(struct webspider *server, int accepted_socket)
{
    LOGGER(server);

    int result = 0;

    memory_block buffer = ALLOCATE_BUFFER(server->connection_allocator, KILOBYTES(1));
    if (buffer.memory == NULL)
    {
        LOG("Could not allocate 1Kb to place received message");
    }
    else
    {
        int bytes_received = recv(accepted_socket, buffer.memory, buffer.size - 1, 0);
        if (bytes_received < 0)
        {
            LOG("recv returned %d (errno: %d - \"%s\")\n", bytes_received, errno, strerror(errno));
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                LOG("errno=EAGAIN or EWOULDBLOCK...\n");
            }
            else
            {
                result = -1;
            }
        }
        else if (bytes_received == 0)
        {
            LOG("Read 0 bytes, that means EOF, peer closed the connection (set slot to 0)\n");
        }
        else
        {
            LOG("Successfully read %d bytes after the event!\n", bytes_received);
            LOG_UNTRUSTED(buffer.memory, bytes_received);

            memory_block report = prepare_report(server);
            if (report.memory != NULL)
            {
                int bytes_sent = send(accepted_socket, report.memory, report.size, 0);
                if (bytes_sent < 0)
                {
                    LOG("Error send (errno: %d - \"%s\")\n", errno, strerror(errno));
                    result = -1;
                }
            }
        }
    }

    return result;
}

int accepted_socket_ready_to_write(struct webspider *server, int accepted_socket)
{
    return send_payload(server, accepted_socket);
}

int send_payload(struct webspider *server, int accepted_socket)
{
    LOGGER(server);

    int result = 0;

    memory_block response_buffer = ALLOCATE_BUFFER(server->connection_allocator, KILOBYTES(32));
    if (response_buffer.memory == NULL)
    {
        LOG("Could not allocate 32Kb to place http response");
    }
    else
    {
        string_builder sb = {
            .memory = response_buffer,
            .used = 0
        };
        memory_block payload = make_http_response(server, server->connection_allocator, &sb);

        if (payload.memory == NULL)
        {
            LOG("Failed to make http response in memory\n");
            result = -1;
        }
        else
        {
            int bytes_sent = send(accepted_socket, payload.memory, payload.size, 0);
            if (bytes_sent < 0)
            {
                LOG("Could not send anything back (errno: %d - \"%s\")\n", errno, strerror(errno));
                result = -1;
            }
            else
            {
                LOG("Sent back %d bytes of http\n", bytes_sent);
            }
        }
    }

    return result;
}


memory_block make_http_response(struct webspider *server, memory_allocator allocator, string_builder *sb)
{
    LOGGER(server);

    char const *filename = "../www/index.html";
    memory_block file = load_file(allocator, filename);
    if (file.memory == NULL)
    {
        LOG("Could not load file \"%s\"", filename);
    }
    else
    {
        string_builder__append_format(sb, payload_template, file.size);
        string_builder__append_buffer(sb, file);
    }

    return string_builder__get_string(sb);
}

memory_block prepare_report(struct webspider *server)
{
    char spaces[] = "                                        ";
    char squares[] = "▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮";

    struct memory_allocator__report m_report = memory_allocator__report(server->connection_allocator);
    int n_spaces = truncate_to_int32(40.0f * m_report.used / m_report.size);

    struct async_context__report q_report = async_context__report(server->async);

    string_builder sb = make_string_builder(ALLOCATE_BUFFER(server->connection_allocator, KILOBYTES(1)));
    string_builder__append_format(&sb, "========= MEMORY ALLOCATOR REPORT ========\n");
    string_builder__append_format(&sb, "connection allocator: %llu / %llu bytes used;\n", m_report.used, m_report.size);
    string_builder__append_format(&sb, "+----------------------------------------+\n");
    string_builder__append_format(&sb, "|%.*s|%.*s|\n", n_spaces, squares, 40 - n_spaces - 1, spaces);
    string_builder__append_format(&sb, "+----------------------------------------+\n");
    string_builder__append_format(&sb, "==========================================\n");
    string_builder__append_format(&sb, "ASYNC QUEUE BUFFER:\n");
    for (int i = 0; i < ARRAY_COUNT(q_report.events_in_work); i++)
    {
        queue__event_data *e = q_report.events_in_work + i;
        string_builder__append_format(&sb, "%2d)", i + 1);
        if (e->socket_fd != 0)
        {
            string_builder__append_format(&sb, " [%5d]", e->socket_fd);
        }
        else
        {
            string_builder__append_format(&sb, " [     ]");
        }
        if (e->event_type != 0)
        {
            string_builder__append_format(&sb, " %s | %s",
                e->event_type == 0 ? "" :
                queue_event__is(e, QUEUE_EVENT__INET_SOCKET) ? "INET" : "UNIX",
                e->event_type == 0 ? "" :
                queue_event__is(e, SOCKET_EVENT__INCOMING_CONNECTION) ? "CONNECTIONS" :
                queue_event__is(e, SOCKET_EVENT__INCOMING_MESSAGE) ? "INCOMING MSG" : "OUTGOING MSG");
        }
        string_builder__append_format(&sb, "\n");
    }
    string_builder__append_format(&sb, "==========================================\n");

    return string_builder__get_string(&sb);
}


#include <memory_allocator.c>
#include <string_builder.c>
#include "logger.c"

#if OS_MAC || OS_FREEBSD
#include "async_queue_kqueue.c"
#elif OS_LINUX
#include "async_queue_epoll.c"
#endif



