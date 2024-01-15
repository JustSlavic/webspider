#ifndef WEBSPIDER_H
#define WEBSPIDER_H

#include <base.h>
#include <logger.h>
#include "async_queue.h"


struct webspider
{
    int socket_fd;
    int socket_for_inspector;

    struct async_context *async;

    memory_allocator webspider_allocator;
    memory_allocator connection_allocator;

    struct logger *logger;
};



#endif // WEBSPIDER_H
