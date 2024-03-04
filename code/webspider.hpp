#ifndef WEBSPIDER_H
#define WEBSPIDER_H

#include <base.h>
#include <logger.hpp>

#include "gen/config.hpp"
#include "socket.hpp"
#include "http.hpp"
#include "async.hpp"


enum response_type
{
    SERVER_RESPONSE__STATIC,
    SERVER_RESPONSE__DYNAMIC,
};

typedef http::response request_handler(http::request);
struct response_data
{
    union
    {
        struct
        {
            string_view filename;
            string_view content_type;
        };
        request_handler *cb;
    };
};

struct webspider
{
    web::listener webspider_listener;
    web::listener inspector_listener;

    ::async async;

    memory_allocator arena;
    memory_allocator pool;

    // @todo: change string_id to http::url or something
    string_id     route_table__keys[64];
    response_type route_table__type[64];
    response_data route_table__vals[64];
    uint32        route_table__count;

    // array(uint32) ip_ban_list;
};



#endif // WEBSPIDER_H
