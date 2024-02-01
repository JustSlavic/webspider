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

char const *http_response_code_to_cstring(enum http_response_code code)
{
    switch (code)
    {
        case HTTP__OK: return "OK";
        case HTTP__NOT_FOUND: return "Not Found";
        case HTTP__SERVER_ERROR: return "Internal Server Error";
        // case HTTP__: return "";
        default: return "<Unknown response code>";
    }
    return NULL;
}

int http_request_to_blob(memory_block, http_request); // @todo

int http_response_to_blob(memory_block blk, http_response response)
{
    int result = 0;

    string_builder sb = make_string_builder(blk);
    result += sb.append("HTTP/1.1 %d %s\n", response.code, http_response_code_to_cstring(response.code));

    return result;
}

bool32 is_newline(char c) { return (c == '\n') || (c == '\r'); }
bool32 is_ascii_semicolon(char c) { return (c == ':'); }
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
        string_view path = { .data = get_pointer(&lexer) };
        path.size = consume_until(&lexer, is_ascii_whitespace);

        request.url = http::url{ path };
    }

    consume_until(&lexer, is_newline); // Skip until end of line
    eat_newline(&lexer);

    for (usize header_index = 0; header_index < ARRAY_COUNT(request.header_keys); header_index++)
    {
        char c = get_char(&lexer);
        if (is_newline(c))
        {
            break;
        }
        else
        {
            string_view key_part = { .data = get_pointer(&lexer) };
            key_part.size = consume_until(&lexer, is_ascii_semicolon);

            eat_char(&lexer); // eat semicolon

            consume_while(&lexer, is_ascii_space);

            string_view val_part = { .data = get_pointer(&lexer) };
            val_part.size = consume_until(&lexer, is_newline);

            eat_newline(&lexer);

            request.header_keys[header_index] = key_part;
            request.header_vals[header_index] = val_part;
            request.header_count += 1;
        }
    }

    return request;
}

http_response http_response_from_blob(memory_block);


