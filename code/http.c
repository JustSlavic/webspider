#include "http.h"
#include <lexer.h>


memory_block http_request_to_blob(http_request);
memory_block http_response_to_blob(http_response);

http_request http_request_from_blob(memory_block blob)
{
    http_request result = {};

    struct lexer lexer = make_lexer(blob);

    bool is_get = eat_cstring(lexer, "GET");
    if (is_get)
    {
        char c = get_char(lexer);
        if (c == ' ')
        {
            consume_until("\n");
        }
    }



    return result;
}

http_response http_response_from_blob(memory_block);


