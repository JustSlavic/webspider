#ifndef WEB_SOCKET_HPP
#define WEB_SOCKET_HPP

#include <base.h>
#include <string_view.hpp>

#define IP4_ANY 0
#define IP4(x, y, z, w) ((((uint8) w) << 24) | (((uint8) z) << 16) | (((uint8) y) << 8) | ((uint8) x))
#define IP4_LOCALHOST IP4(127, 0, 0, 1)

#define PORT(n) uint16__change_endianness((n))


namespace web {

struct address
{
    uint32 ip4;
    uint16 port;
};

struct socket
{
    int fd;

    static socket inet(address a);
    static socket unix(string_view name);

    bool32 is_ok();
    bool32 is_fail();
    int listen(int32 backlog_size);
    int make_nonblocking();
};


} // namespace web

#endif // WEB_SOCKET_HPP
