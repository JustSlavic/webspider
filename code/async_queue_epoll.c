#include "async_queue.h"


struct async_context;
{
    int queue_fd;
    struct socket_event_data registered_events[MAX_EVENTS];
};


int create_async_context(struct async_context *context)
{
    memory__set(context, 0, sizeof(struct async_context));
    int result = context->queue_fd = epoll_create1(0);
    return result;
}


int register_socket_to_read(struct async_context *context, int socket_to_register, enum socket_event_type type)
{
    int result = -2;
    for (int i = 0; i < ARRAY_COUNT(context->registered_events); i++)
    {
        struct socket_event_data *event = context->registered_events + i;
        if (event->type == SOCKET_EVENT__NONE)
        {
            struct epoll_event reg_event = {
                .events  = EPOLLIN,
                .data.fd = socket_to_register,
            };

            result = epoll_ctl(context->queue_fd, EPOLL_CTL_ADD, socket_to_register, &reg_event);
            if (result < 0)
            {
                printf("Could not register, epoll_ctl failed (errno: %d - \"%s\")\n", errno, strerror(errno));
            }
            else
            {
                event->type = type;
                event->socket_fd = socket_to_register;
            }
            break;
        }
    }

    return result;
}


struct socket_event_data *wait_for_new_events(struct async_context *context)
{
    struct socket_event_data *result = NULL;

    struct epoll_event incoming_event;
    int event_count = epoll_wait(epollfd, &incoming_event, 1, -1);
    if (event_count < 0)
    {
        printf("Error epoll_wait (errno: %d - \"%s\")\n", errno, strerror(errno));
    }
    else if (event_count > 0)
    {
        for (int i = 0; i < ARRAY_COUNT(context->registered_events); i++)
        {
            if (context->registered_events[i].socket_fd == event->data.fd)
            {
                result = context->registered_events + i;
                break;
            }
        }
    }

    return result;
}

