#include "async_queue.h"

#include <sys/epoll.h>


struct async_context
{
    int queue_fd;
    struct socket_event_data registered_events[MAX_EVENTS];
};


struct async_context *create_async_context()
{
    struct async_context *context = malloc(sizeof(struct async_context));
    memory__set(context, 0, sizeof(struct async_context));
    context->queue_fd = epoll_create1(0);
    return context;
}


void destroy_async_context(struct async_context *context)
{
    close(context->queue_fd);
    free(context);
}


int register_socket(struct async_context *context, int socket_to_register, enum socket_event_type type)
{
    int result = -2;
    for (int i = 0; i < ARRAY_COUNT(context->registered_events); i++)
    {
        struct socket_event_data *event = context->registered_events + i;
        if (event->type == SOCKET_EVENT__NONE)
        {
            bool to_read  = ((type & SOCKET_EVENT__INCOMING_CONNECTION) != 0) || ((type & SOCKET_EVENT__INCOMING_MESSAGE) != 0);
            bool to_write = ((type & SOCKET_EVENT__OUTGOING_MESSAGE) != 0);

            int event_types = 0;
            if (to_read && to_write) event_types = EPOLLIN | EPOLLOUT;
            else if (!to_read && to_write) event_types = EPOLLOUT;
            else if (to_read && !to_write) event_types = EPOLLIN;

            struct epoll_event reg_event = {
                .events  = event_types,
                .data.fd = socket_to_register,
            };

            result = epoll_ctl(context->queue_fd, EPOLL_CTL_ADD, socket_to_register, &reg_event);
            if (result >= 0)
            {
                event->type = type;
                event->socket_fd = socket_to_register;
            }
            break;
        }
    }

    return result;
}


struct socket_event_waiting_result wait_for_new_events(struct async_context *context, int milliseconds)
{
    struct socket_event_waiting_result result = {};

    struct epoll_event incoming_event;
    int event_count = epoll_wait(context->queue_fd, &incoming_event, 1, milliseconds);
    if (event_count > 0)
    {
        for (int i = 0; i < ARRAY_COUNT(context->registered_events); i++)
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

