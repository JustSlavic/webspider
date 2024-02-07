#include "http.hpp"
#include <lexer.h>


char const *http::to_cstring(http::request_type type)
{
    switch (type)
    {
        case http::GET: return "GET";
        case http::POST: return "POST";
        case http::PUT: return "PUT";
        case http::DELETE: return "DELETE";
        default: return "NONE";
    }
    return "Error";
}

char const *http::to_cstring(http::response_code code)
{
    switch (code)
    {
        case http::OK: return "OK";
        case http::NOT_FOUND: return "Not Found";
        case http::SERVER_ERROR: return "Internal Server Error";
        // case HTTP__: return "";
        default: return "<Unknown response code>";
    }
    return NULL;
}

int http::response::serialize_to(memory_block blk)
{
    int result = 0;

    string_builder sb = make_string_builder(blk);
    result += sb.append("HTTP/1.1 %d %s\n", code, to_cstring(code));

    return result;
}

bool32 is_newline(char c) { return (c == '\n') || (c == '\r'); }
bool32 is_ascii_semicolon(char c) { return (c == ':'); }
bool32 is_space_or_slash_or_question(char c) { return (c == ' ') || (c == '/') || (c == '?'); }

http::request http::request::deserialize(memory_block blob)
{
    http::request request = {};
    struct lexer lexer = make_lexer(blob);

    request.type = eat_cstring(&lexer, "GET") ? http::GET :
                   eat_cstring(&lexer, "POST") ? http::POST :
                   eat_cstring(&lexer, "PUT") ? http::PUT :
                   eat_cstring(&lexer, "DELETE") ? http::DELETE :
                   http::NONE;

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
        if (c == 0)
        {
            break;
        }
        else if (is_newline(c))
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

// http_response http_response_from_blob(memory_block);


