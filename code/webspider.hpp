#ifndef WEBSPIDER_H
#define WEBSPIDER_H

#include <base.h>
#include <array.h>
#include <logger.h>
#include "http.h"
#include "async_queue.hpp"


typedef http_response (*process_request_cb)(http_request);

struct webspider
{
    int socket_fd;
    int socket_for_inspector;

    ::async async;

    memory_allocator webspider_allocator;
    memory_allocator connection_allocator;

    // @todo: change string_id to http::url or something
    string_id          route_table__keys[64];
    process_request_cb route_table__vals[64];
    uint32 route_table_count;

    struct logger *logger;

    // array(uint32) ip_ban_list;
};



#endif // WEBSPIDER_H
