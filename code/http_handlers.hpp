#ifndef HTTP_HANDLERS_HPP
#define HTTP_HANDLERS_HPP

#include <base.h>
#include "http.h"


http_response serve_index_html(http_request);
http_response serve_favicon_ico(http_request);


#endif // HTTP_HANDLERS_HPP
