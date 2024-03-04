#ifndef ASYNC_QUEUE_H
#define ASYNC_QUEUE_H

#include <base.h>
#include <memory_bucket.hpp>


#define ASYNC_MAX_LISTENERS   4
#define ASYNC_MAX_CONNECTIONS 32


struct async_impl;
struct async
{
    async_impl *impl;

    enum event_type
    {
        EVENT__NONE = 0,

        // first and second byte
        EVENT__LISTENER = 0x1,
        EVENT__CONNECTION = 0x2,

        // third and forth byte
        EVENT__INET_DOMAIN = 0x10000,
        EVENT__UNIX_DOMAIN = 0x20000,
    };

    struct event
    {
        int32 type;
        union
        {
            web::listener listener;
            web::connection connection;
        };

        uint64 create_time;
        uint64 update_time;

        FORCE_INLINE
        bool32 is(event_type t)
        {
            return (type & t) > 0;
        }
    };

    enum register_result
    {
        REG_SUCCESS,
        REG_FAILED,
        REG_NO_SLOTS,
    };

    struct wait_result
    {
        async::event *event;
        union // Negative number is error code, positive or 0 is ok
        {
            uint32 event_count; // Always 1
            int32  error_code;
        };
    };

    struct prune_result
    {
        int pruned_count;
        int fds[ASYNC_MAX_CONNECTIONS];
        memory_buffer mem[ASYNC_MAX_CONNECTIONS];
    };

    struct report_result
    {
        async::event listeners[ASYNC_MAX_LISTENERS];
        async::event connections[ASYNC_MAX_CONNECTIONS];
    };

    static async create_context();
    void destroy_context();

    bool32 is_valid();

    register_result register_listener(web::listener listener, int event_type);
    register_result register_connection(web::connection connection, int event_type);
    int unregister(event *e);

    wait_result wait_for_events(int milliseconds);
    prune_result prune(uint64 microseconds);
    report_result report();
};




#endif // ASYNC_QUEUE_H
