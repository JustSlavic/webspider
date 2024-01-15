#ifndef ASYNC_QUEUE_H
#define ASYNC_QUEUE_H

#include <base.h>

#define MAX_EVENTS 32


enum queue__event_type
{
    QUEUE_EVENT__NONE,
    SOCKET_EVENT__INCOMING_CONNECTION = 0x1,
    SOCKET_EVENT__INCOMING_MESSAGE    = 0x2,
    SOCKET_EVENT__OUTGOING_MESSAGE    = 0x4,

    QUEUE_EVENT__INET_SOCKET = 0x10000000,
    QUEUE_EVENT__UNIX_SOCKET = 0x20000000,
};
typedef enum queue__event_type queue__event_type;

struct queue__event_data
{
    int event_type;
    int socket_fd;
};
typedef struct queue__event_data queue__event_data;

struct queue__waiting_result
{
    queue__event_data *events;
    union // Negative number is error code, positive or 0 is ok
    {
        uint32 event_count;
        int32  error_code;
    };
};
typedef struct queue__waiting_result queue__waiting_result;

struct async_context;

struct async_context *create_async_context();
void destroy_async_context(struct async_context *context);
int queue__register(struct async_context *context, int socket_to_register, int event_type);
queue__waiting_result wait_for_new_events(struct async_context *context, int milliseconds);

FORCE_INLINE bool32 queue_event__is(queue__event_data *event, queue__event_type t)
{
    return (event->event_type & t) > 0;
}


#endif // ASYNC_QUEUE_H
