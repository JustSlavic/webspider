#include "async.hpp"

#include <sys/epoll.h>
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
    result.impl = mallocator()->allocate<async_impl>();
    if (result.impl)
    {
        result.impl->queue_fd = epoll_create1(0);
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
    mallocator()->deallocate(impl, sizeof(async_impl));
}

async::register_result async::register_listener(web::listener listener, int event_type)
{
    register_result result = REG_NO_SLOTS;
    for (int i = 0; i < ASYNC_MAX_LISTENERS; i++)
    {
        async::event *event = impl->registered_listeners + i;
        if (event->type == async::EVENT__NONE)
        {
            struct epoll_event reg_event;
            reg_event.events  = EPOLLIN;
            reg_event.data.fd = listener.fd;

            int ec = epoll_ctl(impl->queue_fd, EPOLL_CTL_ADD, listener.fd, &reg_event);
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
            struct epoll_event reg_event;
            reg_event.events  = EPOLLIN;
            reg_event.data.fd = connection.fd;

            int ec = epoll_ctl(impl->queue_fd, EPOLL_CTL_ADD, connection.fd, &reg_event);
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


int async::unregister(async::event *e)
{
    if (e->is(EVENT__CONNECTION))
    {
        e->connection.close();
    }
    else if (e->is(EVENT__LISTENER))
    {
        e->listener.close();
    }
    memset(e, 0, sizeof(async::event));
    return 0;
}

async::wait_result async::wait_for_events(int milliseconds)
{
    wait_result result = {};

    struct epoll_event incoming_event;
    int event_count = epoll_wait(impl->queue_fd, &incoming_event, 1, milliseconds);
    if (event_count > 0)
    {
        for (usize i = 0; i < ARRAY_COUNT(impl->registered_listeners); i++)
        {
            if (impl->registered_listeners[i].listener.fd == incoming_event.data.fd)
            {
                result.event = impl->registered_listeners + i;
                result.event_count = 1;
                break;
            }
        }
        for (usize i = 0; i < ARRAY_COUNT(impl->registered_connections); i++)
        {
            if (impl->registered_connections[i].connection.fd == incoming_event.data.fd)
            {
                result.event = impl->registered_connections + i;
                result.event_count = 1;
                break;
            }
        }
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
                result.fds[result.pruned_count] = event->connection.fd;
                result.mem[result.pruned_count] = event->connection.buffer.get_buffer();
                result.pruned_count += 1;

                close(event->connection.fd);
                memset(event, 0, sizeof(async::event));
            }
        }
    }

    return result;
}

async::report_result async::report()
{
    report_result report;
    memcpy(report.listeners, impl->registered_listeners, sizeof(impl->registered_listeners));
    memcpy(report.connections, impl->registered_connections, sizeof(impl->registered_connections));

    return report;
}

