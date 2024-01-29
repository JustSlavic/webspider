#include "async_queue.h"

#include <sys/epoll.h>
#include <sys/time.h>


struct async_context
{
    int queue_fd;
    queue__event_data registered_events[MAX_EVENTS];
};


struct async_context *create_async_context()
{
    auto *context = ALLOCATE(mallocator(), async_context);
    memory__set(context, 0, sizeof(struct async_context));
    context->queue_fd = epoll_create1(0);
    return context;
}


void destroy_async_context(struct async_context *context)
{
    close(context->queue_fd);
    free(context);
}


int queue__register(struct async_context *context, int socket_to_register, int type)
{
    int result = -2;
    for (usize i = 0; i < ARRAY_COUNT(context->registered_events); i++)
    {
        queue__event_data *event = context->registered_events + i;
        if (event->event_type == QUEUE_EVENT__NONE)
        {
            bool to_read  = ((type & SOCKET_EVENT__INCOMING_CONNECTION) != 0) ||
                            ((type & SOCKET_EVENT__INCOMING_MESSAGE) != 0);
            bool to_write = ((type & SOCKET_EVENT__OUTGOING_MESSAGE) != 0);

            int event_types = 0;
            if (to_read && to_write) event_types = EPOLLIN | EPOLLOUT;
            else if (!to_read && to_write) event_types = EPOLLOUT;
            else if (to_read && !to_write) event_types = EPOLLIN;

            struct epoll_event reg_event;
            reg_event.events  = event_types;
            reg_event.data.fd = socket_to_register;

            result = epoll_ctl(context->queue_fd, EPOLL_CTL_ADD, socket_to_register, &reg_event);
            if (result >= 0)
            {
                struct timeval tv;
                gettimeofday(&tv, NULL);

                event->event_type = type;
                event->socket_fd = socket_to_register;
                event->timestamp = 1000000LLU * tv.tv_sec + tv.tv_usec;
            }
            break;
        }
    }

    return result;
}

int async__unregister(struct async_context *context, queue__event_data *event)
{
    close(event->socket_fd);
    memory__set(event, 0, sizeof(queue__event_data));

    return 0;
}

queue__waiting_result wait_for_new_events(struct async_context *context, int milliseconds)
{
    queue__waiting_result result = {};

    struct epoll_event incoming_event;
    int event_count = epoll_wait(context->queue_fd, &incoming_event, 1, milliseconds);
    if (event_count > 0)
    {
        for (usize i = 0; i < ARRAY_COUNT(context->registered_events); i++)
        {
            if (context->registered_events[i].socket_fd == incoming_event.data.fd)
            {
                result.events = context->registered_events + i;
                result.event_count = 1;
                break;
            }
        }
    }

    return result;
}

queue__prune_result queue__prune(struct async_context *context, uint64 microseconds)
{
    queue__prune_result result = {};

    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64 now = 1000000LLU * tv.tv_sec + tv.tv_usec;

    for (usize i = 0; i < ARRAY_COUNT(context->registered_events); i++)
    {
        queue__event_data *event = context->registered_events + i;

        if (queue_event__is(event, SOCKET_EVENT__INCOMING_MESSAGE))
        {
            uint64 dt = now - event->timestamp;
            if (dt > microseconds)
            {
                result.fds[result.pruned_count++] = event->socket_fd;

                close(event->socket_fd);
                memory__set(event, 0, sizeof(queue__event_data));
            }
        }
    }

    return result;
}


struct async_context__report async_context__report(struct async_context *context)
{
    struct async_context__report report;
    memory__copy(report.events_in_work, context->registered_events, sizeof(context->registered_events));

    return report;
}

