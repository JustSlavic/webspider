#ifndef HTTP_H
#define HTTP_H

#include <base.h>
#include <string_view.hpp>


enum http_request_type
{
    HTTP__NONE,
    HTTP__GET,
    HTTP__POST,
    HTTP__PUT,
    HTTP__DELETE,
};

struct http_request
{
    enum http_request_type type;
    string_view path[16];
    uint32 path_part_count;
    string_view header_keys[16];
    string_view header_vals[16];
    uint32 header_count;
};
typedef struct http_request http_request;

enum http_response_code
{
    HTTP__OK = 200,
    HTTP__NOT_FOUND = 404,
    HTTP__SERVER_ERROR = 500,
};

struct http_response
{
    enum http_response_code code;
};
typedef struct http_response http_response;


char const *http_request_type_to_cstring(enum http_request_type type);

memory_block http_request_to_blob(http_request);
memory_block http_response_to_blob(http_response);

http_request http_request_from_blob(memory_block);
http_response http_response_from_blob(memory_block);


#endif // HTTP_H
