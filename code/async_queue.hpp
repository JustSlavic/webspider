#ifndef ASYNC_QUEUE_H
#define ASYNC_QUEUE_H

#include <base.h>

#define MAX_EVENTS 32


struct async_context;
struct async
{
    async_context *impl;

    enum event_type
    {
        EVENT__NONE = 0,

        // first and second byte
        EVENT__CONNECTION  = 0x1,
        EVENT__MESSAGE_IN  = 0x2,
        EVENT__MESSAGE_OUT = 0x4,

        // third and forth byte
        EVENT__INET_SOCKET = 0x10000,
        EVENT__UNIX_SOCKET = 0x20000,
    };

    struct event
    {
        int type;
        int fd;
        uint64 timestamp;
    };
};

struct queue__waiting_result
{
    async::event *events;
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
int async__unregister(struct async_context *context, async::event *event);
queue__waiting_result wait_for_new_events(struct async_context *context, int milliseconds);

struct queue__prune_result
{
    int pruned_count;
    int fds[MAX_EVENTS];
};
typedef struct queue__prune_result queue__prune_result;

queue__prune_result queue__prune(struct async_context *context, uint64 microseconds);

FORCE_INLINE bool32 queue_event__is(async::event *event, async::event_type t)
{
    return (event->type & t) > 0;
}

struct async_context__report
{
    async::event events_in_work[MAX_EVENTS];
};

struct async_context__report async_context__report(struct async_context *context);


#endif // ASYNC_QUEUE_H
