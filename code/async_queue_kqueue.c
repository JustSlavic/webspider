#include "async_queue.h"

#include <stdlib.h>
#include <sys/event.h>
#include <sys/time.h>


struct async_context
{
    int queue_fd;
    queue__event_data registered_events[MAX_EVENTS];
};


struct async_context *create_async_context()
{
    struct async_context *context = malloc(sizeof(struct async_context));
    memory__set(context, 0, sizeof(struct async_context));
    context->queue_fd = kqueue();
    return context;
}

void destroy_async_context(struct async_context *context)
{
    close(context->queue_fd);
    free(context);
}

int queue__register(struct async_context *context, int socket_to_register, int event_type)
{
    int result = -2;
    for (int i = 0; i < ARRAY_COUNT(context->registered_events); i++)
    {
        queue__event_data *event = context->registered_events + i;
        if (event->event_type == QUEUE_EVENT__NONE)
        {
            bool to_read  = (event_type & SOCKET_EVENT__INCOMING_CONNECTION) != 0 ||
                            (event_type & SOCKET_EVENT__INCOMING_MESSAGE) != 0;
            bool to_write = (event_type & SOCKET_EVENT__OUTGOING_MESSAGE) != 0;

            struct kevent reg_events[2] = {}; // 0 - read, 1 - write
            EV_SET(&reg_events[0], socket_to_register, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, event);
            EV_SET(&reg_events[1], socket_to_register, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, event);

            if (to_read && to_write)
                result = kevent(context->queue_fd, reg_events, 2, NULL, 0, NULL);
            else if (to_read && !to_write)
                result = kevent(context->queue_fd, &reg_events[0], 1, NULL, 0, NULL);
            else if (!to_read && to_write)
                result = kevent(context->queue_fd, &reg_events[1], 1, NULL, 0, NULL);

            if (result < 0)
            {
                printf("Could not register, kevent failed (errno: %d - \"%s\")\n", errno, strerror(errno));
            }
            else
            {
                struct timeval tv;
                gettimeofday(&tv, NULL);

                event->event_type = event_type;
                event->socket_fd = socket_to_register;
                event->timestamp = 1000000LLU * tv.tv_sec + tv.tv_usec;
            }
            break;
        }
    }

    return result;
}

queue__waiting_result wait_for_new_events(struct async_context *context, int milliseconds)
{
    queue__waiting_result result = {};

    struct timespec timeout = { 0, 1000 * milliseconds };

    struct kevent incoming_event;
    int event_count = kevent(context->queue_fd, NULL, 0, &incoming_event, 1, &timeout);
    if (event_count > 0 && (incoming_event.flags & EV_ERROR))
    {
        // Ignore error messages for now
        result.event_count = 0;
    }
    else if (event_count > 0)
    {
        result.events = incoming_event.udata;
        result.event_count = 1;
    }
    else
    {
        result.error_code = event_count;
    }

    return result;
}

queue__prune_result queue__prune(struct async_context *context, uint64 microseconds)
{
    queue__prune_result result = {};

    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64 now = 1000000LLU * tv.tv_sec + tv.tv_usec;

    for (int i = 0; i < ARRAY_COUNT(context->registered_events); i++)
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

