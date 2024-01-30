#include "async_queue.hpp"

#include <stdlib.h>
#include <sys/event.h>
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
    DEALLOCATE(mallocator(), impl);
}

int async::register_socket_for_async_io(int socket, int event_type)
{
    int result = -2;
    for (int i = 0; i < ARRAY_COUNT(impl->registered_events); i++)
    {
        async::event *event = impl->registered_events + i;
        if (event->type == async::EVENT__NONE)
        {
            bool to_read  = ((event_type & async::EVENT__CONNECTION) != 0) ||
                            ((event_type & async::EVENT__MESSAGE_IN) != 0);
            bool to_write = ((event_type & async::EVENT__MESSAGE_OUT) != 0);

            struct kevent reg_events[2] = {}; // 0 - read, 1 - write
            EV_SET(&reg_events[0], socket, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, event);
            EV_SET(&reg_events[1], socket, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, event);

            if (to_read && to_write)
                result = kevent(impl->queue_fd, reg_events, 2, NULL, 0, NULL);
            else if (to_read && !to_write)
                result = kevent(impl->queue_fd, &reg_events[0], 1, NULL, 0, NULL);
            else if (!to_read && to_write)
                result = kevent(impl->queue_fd, &reg_events[1], 1, NULL, 0, NULL);

            if (result < 0)
            {
                printf("Could not register, kevent failed (errno: %d - \"%s\")\n", errno, strerror(errno));
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
    memory__set(event, 0, sizeof(async::event));

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
        result.events = (async::event *) incoming_event.udata;
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

    for (int i = 0; i < ARRAY_COUNT(impl->registered_events); i++)
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

