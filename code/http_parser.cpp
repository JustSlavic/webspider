#include "http_parser.hpp"


char const *to_cstring(http_parser::state s)
{
    switch (s)
    {
        case http_parser::IDLE: return "IDLE";
        case http_parser::PARSING_START_LINE: return "PARSING_START_LINE";
        case http_parser::PARSING_HEADERS: return "PARSING_HEADERS";
        case http_parser::PARSING_BODY: return "PARSING_BODY";
        default: return "Error";
    }
}

bool is_semicolon(char c) { return (c == ':'); }

int http_parser::parse_request(void *buffer, usize size, http::request & request)
{
    auto l = lexer::from(buffer, size);

    if (status == PARSING_START_LINE) goto http_parser__start_line;
    if (status == PARSING_HEADERS) goto http_parser__headers;
    if (status == PARSING_BODY) goto http_parser__body;

http_parser__start_line: status = PARSING_START_LINE;
    {
        request.type = l.eat_string("GET") ? http::GET :
                       l.eat_string("POST") ? http::POST :
                       l.eat_string("PUT") ? http::PUT :
                       l.eat_string("DELETE") ? http::DELETE :
                       http::NONE;

        if (request.type == http::NONE) goto stop_parsing;
        char space1 = l.get_char();
        if (space1 == 0) goto stop_parsing;
        if (space1 == ' ')
        {
            l.eat_char();
            string_view path;
            path.data = l.get_remaining_input();
            path.size = l.consume_until(is_ascii_space);

            char space2 = l.get_char();
            if (space2 == 0) goto stop_parsing;
            if (space2 == ' ')
            {
                request.url = path;

                l.consume_until(is_crlf); // Skip until end of line
                char r = l.get_char();
                char n = l.get_char(1);
                if (r == 0 || n == 0) goto stop_parsing;
                l.eat_crlf();
                status = PARSING_HEADERS;
            }
        }
    }
http_parser__headers: status = PARSING_HEADERS;
    {
        while (true)
        {
            char c = l.get_char();
            if (c == '\r' || c == '\n')
            {
                l.eat_crlf();
                break; // Break from the loop, but continue to parse body
            }

            char const *header_key = l.get_remaining_input();
            int header_key_length = l.consume_until(is_semicolon);
            if (header_key_length == 0) goto stop_parsing;

            char sc = l.get_char();
            if (sc == 0) goto stop_parsing;
            l.eat_char(); // consume semicolon

            l.consume_while(is_ascii_whitespace); // consume whitespaces after semicolon like in 'key: value'

            char const *header_val = l.get_remaining_input();
            int header_val_length = l.consume_until(is_crlf);
            if (header_val_length == 0) goto stop_parsing;

            char r = l.get_char();
            char n = l.get_char(1);
            if (r == 0 || n == 0) goto stop_parsing;
            l.eat_crlf();

            request.header_keys[request.header_count] = string_view::from(header_key, header_key_length);
            request.header_vals[request.header_count] = string_view::from(header_val, header_val_length);
            request.header_count += 1;
        }
    }
http_parser__body: status = PARSING_BODY;
    {
        char const *body = l.get_remaining_input();
        int bytes_consumed = l.consume_until(is_eof);
        request.body = string_view::from(body, bytes_consumed);
    }
stop_parsing:

    return l.cursor;
}

