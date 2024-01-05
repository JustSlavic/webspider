#ifndef ASYNC_QUEUE_H
#define ASYNC_QUEUE_H

#include <base.h>

#define MAX_EVENTS 32


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

struct async_context
{
    int queue_fd;
    struct socket_event_data registered_events[MAX_EVENTS];
};

int create_async_context(struct async_context *context);
int register_socket_to_read(struct async_context *context, int socket_to_register, enum socket_event_type type);
struct socket_event_data *wait_for_new_events(struct async_context *context);



#endif // ASYNC_QUEUE_H
