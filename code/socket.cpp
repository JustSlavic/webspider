#include "socket.hpp"


int make_nonblocking(int fd)
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

char const *to_cstring(web::connection::result_code c)
{
    switch (c)
    {
        case web::connection::RECEIVE__OK: return "RECEIVE__OK";
        case web::connection::RECEIVE__DROP: return "RECEIVE__DROP";
        case web::connection::RECEIVE__ERROR: return "RECEIVE__ERROR";
        case web::connection::RECEIVE__OVERFLOW: return "RECEIVE__OVERFLOW";
        default: return "Error";
    }
}

bool32 web::connection::good()
{
    return (fd > 0);
}

bool32 web::connection::fail()
{
    return !good();
}

/*

web::connection::receive_result receive_loop(web::connection &c)
{
    web::connection::receive_result result = {};
    while(c.buffer.used < c.buffer.size)
    {
        auto buffer = c.buffer.get_free();
        auto r = c.receive(buffer.data, buffer.size);

        result.code = r.code;
        result.bytes_received += r.bytes_received;
        if (r.code != web::connection::RECEIVE__OK) break;
    }
    return result;
}

*/

web::connection::receive_result web::connection::receive(void *buffer, usize size)
{
    receive_result result;

    auto bucket = memory_bucket::from(buffer, size);
    while (true)
    {
        auto free_buffer = bucket.get_free();
        int bytes_received = recv(fd, free_buffer.data, free_buffer.size, 0);

        if (bytes_received > 0)
        {
            bucket.used += bytes_received;
            if (bucket.used == bucket.size)
            {
                result.code = RECEIVE__OVERFLOW;
                result.bytes_received = bucket.used;
                break;
            }
        }
        else if (bytes_received == 0)
        {
            result.code = RECEIVE__DROP;
            result.bytes_received = 0;
            break;
        }
        else if (bytes_received < 0)
        {
            result.bytes_received = bucket.used;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // This is actually normal exit for streaming-based connection
                result.code = RECEIVE__OK;
            }
            else
            {
                // This is an error
                result.code = RECEIVE__ERROR;
            }
            break;
        }
        else
        {
            ASSERT_FAIL();
        }
    }

    return result;
}

int web::connection::send(void *buffer, usize size)
{
    NOT_IMPLEMENTED();
    return 0;
}

int web::connection::close()
{
    int result = ::close(fd);
    fd = 0;
    return result;
}


web::listener web::listener::inet(uint32 ip4, uint16 port)
{
    listener result;

    result.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (result.fd > 0)
    {
        make_nonblocking(result.fd);

        struct sockaddr_in addr_inet;
        addr_inet.sin_family      = AF_INET;
        addr_inet.sin_port        = port;
        addr_inet.sin_addr.s_addr = ip4;

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
            ::close(result.fd);
            result.fd = -1;
        }
    }

    return result;
}

web::listener web::listener::unix(string_view filename)
{
    listener result;

    result.fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (result.fd > 0)
    {
        make_nonblocking(result.fd);

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
            ::close(result.fd);
            result.fd = -1;
        }
    }

    return result;
}

bool32 web::listener::good()
{
    return (fd > 0);
}

bool32 web::listener::fail()
{
    return !good();
}

int web::listener::listen(int32 backlog_size)
{
    int ec = ::listen(fd, backlog_size);
    return ec;
}

web::connection web::listener::accept()
{
    struct sockaddr accepted_address;
    socklen_t accepted_address_size = sizeof(accepted_address);

    web::connection result = {};
    result.fd = ::accept(fd, &accepted_address, &accepted_address_size);
    make_nonblocking(result.fd);
    if (accepted_address.sa_family == AF_INET)
    {
        sockaddr_in *sin = (sockaddr_in *) &accepted_address;
        result.ip4  = sin->sin_addr.s_addr;
        result.port = sin->sin_port;
    }
    return result;
}

int web::listener::close()
{
    int result = ::close(fd);
    fd = 0;
    return result;
}

