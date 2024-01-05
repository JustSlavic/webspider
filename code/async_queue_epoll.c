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

    printf("\nwaiting for new events...")
    printf("available events: [(%d: %s)", context->registered_events[0].socket_fd,
                                          context->registered_events[0].type == SOCKET_EVENT__NONE ? "none" :
                                          context->registered_events[0].type == SOCKET_EVENT__INCOMING_CONNECTION ? "incoming_connection" :
                                          context->registered_events[0].type == SOCKET_EVENT__INCOMING_MESSAGE ? "incoming_message");
    for (int i = 1; i < ARRAY_COUNT(context->registered_events); i++)
    {
        printf(", (%d: %s)", context->registered_events[i].socket_fd,
                             context->registered_events[i].type == SOCKET_EVENT__NONE ? "none" :
                             context->registered_events[i].type == SOCKET_EVENT__INCOMING_CONNECTION ? "incoming_connection" :
                             context->registered_events[i].type == SOCKET_EVENT__INCOMING_MESSAGE ? "incoming_message");

    }
    printf("]\n");

    struct epoll_event incoming_event;
    int event_count = epoll_wait(context->queue_fd, &incoming_event, 1, -1);
    if (event_count < 0)
    {
        printf("Error epoll_wait (errno: %d - \"%s\")\n", errno, strerror(errno));
    }
    else if (event_count > 0)
    {
        for (int i = 0; i < ARRAY_COUNT(context->registered_events); i++)
        {
            if (context->registered_events[i].socket_fd == incoming_event.data.fd)
            {
                result = context->registered_events + i;
                break;
            }
        }
    }

    return result;
}

