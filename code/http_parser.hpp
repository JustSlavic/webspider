#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <base.h>
#include <lexer.hpp>
#include "http.hpp"


struct http_parser
{
    enum state
    {
        IDLE,
        PARSING_START_LINE,
        PARSING_HEADERS,
        PARSING_BODY,
    };

    state status;

    int parse_request(void *buffer, usize size, http::request & request);
};

char const *to_cstring(http_parser::state s);


#endif // HTTP_PARSER_HPP
