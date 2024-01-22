#include "http.h"
#include <lexer.h>


char const *http_request_type_to_cstring(enum http_request_type type)
{
    switch (type)
    {
    case HTTP__GET: return "GET";
    case HTTP__POST: return "POST";
    case HTTP__PUT: return "PUT";
    case HTTP__DELETE: return "DELETE";
    default: return "NONE";
    }
    return "Error";
}

memory_block http_request_to_blob(http_request);
memory_block http_response_to_blob(http_response);

bool32 is_newline(char c) { return (c == '\n') ; }

http_request http_request_from_blob(memory_block blob)
{
    http_request request = {};
    struct lexer lexer = make_lexer(blob);

    bool is_get = eat_cstring(&lexer, "GET");
    if (is_get)
    {
        request.type = HTTP__GET;
    }
    else
    {
        bool is_post = eat_cstring(&lexer, "POST");
        if (is_post)
        {
            request.type = HTTP__POST;
        }
        else
        {
            bool is_put = eat_cstring(&lexer, "PUT");
            if (is_put)
            {
                request.type = HTTP__PUT;
            }
            else
            {
                bool is_delete = eat_cstring(&lexer, "DELETE");
                if (is_delete)
                {
                    request.type = HTTP__DELETE;
                }
            }
        }
    }

    return request;
}

http_response http_response_from_blob(memory_block);


