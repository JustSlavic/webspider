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

        FORCE_INLINE bool32 is(event_type t)
        {
            return (type & t) > 0;
        }
    };

    struct wait_result
    {
        async::event *events;
        union // Negative number is error code, positive or 0 is ok
        {
            uint32 event_count;
            int32  error_code;
        };
    };

    struct prune_result
    {
        int pruned_count;
        int fds[MAX_EVENTS];
    };

    struct report_result
    {
        async::event events_in_work[MAX_EVENTS];
    };

    static async create_context();
    void destroy_context();

    bool32 is_valid();

    int register_socket_for_async_io(int socket, int event_type);
    int unregister(event *e);

    wait_result wait_for_events(int milliseconds);
    prune_result prune(uint64 microseconds);
    report_result report();
};




#endif // ASYNC_QUEUE_H
