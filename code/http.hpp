#ifndef HTTP_H
#define HTTP_H

#include <base.h>
#include <string_view.hpp>


namespace http
{

struct url
{
    string_view path;
};

enum request_type
{
    NONE,
    GET,
    POST,
    PUT,
    DELETE,
};

char const *to_cstring(request_type type);


struct request
{
    request_type type;

    http::url url;
    uint32 path_part_count;
    string_view header_keys[32];
    string_view header_vals[32];
    uint32 header_count;
    string_view body;

    int serialize_to(memory_block);
    static http::request deserialize(memory_block);
};

enum response_code
{
    OK = 200,
    NOT_FOUND = 404,
    SERVER_ERROR = 500,
};

char const *to_cstring(response_code code);


struct response
{
    response_code code;

    int serialize_to(memory_block);
    static http::response deserialize(memory_block);
};

} // namespace http


#endif // HTTP_H
