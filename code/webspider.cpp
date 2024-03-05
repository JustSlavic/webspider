// Based
#include <base.h>
#include <math/integer.h>
#include <memory/memory.hpp>
#include <memory/allocator.hpp>
#include <string_builder.hpp>
#include <math/float32.h>
#include <logger.hpp>
#include <string_id.hpp>
#include <acf.hpp>
#include <util.hpp>
#include <fs.hpp>
#include <web.hpp>
#include <platform.hpp>

// *nix
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <arpa/inet.h>

// Stdlib
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

// Project-specific
#include "webspider.hpp"
#include "context.hpp"
#include "version.h"

#include "http_parser.hpp"
#include "http_handlers.hpp"


enum process_connection_result
{
    CLOSE_CONNECTION,
    KEEP_CONNECTION,
};

process_connection_result process_connection(context *ctx, webspider *server, web::connection &c);
void respond_to_requst(context *ctx, webspider *server, web::connection &c);
void serve_static_file(context *ctx, webspider *server, web::connection &connection, char const *filename, char const *content_type);
memory_bucket prepare_report(webspider *server);


// @todo
// - add used config to inspector report
// - add acf-based command-line parser

bool is_symbol_ok(char c)
{
    return (('0' <= c) && (c <= '9')) ||
           (('a' <= c) && (c <= 'z')) ||
           (('A' <= c) && (c <= 'Z')) ||
            (c == '.') || (c == ',')  ||
            (c == ':') || (c == ';')  ||
            (c == '!') || (c == '?')  ||
            (c == '@') || (c == '#')  ||
            (c == '$') || (c == '%')  ||
            (c == '^') || (c == '&')  ||
            (c == '*') || (c == '~')  ||
            (c == '(') || (c == ')')  ||
            (c == '<') || (c == '>')  ||
            (c == '[') || (c == ']')  ||
            (c == '{') || (c == '}')  ||
            (c == '-') || (c == '+')  ||
            (c == '/') || (c == '\\') ||
            (c == '_') || (c == '|')  ||
            (c == '"') || (c == '\'') ||
            (c == '`') || (c == '=')  ||
            (c == ' ') || (c == '\n') ||
            (c == '\r')|| (c == '\t');
}

void log_untrusted_impl(logger *l, code_location cl, char const *buffer, usize size)
{
    char trusted[KILOBYTES(4)] = {};
    uint32 cursor = 0;

    trusted[cursor++] = '\n';

    for (usize i = 0; i < size; i++)
    {
        char c = buffer[i];
        if (is_symbol_ok(c) && cursor < ARRAY_COUNT(trusted))
        {
            trusted[cursor++] = c;
        }
        else if ((cursor + 5) < ARRAY_COUNT(trusted))
        {
            snprintf(trusted + cursor, KILOBYTES(4) - cursor, "\\0x%02x", (int) (c & 0xff));
            cursor += 5;
        }
        else
        {
            break;
        }
    }
    l->log(cl, "%s", trusted);
}

#define LOG_UNTRUSTED(BUFFER, SIZE) log_untrusted_impl(logger__, CL_HERE, (char const *) (BUFFER), (SIZE))


GLOBAL volatile bool running;

void signal__SIGINT(int dummy) {
    running = false;
}


int main()
{
    signal(SIGINT, signal__SIGINT);

    auto webspider_memory_usage = MEGABYTES(20);
    auto string_id_arena_size = MEGABYTES(1);

    webspider server;
    server.arena = mallocator()->allocate_arena(webspider_memory_usage);

    auto string_id_arena = server.arena.allocate_arena(string_id_arena_size);
    string_id::initialize(string_id_arena);

    context ctx;
    {
        auto config_data = platform::load_file("config.acf", mallocator());
        ctx.config = config::load(mallocator(), config_data);
    }

    server.async = {};
    server.pool  = server.arena.allocate_pool(ctx.config.memory_usage, ctx.config.memory_usage_per_connection);
    server.route_table__count = 0;

    {
        auto mapping_data = platform::load_file("mapping.acf", &server.arena);
        auto mapping = acf::parse(&server.arena, mapping_data);

        auto get_map = mapping.get_value("GET");
        for (auto v : get_map.values())
        {
            if (v.is_object())
            {
                auto url = v.get_value("url").to_string("");
                auto file = v.get_value("serve_file").to_string("");
                auto content_type = v.get_value("content_type").to_string("");
                if (!file.is_empty())
                {
                    server.route_table__keys[server.route_table__count] = string_id::from(url.data, url.size);
                    server.route_table__type[server.route_table__count] = SERVER_RESPONSE__STATIC;
                    server.route_table__vals[server.route_table__count].filename = file;
                    server.route_table__vals[server.route_table__count].content_type = content_type;
                    server.route_table__count += 1;
                }
            }
        }
    }

    auto logger_buffer = server.arena.allocate_buffer(MEGABYTES(1));
    struct logger logger_ = {};
    logger_.sb = string_builder::from(logger_buffer);
    if (ctx.config.logger.stream)
    {
        logger_.type = logger_.type | LOGGER__STREAM;
        logger_.fd = 1; // stdout
    }
    if (ctx.config.logger.file)
    {
        logger_.type = logger_.type | LOGGER__FILE;
        logger_.filename = ctx.config.logger.filename;
        logger_.rotate_size = ctx.config.logger.max_size;
    }
    ctx.logger = &logger_;
    LOGGER(&ctx);

    LOG("-------------------------------------");
    LOG("Starting initialization...");

    server.async = async::create_context();
    if (!server.async.is_valid())
    {
        LOG("Error async queue");
        return EXIT_FAILURE;
    }

    server.inspector_listener = web::listener::unix(ctx.config.unix_domain_socket);
    if (server.inspector_listener.fail())
    {
        LOG("Error: could not create Unix Domain Socket (errno: %d - \"%s\")\n", errno, strerror(errno));
    }
    else
    {
        int reg_result = server.async.register_listener(
            server.inspector_listener,
            async::EVENT__LISTENER | async::EVENT__UNIX_DOMAIN);
        if (reg_result == async::REG_FAILED)
        {
            LOG("Error: could not register listener (errno: %d - \"%s\")", errno, strerror(errno));
        }
        else if (reg_result == async::REG_NO_SLOTS)
        {
            LOG("Error coult not add to the async system because all %d slots in the array are occupied", ASYNC_MAX_LISTENERS);
        }
        else
        {
            server.inspector_listener.listen(ctx.config.backlog_size);
        }
    }

    server.webspider_listener = web::listener::inet(IP4_ANY, PORT(80));
    if (server.webspider_listener.fail())
    {
        LOG("Error: could not create Internet Domain Socket (errno: %d - \"%s\")\n", errno, strerror(errno));
    }
    else
    {
        auto reg_result = server.async.register_listener(server.webspider_listener,
            async::EVENT__LISTENER | async::EVENT__INET_DOMAIN);
        if (reg_result == async::REG_FAILED)
        {
            LOG("Error: could not register listener (errno: %d - \"%s\")", errno, strerror(errno));
        }
        else if (reg_result == async::REG_NO_SLOTS)
        {
            LOG("Error coult not add to the async system because all %d slots in the array are occupied", ASYNC_MAX_LISTENERS);
        }
        else
        {
            server.webspider_listener.listen(ctx.config.backlog_size);

            LOG("Successfully started webspider version %s", version);
            LOG("Memory usage: %4.2fMb", MEGABYTES_FROM_BYTES(webspider_memory_usage));
            LOG("-------------- WELCOME --------------");

            running = true;
        }

        while(running)
        {
            auto wait_result = server.async.wait_for_events(ctx.config.wait_timeout); // timeout in milliseconds
            if (wait_result.error_code < 0)
            {
                if (errno != EINTR)
                {
                    LOG("Could not receive events (return code: %d) (errno: %d - \"%s\")", wait_result.error_code, errno, strerror(errno));
                }
            }
            else if (wait_result.event_count == 0)
            {
                auto prune_result = server.async.prune(ctx.config.prune_timeout * 10); // timeout in microseconds
                if (prune_result.pruned_count > 0)
                {
                    for (int i = 0; i < prune_result.pruned_count; i++)
                    {
                        if (prune_result.mem[i].data == NULL) continue;
                        LOG("Deallocating %4.2fKb of memory back to pool allocator", KILOBYTES_FROM_BYTES(prune_result.mem[i].size));
                        server.pool.deallocate(prune_result.mem[i]);
                    }

                    char buffer[1024] = {};
                    {
                        auto bucket = memory_bucket::from(buffer, 1024);
                        bucket.append("%d", prune_result.fds[0]);
                        for (int i = 1; i < prune_result.pruned_count; i++)
                        {
                            bucket.append(", %d", prune_result.fds[i]);
                        }
                    }

                    LOG("Pruned %d connections: %s", prune_result.pruned_count, buffer);
                }
            }
            else if (wait_result.event_count > 0)
            {
                async::event *event = wait_result.event;
                if (event->is(async::EVENT__INET_DOMAIN))
                {
                    if (event->is(async::EVENT__LISTENER))
                    {
                        web::connection connection = event->listener.accept();
                        LOG("Accepted connection: (fd: %d)", connection.fd);
                        connection.buffer = memory_bucket::from(
                            server.pool.allocate_buffer(KILOBYTES(5)));
                        LOG("Allocated %4.2fKb from webspider allocator", KILOBYTES_FROM_BYTES(connection.buffer.size));

                        auto res = process_connection(&ctx, &server, connection);
                        if (res == KEEP_CONNECTION)
                        {
                            async::register_result reg_result = server.async.register_connection(connection,
                                async::EVENT__CONNECTION | async::EVENT__INET_DOMAIN);
                            if (reg_result == async::REG_FAILED)
                            {
                                LOG("Error: could not register connection to async queue (errno: %d - \"%s\")", errno, strerror(errno));
                            }
                            else if (reg_result == async::REG_NO_SLOTS)
                            {
                                LOG("Error couldn't add to the async queue because all %d slots in the array are occupied", ASYNC_MAX_CONNECTIONS);
                            }
                            else
                            {
                                LOG("Added to async queue");
                                continue;
                            }
                        }
                        LOG("Deallocating %4.2fKb of memory back to pool allocator", KILOBYTES_FROM_BYTES(connection.buffer.size));
                        server.pool.deallocate(connection.buffer.get_buffer());
                        LOG("close(%d)", connection.fd);
                        connection.close();
                    }
                    else if (event->is(async::EVENT__CONNECTION))
                    {
                        auto res = process_connection(&ctx, &server, event->connection);
                        if (res == KEEP_CONNECTION)
                        {
                            LOG("Processed new data on the connection, but it is not done yet, wait more");
                            continue;
                        }
                        LOG("Deallocating %4.2fKb of memory back to pool allocator", KILOBYTES_FROM_BYTES(event->connection.buffer.size));
                        server.pool.deallocate(event->connection.buffer.get_buffer());
                        LOG("unregister connection(%d)", event->connection.fd);
                        server.async.unregister(event);
                    }
                }
                else if (event->is(async::EVENT__UNIX_DOMAIN))
                {
                    if (event->is(async::EVENT__LISTENER))
                    {
                        auto conn = event->listener.accept();

                        char buffer[512];
                        int received_bytes = recv(conn.fd, buffer, sizeof(buffer), 0);
                        if (received_bytes < 0)
                        {
                            LOG("Could not receive request from Unix Domain Socket (errno: %d - \"%s\")", errno, strerror(errno));
                        }
                        auto report = prepare_report(&server);
                        int sent_bytes = send(conn.fd, report.data, report.used, 0);
                        if (sent_bytes < 0)
                        {
                            LOG("Could not send report to Unix Domain Socket (errno: %d - \"%s\")", errno, strerror(errno));
                        }
                        mallocator()->deallocate(report.data, report.size);
                    }
                    else if (event->is(async::EVENT__CONNECTION))
                    {
                    }
                }
            }
        }

        LOG("Closing server (socket: %d)", server.webspider_listener.fd);
        server.webspider_listener.close();
    }

    {
        char uds_name_cstr[512] = {};
        memcpy(uds_name_cstr, ctx.config.unix_domain_socket.data, ctx.config.unix_domain_socket.size);
        unlink(uds_name_cstr);
    }
    if (server.inspector_listener.good())
    {
        LOG("Closing inspector (socket: %d)", server.inspector_listener.fd);
        server.inspector_listener.close();
    }

    server.async.destroy_context();

    return 0;
}

process_connection_result process_connection(context *ctx, webspider *server, web::connection &c)
{
    LOGGER(ctx);

    auto next_part = c.buffer.get_free();
    web::connection::receive_result rres = c.receive(next_part.data, next_part.size);
    if (rres.code == web::connection::RECEIVE__OK)
    {
        if (rres.bytes_received == 0)
        {
            LOG("The message did not arrive yet. Wait for it a bit.");
            return KEEP_CONNECTION;
        }

        c.buffer.used += rres.bytes_received;

        LOG("Received %d bytes from connection", rres.bytes_received);
        LOG_UNTRUSTED(next_part.data, rres.bytes_received);

        LOG("Http parser started in state '%s'", to_cstring(c.parser.status));
        int parsed_bytes = c.parser.parse_request(next_part.data, rres.bytes_received, c.request);
        if (parsed_bytes < rres.bytes_received)
        {
            LOG("Error: could not parse received data, just drop connection");
            return CLOSE_CONNECTION;
        }
        LOG("After parsing that part, http parser stopped in state '%s'", to_cstring(c.parser.status));

        auto content_length_str = c.request.get_header_value(string_view::from("Content-Length"));
        int content_length = to_int(content_length_str.data, content_length_str.size);

        LOG("Content-Length header gives off value '%d'", content_length);

        if ((c.parser.status == http_parser::PARSING_BODY) &&
            (c.request.body.size == (usize) content_length))
        {
            LOG("Parser reached body, and size of parsed body matches Content-Length");
            respond_to_requst(ctx, server, c);
            return CLOSE_CONNECTION;
        }

        LOG("Parsing did not reach body or did not parse enough of content-length (parsed %d bytes, content-length is %d bytes)", c.request.body.size, content_length);
        return KEEP_CONNECTION;
    }
    else if (rres.code == web::connection::RECEIVE__DROP)
    {
        LOG("Receive returned 0 that means that peer dropped connection");
    }
    else if (rres.code == web::connection::RECEIVE__ERROR)
    {
        LOG("Error: Could not receive any data from a connection (errno: %d - \"%s\")", errno, strerror(errno));
    }
    else // if (rres.code == web::connection::RECEIVE__OVERFLOW)
    {
        char payload[] =
            "HTTP/1.1 413 Request Entity Too Large\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 135\r\n"
            "\r\n"
            "<!DOCTYPE html>\r\n"
            "<html>\r\n"
            "<head>\r\n"
            "    <title>413 Request Entity Too Large</title>\r\n"
            "</head>\r\n"
            "<body>\r\n"
            "    <h1>413 Request Entity Too Large</h1>\r\n"
            "    <p>The request entity is too large and exceeds the server's configured limit. Please reduce the size of your request and try again.</p>\r\n"
            "</body>\r\n"
            "</html>\r\n";
        isize bytes_sent = send(c.fd, payload, sizeof(payload) - 1, 0);
        if (bytes_sent < 0)
        {
            LOG("Could not send anything back (errno: %d - \"%s\")", errno, strerror(errno));
        }
        else
        {
            LOG("Sent back '413 - Request Entity Too Large' message over http", bytes_sent);
        }

        LOG("Error: client sent me too long of a request, we drop such connections");
    }
    return CLOSE_CONNECTION;
}

void respond_to_requst(context *ctx, webspider *server, web::connection &connection)
{
    LOGGER(ctx);

    if (connection.request.type == http::GET)
    {
        bool ok = false;
        auto response_memory = memory_bucket::from(connection.buffer.get_free());
        {
            for (uint32 path_index = 0; path_index < server->route_table__count; path_index++)
            {
                if (server->route_table__keys[path_index] == string_id::from(connection.request.url))
                {
                    if (server->route_table__type[path_index] == SERVER_RESPONSE__STATIC)
                    {
                        auto filename = server->route_table__vals[path_index].filename;
                        auto content_type = server->route_table__vals[path_index].content_type;

                        char filename_buffer[512] = {};
                        memcpy(filename_buffer, filename.data, filename.size);

                        auto f = file::open(filename_buffer);

                        response_memory.append("HTTP/1.1 200 OK\n");
                        response_memory.append("Content-Length: %d\n", f.size());
                        response_memory.append("Content-Type: %.*s\n", content_type.size, content_type.data);
                        response_memory.append("\n");

                        auto payload = response_memory.get_free();
                        isize payload_size = f.read(payload.data, payload.size);
                        response_memory.used += payload_size;
                        ok = true;

                        f.close();
                    }
                    break;
                }
            }

            if (!ok)
            {
                response_memory.append("HTTP/1.1 404 Not Found\n");
                response_memory.append("Server: Webspider\n");
                response_memory.append("Content-Length: 32\n");
                response_memory.append("Content-Type: text/plain\n");
                response_memory.append("\n");
                response_memory.append("Such web page does not exist :(\n");
                LOG("Prepared HTTP 404 - Not Found");
            }

            {
                isize bytes_sent = send(connection.fd, response_memory.data, response_memory.used, 0);
                if (bytes_sent < 0)
                {
                    LOG("Could not send anything back (errno: %d - \"%s\")", errno, strerror(errno));
                }
                else
                {
                    LOG("Sent back %lld bytes of http", bytes_sent);
                }
            }
        }
    }
    else if (connection.request.type == http::POST)
    {
        if (string_id::from(connection.request.url) == string_id::from("/message"))
        {
            LOG("INCOMING MESSAGE FROM A PERSON!!!");

            int fd = open("messages.txt", O_WRONLY | O_APPEND | O_CREAT, 0644);
            if (fd < 0) LOG("COULD NOT OPEN FILE (errno: %d - \"%s\")", errno, strerror(errno));

            auto decoded = server->pool.allocate_buffer(KILOBYTES(4));
            int decoded_size = web::url_decode(memory_buffer::from((void *) connection.request.body.data, connection.request.body.size), decoded);

            auto written = write(fd, decoded.data, decoded_size);
            write(fd, "\n\n", 2);
            if (written < 0) LOG("COULD NOT WRITE FILE (errno: %d - \"%s\")", errno, strerror(errno));
            else LOG("Written %lld bytes to \"message.txt\"", written);
            serve_static_file(ctx, server, connection, "redirect.html", "text/html");

            server->pool.deallocate(decoded);
        }
    }
    else
    {
        char payload[] =
            "HTTP/1.1 404 Not Found\n"
            "Content-Type: text/html; charset=utf-8\n"
            "\n"
            "fuck you\n";
        isize bytes_sent = send(connection.fd, payload, sizeof(payload) - 1, 0);
        if (bytes_sent < 0)
        {
            LOG("Could not send anything back (errno: %d - \"%s\")", errno, strerror(errno));
        }
        else
        {
            LOG("Sent back 'fu' message over http", bytes_sent);
        }
        LOG("Did not recongnize http method - ignore request, just close connection!");
    }
}

void serve_static_file(context *ctx, webspider *server, web::connection &connection, char const *filename, char const *content_type)
{
    LOGGER(ctx);

    LOG("Serving static file \"%s\"", filename);

    auto response_memory = memory_bucket::from(connection.buffer.get_free());
    auto f = file::open(filename);

    response_memory.append("HTTP/1.1 200 OK\n");
    response_memory.append("Content-Length: %d\n", f.size());
    response_memory.append("Content-Type: %s\n", content_type);
    response_memory.append("\n");

    auto payload = response_memory.get_free();
    isize payload_size = f.read(payload.data, payload.size);
    f.close();

    response_memory.used += payload_size;

    isize bytes_sent = send(connection.fd, response_memory.data, response_memory.used, 0);
    if (bytes_sent < 0)
    {
        LOG("Could not send anything back (errno: %d - \"%s\")", errno, strerror(errno));
    }
    else
    {
        LOG("Sent back %lld bytes of http", bytes_sent);
    }
}

memory_bucket prepare_report(webspider *server)
{
    // @todo: move this "rendering" part to the inspector,
    // It is more convinient to pass only data;
    char spaces[]  = "                                        ";
    char squares[] = "########################################";

    auto report1 = server->arena.get_report();
    int n_spaces1 = truncate_to_int32(40.0f * report1.used / report1.size);
    auto report2 = server->pool.get_report();
    int n_spaces2 = truncate_to_int32(40.0f * report2.used / report2.size);

    auto report = server->async.report();

    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64 now = 1000000LLU * tv.tv_sec + tv.tv_usec;

    auto buffer = mallocator()->allocate_buffer(KILOBYTES(3));
    auto sb = memory_bucket::from(buffer);

    sb.append("           Webspider v%s\n", version);
    // sb.append("Connections done: %llu at rate (%4.2f / sec)\n", connections_done, connections_per_second);
    sb.append("========= MEMORY ALLOCATOR REPORT ========\n");
    sb.append("webspider allocator: %llu / %llu bytes used;\n", report1.used, report1.size);
    sb.append("+----------------------------------------+\n");
    sb.append("|%.*s%.*s|\n", n_spaces1, squares, 40 - n_spaces1, spaces);
    sb.append("+----------------------------------------+\n");
    sb.append("connection allocator: %llu / %llu bytes used; size of chunk = %d; chunks used: %d / %d\n",
        report2.used, report2.size,
        report2.chunk_size, report2.chunks_used, report2.chunk_count);
    sb.append("+----------------------------------------+\n");
    sb.append("|%.*s%.*s|\n", n_spaces2, squares, 40 - n_spaces2, spaces);
    sb.append("+----------------------------------------+\n");
    sb.append("==========================================\n");
    sb.append("ASYNC SERVER BUFFER:               created         updated\n");
    for (usize i = 0; i < ASYNC_MAX_LISTENERS; i++)
    {
        async::event *e = report.listeners + i;
        if (e->listener.fd <= 0)
        {
            continue;
        }
        sb.append("%2d)", i+1);
        sb.append(" [%5d]", e->listener.fd);
        sb.append(" %s | %s",
            e->is(async::EVENT__INET_DOMAIN) ? "INET" :
            e->is(async::EVENT__UNIX_DOMAIN) ? "UNIX" : "????",
            e->is(async::EVENT__LISTENER) ? "SERVER    " :
            e->is(async::EVENT__CONNECTION) ? "CONNECTION" : "   ???    ");
        float32 dt_create = (float32) (now - e->create_time) / 1000000.f;
        float32 dt_update = (float32) (now - e->update_time) / 1000000.f;
        sb.append(" %10.2fs ago %10.2fs ago\n", dt_create, dt_update);
    }

    sb.append("CONNECTION SOCKETS:                created         updated\n");
    for (usize i = 0; i < ARRAY_COUNT(report.connections); i++)
    {
        async::event *e = report.connections + i;

        // Skip empties
        if (e->type == 0)
        {
            int n_empty_entries = 0;
            for (usize j = i; j < ARRAY_COUNT(report.connections); j++)
            {
                async::event *q = report.connections + i;
                if (q->type == 0) n_empty_entries += 1;
                else break;
            }
            if (n_empty_entries > 2)
            {
                sb.append("...\n");
                i += (n_empty_entries - 1);
                continue;
            }
        }

        sb.append("%2d)", i+1);
        if (e->listener.fd > 0)
            sb.append(" [%5d]", e->listener.fd);
        else
            sb.append(" [     ]");
        sb.append(" %s | %s",
            e->is(async::EVENT__INET_DOMAIN) ? "INET" :
            e->is(async::EVENT__UNIX_DOMAIN) ? "UNIX" : "????",
            e->is(async::EVENT__LISTENER) ? "SERVER    " :
            e->is(async::EVENT__CONNECTION) ? "CONNECTION" : "   ???    ");
        float32 dt_create = (float32) (now - e->create_time) / 1000000.f;
        float32 dt_update = (float32) (now - e->update_time) / 1000000.f;
        sb.append(" %10.2fs ago %10.2fs ago\n", dt_create, dt_update);
    }
    sb.append("==========================================\n");

    return sb;
}



#include <memory/allocator.cpp>
#include <string_builder.cpp>
#include <string_id.cpp>
#include <logger.cpp>
#include <acf.cpp>
#include <util.cpp>
#include <fs.cpp>
#include <web.cpp>
#include <os/platform_posix.cpp>

#include "http.cpp"
#include "gen/version.c"
#include "gen/config.cpp"
#include "http_handlers.cpp"

#if OS_MAC || OS_FREEBSD
#include "async_queue_kqueue.cpp"
#elif OS_LINUX
#include "async_queue_epoll.cpp"
#endif

#include "socket.cpp"
#include "lexer.cpp"
#include "http_parser.cpp"
#include "memory_bucket.cpp"
