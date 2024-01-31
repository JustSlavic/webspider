#include "http_handlers.hpp"


http_response serve_index_html(http_request)
{
    http_response response = { HTTP__OK };
    return response;
}

http_response serve_favicon_ico(http_request)
{
    http_response response = { HTTP__NOT_FOUND };
    return response;
}
