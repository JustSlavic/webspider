#ifndef WEBSPIDER_H
#define WEBSPIDER_H

#include <base.h>
#include <array.h>
#include <logger.h>
#include "async_queue.hpp"


struct webspider
{
    int socket_fd;
    int socket_for_inspector;

    async async;

    memory_allocator webspider_allocator;
    memory_allocator connection_allocator;

    struct logger *logger;

    // array(uint32) ip_ban_list;
};



#endif // WEBSPIDER_H
