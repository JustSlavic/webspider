#include "http.hpp"
#include <lexer.hpp>


char const *http::to_cstring(http::request_type type)
{
    switch (type)
    {
        case http::GET:    return "GET";
        case http::POST:   return "POST";
        case http::PUT:    return "PUT";
        case http::DELETE: return "DELETE";
        default:           return "NONE";
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

int http::response::serialize_to(memory_buffer buffer)
{
    int result = 0;

    string_builder sb = string_builder::from(buffer);
    result += sb.append("HTTP/1.1 %d %s\n", code, to_cstring(code));

    return result;
}


bool is_ascii_semicolon(char c) { return (c == ':'); }
bool is_space_or_slash_or_question(char c) { return (c == ' ') || (c == '/') || (c == '?'); }

http::request http::request::deserialize(memory_buffer blob)
{
    http::request request = {};
    lexer l = lexer::from(blob.data, blob.size);

    request.type = l.eat_string("GET") ? http::GET :
                   l.eat_string("POST") ? http::POST :
                   l.eat_string("PUT") ? http::PUT :
                   l.eat_string("DELETE") ? http::DELETE :
                   http::NONE;

    char c = l.get_char();
    if (c == ' ')
    {
        l.eat_char();

        // Parsing URL
        string_view path;
        path.data = l.get_remaining_input();
        path.size = l.consume_until(is_ascii_whitespace);

        request.url = path;
    }

    l.consume_until(is_newline); // Skip until end of line
    l.eat_char(); // eat newline character

    for (usize header_index = 0; header_index < ARRAY_COUNT(request.header_keys); header_index++)
    {
        char c = l.get_char();
        if (c == 0)
        {
            break;
        }
        else if (c == '\n')
        {
            break;
        }
        else
        {
            string_view key_part;
            key_part.data = l.get_remaining_input();
            key_part.size = l.consume_until(is_ascii_semicolon);
            l.eat_char(); // eat semicolon

            l.consume_while(is_ascii_space);

            string_view val_part;
            val_part.data = l.get_remaining_input();
            val_part.size = l.consume_until(is_newline);
            l.eat_char(); // eat newline character

            request.header_keys[header_index] = key_part;
            request.header_vals[header_index] = val_part;
            request.header_count += 1;
        }
    }

    l.consume_while(is_newline);

    request.body.data = l.get_remaining_input();
    request.body.size = l.consume_until(is_eof);

    return request;
}

string_view http::request::get_header_value(string_view key)
{
    for (int header_index = 0; header_index < header_count; header_index++)
    {
        if (header_keys[header_index] == key)
        {
            return header_vals[header_index];
        }
    }
    return string_view{};
}

// http_response http_response_from_blob(memory_block);


