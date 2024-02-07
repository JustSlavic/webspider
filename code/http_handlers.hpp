#ifndef HTTP_HANDLERS_HPP
#define HTTP_HANDLERS_HPP

#include <base.h>
#include "http.hpp"


http::response serve_index_html(http::request);
http::response serve_favicon_ico(http::request);


#endif // HTTP_HANDLERS_HPP
