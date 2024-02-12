#ifndef WEB_SOCKET_HPP
#define WEB_SOCKET_HPP

#include <base.h>
#include <memory_bucket.hpp>
#include <string_view.hpp>

#include "http.hpp"
#include "http_parser.hpp"

#define IP4_ANY 0
#define IP4(x, y, z, w) ((((uint8) w) << 24) | (((uint8) z) << 16) | (((uint8) y) << 8) | ((uint8) x))
#define IP4_LOCALHOST IP4(127, 0, 0, 1)

#define PORT(n) uint16__change_endianness((n))


namespace web {


struct connection
{
    int fd;
    uint32 ip4;
    uint16 port;

    memory_bucket buffer;
    http_parser   parser;
    http::request request;

    enum result_code
    {
        RECEIVE__OK,
        RECEIVE__DROP,
        RECEIVE__ERROR,
        RECEIVE__OVERFLOW,
    };

    struct receive_result
    {
        result_code code;
        int bytes_received;
    };

    bool32 good();
    bool32 fail();

    receive_result receive(void *buffer, usize size);
    int send(void *buffer, usize size);
    int close();
};

struct listener
{
    int fd;

    static listener inet(uint32 ip4, uint16 port);
    static listener unix(string_view name);

    bool32 good();
    bool32 fail();

    int listen(int32 backlog_size);
    connection accept();
    int close();
};


} // namespace web

char const *to_cstring(web::connection::result_code c);

#endif // WEB_SOCKET_HPP
