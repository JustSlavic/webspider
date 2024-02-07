#include "socket.hpp"



int web::socket::make_nonblocking()
{
    int result = 0;

    int server_socket_flags = fcntl(fd, F_GETFL, 0);
    if (server_socket_flags < 0)
    {
        result = -1;
    }
    else
    {
        int set_flags_result = fcntl(fd, F_SETFL, server_socket_flags|O_NONBLOCK);
        if (set_flags_result < 0)
        {
            result = -1;
        }
    }

    return result;
}

bool32 web::socket::is_ok()
{
    return fd > 0;
}

bool32 web::socket::is_fail()
{
    return !is_ok();
}


web::socket web::socket::inet(address a)
{
    web::socket result;

    result.fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (result.fd > 0)
    {
        struct sockaddr_in addr_inet;
        addr_inet.sin_family      = AF_INET;
        addr_inet.sin_port        = a.port;
        addr_inet.sin_addr.s_addr = a.ip4;

        int bind_result = bind(result.fd, (struct sockaddr const *) &addr_inet, sizeof(addr_inet));
        if (bind_result < 0)
        {
            if (errno == EADDRINUSE)
            {
                int reuse = 1;
                setsockopt(result.fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
                bind_result = bind(result.fd, (struct sockaddr const *) &addr_inet, sizeof(addr_inet));
            }
        }

        if (bind_result < 0)
        {
            close(result.fd);
            result.fd = -1;
        }
    }

    return result;
}

web::socket web::socket::unix(string_view filename)
{
    web::socket result;

    result.fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (result.fd > 0)
    {
        struct sockaddr_un name = {
            .sun_family = AF_UNIX
        };
        memory__copy(name.sun_path, filename.data, filename.size);

        int ec = bind(result.fd, (struct sockaddr const *) &name, sizeof(name));
        if (ec < 0)
        {
            if (errno == EADDRINUSE)
            {
                unlink(name.sun_path);
                ec = bind(result.fd, (struct sockaddr const *) &name, sizeof(name));
            }
        }

        if (ec < 0)
        {
            close(result.fd);
            result.fd = -1;
        }
    }

    return result;
}

int web::socket::listen(int32 backlog_size)
{
    int ec = ::listen(fd, backlog_size);
    return ec;
}
