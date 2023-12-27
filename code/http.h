#ifndef HTTP_H
#define HTTP_H

#include <base.h>


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
};

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


#endif // HTTP_H
