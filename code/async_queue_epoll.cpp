#include "async_queue.hpp"

#include <sys/epoll.h>
#include <sys/time.h>


struct async_context
{
    int queue_fd;
    async::event registered_events[MAX_EVENTS];
};


async async::create_context()
{
    async result;
    result.impl = ALLOCATE(mallocator(), async_context);
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
    DEALLOCATE(mallocator(), impl);
}

int async::register_socket_for_async_io(int socket, int event_type)
{
    int result = -2;
    for (usize i = 0; i < ARRAY_COUNT(impl->registered_events); i++)
    {
        async::event *event = impl->registered_events + i;
        if (event->type == async::EVENT__NONE)
        {
            bool to_read  = ((event_type & async::EVENT__CONNECTION) != 0) ||
                            ((event_type & async::EVENT__MESSAGE_IN) != 0);
            bool to_write = ((event_type & async::EVENT__MESSAGE_OUT) != 0);

            int event_types = 0;
            if (to_read && to_write) event_types = EPOLLIN | EPOLLOUT;
            else if (!to_read && to_write) event_types = EPOLLOUT;
            else if (to_read && !to_write) event_types = EPOLLIN;

            struct epoll_event reg_event;
            reg_event.events  = event_types;
            reg_event.data.fd = socket;

            result = epoll_ctl(impl->queue_fd, EPOLL_CTL_ADD, socket, &reg_event);
            if (result < 0)
            {
                printf("Could not register, epoll failed (errno: %d - \"%s\")\n", errno, strerror(errno));
            }
            else
            {
                struct timeval tv;
                gettimeofday(&tv, NULL);

                event->type = event_type;
                event->fd = socket;
                event->timestamp = 1000000LLU * tv.tv_sec + tv.tv_usec;
            }
            break;
        }
    }

    return result;
}

int async::unregister(async::event *event)
{
    close(event->fd);
    memory__set(event, 0, sizeof(event));

    return 0;
}

async::wait_result async::wait_for_events(int milliseconds)
{
    wait_result result = {};

    struct epoll_event incoming_event;
    int event_count = epoll_wait(impl->queue_fd, &incoming_event, 1, milliseconds);
    if (event_count > 0)
    {
        for (usize i = 0; i < ARRAY_COUNT(impl->registered_events); i++)
        {
            if (impl->registered_events[i].fd == incoming_event.data.fd)
            {
                result.events = impl->registered_events + i;
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

    for (usize i = 0; i < ARRAY_COUNT(impl->registered_events); i++)
    {
        async::event *event = impl->registered_events + i;

        if (event->is(async::EVENT__MESSAGE_IN))
        {
            uint64 dt = now - event->timestamp;
            if (dt > microseconds)
            {
                result.fds[result.pruned_count++] = event->fd;

                close(event->fd);
                memory__set(event, 0, sizeof(event));
            }
        }
    }

    return result;
}


async::report_result async::report()
{
    report_result report;
    memory__copy(report.events_in_work, impl->registered_events, sizeof(impl->registered_events));

    return report;
}

