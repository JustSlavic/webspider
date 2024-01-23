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
bool32 is_space_or_slash_or_question(char c) { return (c == ' ') || (c == '/') || (c == '?'); }

http_request http_request_from_blob(memory_block blob)
{
    http_request request = {};
    struct lexer lexer = make_lexer(blob);

    request.type = eat_cstring(&lexer, "GET") ? HTTP__GET :
                   eat_cstring(&lexer, "POST") ? HTTP__POST :
                   eat_cstring(&lexer, "PUT") ? HTTP__PUT :
                   eat_cstring(&lexer, "DELETE") ? HTTP__DELETE :
                   HTTP__NONE;

    char c = get_char(&lexer);
    if (c == ' ')
    {
        eat_char(&lexer);

        // Parsing URL

        for (int part_index = 0; part_index < ARRAY_COUNT(request.path); part_index++)
        {
            char c = get_char(&lexer);
            if (c == '/')
            {
                eat_char(&lexer);
                c = get_char(&lexer);
                if (c != ' ')
                {
                    string_view path_part = { .data = get_pointer(&lexer) };
                    path_part.size = consume_until(&lexer, is_space_or_slash_or_question);

                    request.path[part_index] = path_part;
                    request.path_part_count += 1;
                }
            }
                else if (c == '?')
            {
                consume_until(&lexer, is_ascii_space);
                break;
            }
            else
            {
                break;
            }
        }
    }

    return request;
}

http_response http_response_from_blob(memory_block);


