#include "async.hpp"

#include <stdlib.h>
#include <sys/event.h>
#include <sys/time.h>


struct async_impl
{
    int queue_fd;
    async::event registered_listeners[ASYNC_MAX_LISTENERS];
    async::event registered_connections[ASYNC_MAX_CONNECTIONS];
};


async async::create_context()
{
    async result;
    result.impl = mallocator().allocate<async_impl>();
    if (result.impl)
    {
        result.impl->queue_fd = kqueue();
    }
    return result;
}

bool32 async::is_valid()
{
    return (impl && impl->queue_fd > 0);
}

void async::destroy_context()
{
    close(impl->queue_fd);
    mallocator().deallocate(impl, sizeof(async_impl));
}

async::register_result async::register_listener(web::listener listener, int event_type)
{
    register_result result = REG_NO_SLOTS;
    for (int i = 0; i < ASYNC_MAX_LISTENERS; i++)
    {
        async::event *event = impl->registered_listeners + i;
        if (event->type == async::EVENT__NONE)
        {
            struct kevent reg_event;
            EV_SET(&reg_event, listener.fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, event);
            int ec = kevent(impl->queue_fd, &reg_event, 1, NULL, 0, NULL);
            if (ec < 0)
            {
                result = REG_FAILED;
            }
            else
            {
                struct timeval tv;
                gettimeofday(&tv, NULL);

                event->type = event_type;
                event->listener = listener;
                event->update_time = 1000000LLU * tv.tv_sec + tv.tv_usec;
                event->create_time = 1000000LLU * tv.tv_sec + tv.tv_usec;

                result = REG_SUCCESS;
            }
            break;
        }
    }

    return result;
}

async::register_result async::register_connection(web::connection connection, int event_type)
{
    register_result result = REG_NO_SLOTS;
    for (int i = 0; i < ASYNC_MAX_CONNECTIONS; i++)
    {
        async::event *event = impl->registered_connections + i;
        if (event->type == async::EVENT__NONE)
        {
            struct kevent reg_event;
            EV_SET(&reg_event, connection.fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, event);
            int ec = kevent(impl->queue_fd, &reg_event, 1, NULL, 0, NULL);
            if (ec < 0)
            {
                result = REG_FAILED;
            }
            else
            {
                struct timeval tv;
                gettimeofday(&tv, NULL);

                event->type = event_type;
                event->connection = connection;
                event->update_time = 1000000LLU * tv.tv_sec + tv.tv_usec;
                event->create_time = 1000000LLU * tv.tv_sec + tv.tv_usec;

                result = REG_SUCCESS;
            }
            break;
        }
    }

    return result;
}

int async::unregister(event *e)
{
    if (e->is(EVENT__CONNECTION))
    {
        e->connection.close();
    }
    else if (e->is(EVENT__LISTENER))
    {
        e->listener.close();
    }
    memory__set(e, 0, sizeof(async::event));
    return 0;
}

async::wait_result async::wait_for_events(int milliseconds)
{
    wait_result result = {};
    struct timespec timeout = { 0, 1000 * milliseconds };

    struct kevent incoming_event;
    int event_count = kevent(impl->queue_fd, NULL, 0, &incoming_event, 1, &timeout);
    if (event_count > 0 && (incoming_event.flags & EV_ERROR))
    {
        // Ignore error messages for now
        result.event_count = 0;
    }
    else if (event_count > 0)
    {
        result.event = (async::event *) incoming_event.udata;
        result.event_count = 1;
    }
    else
    {
        result.error_code = event_count;
    }

    return result;
}

async::prune_result async::prune(uint64 microseconds)
{
    prune_result result = {};

    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64 now = 1000000LLU * tv.tv_sec + tv.tv_usec;

    for (int i = 0; i < ASYNC_MAX_CONNECTIONS; i++)
    {
        async::event *event = impl->registered_connections + i;
        if (event->is(async::EVENT__CONNECTION))
        {
            uint64 dt = now - event->update_time;
            if (dt > microseconds)
            {
                result.fds[result.pruned_count++] = event->connection.fd;

                close(event->connection.fd);
                memory__set(event, 0, sizeof(async::event));
            }
        }
    }

    return result;
}

async::report_result async::report()
{
    report_result report;
    memory__copy(report.listeners, impl->registered_listeners, sizeof(impl->registered_listeners));
    memory__copy(report.connections, impl->registered_connections, sizeof(impl->registered_connections));

    return report;
}

