#ifndef WEBSPIDER_H
#define WEBSPIDER_H

#include <base.h>
#include <array.h>
#include <logger.h>

#include "gen/config.hpp"
#include "http.h"
#include "async_queue.hpp"


enum response_type
{
    SERVER_RESPONSE__STATIC,
    SERVER_RESPONSE__DYNAMIC,
};

typedef http_response handle_request_cb(http_request);
struct response_data
{
    union
    {
        struct
        {
            string_view filename;
            string_view content_type;
        };
        handle_request_cb *cb;
    };
};

struct webspider
{
    int socket_fd;
    int socket_for_inspector;

    ::async async;

    memory_allocator webspider_allocator;
    memory_allocator connection_allocator;

    // @todo: change string_id to http::url or something
    string_id     route_table__keys[64];
    response_type route_table__type[64];
    response_data route_table__vals[64];
    uint32        route_table__count;

    ::config  config;
    ::logger *logger;

    // array(uint32) ip_ban_list;
};



#endif // WEBSPIDER_H
