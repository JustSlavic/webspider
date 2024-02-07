#include "http_handlers.hpp"


http::response serve_index_html(http::request)
{
    http::response response = { http::OK };
    return response;
}

http::response serve_favicon_ico(http::request)
{
    http::response response = { http::NOT_FOUND };
    return response;
}
