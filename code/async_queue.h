#ifndef ASYNC_QUEUE_H
#define ASYNC_QUEUE_H

#include <base.h>

#define MAX_EVENTS 32


struct webspider;

enum socket_event_type
{
    SOCKET_EVENT__NONE,
    SOCKET_EVENT__INCOMING_CONNECTION,
    SOCKET_EVENT__INCOMING_MESSAGE,
};

struct socket_event_data
{
    enum socket_event_type type;
    int socket_fd;
};

struct socket_event_waiting_result
{
    struct socket_event_data *events;
    union // Negative number is error code, positive or 0 is ok
    {
        uint32 event_count;
        int32  error_code;
    };
};

struct async_context;

struct async_context *create_async_context();
void destroy_async_context(struct async_context *context);
int register_socket_to_read(struct async_context *context, int socket_to_register, enum socket_event_type type);
struct socket_event_waiting_result wait_for_new_events(struct async_context *context);



#endif // ASYNC_QUEUE_H
