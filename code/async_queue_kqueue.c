#include "async_queue.h"

#include <stdlib.h>
#include <sys/event.h>


struct async_context
{
    int queue_fd;
    struct socket_event_data registered_events[MAX_EVENTS];
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

int register_socket_to_read(struct async_context *context, int socket_to_register, enum socket_event_type type)
{
    int result = -2;
    for (int i = 0; i < ARRAY_COUNT(context->registered_events); i++)
    {
        struct socket_event_data *event = context->registered_events + i;
        if (event->type == SOCKET_EVENT__NONE)
        {
            struct kevent reg_event;
            EV_SET(&reg_event, socket_to_register,
                   EVFILT_READ, EV_ADD | EV_CLEAR,
                   0, 0, event);
            result = kevent(context->queue_fd, &reg_event, 1,
                            NULL, 0, NULL);
            if (result < 0)
            {
                printf("Could not register, kevent failed (errno: %d - \"%s\")\n", errno, strerror(errno));
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

    struct kevent incoming_event;
    int event_count = kevent(context->queue_fd,
                              NULL, 0,
                              &incoming_event, 1,
                              NULL);
    if (event_count < 0)
    {
        printf("Error kevent incoming_event (errno: %d - \"%s\")\n", errno, strerror(errno));
    }
    else if (event_count > 0 && (incoming_event.flags & EV_ERROR))
    {
        printf("Error kevent incoming_event returned error event (errno: %d - \"%s\")\n", errno, strerror(errno));
    }
    else if (event_count > 0)
    {
        result = incoming_event.udata;
    }

    return result;
}

