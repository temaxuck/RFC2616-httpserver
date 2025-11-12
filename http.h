#ifndef HTTP_H
#  define HTTP_H

#  ifdef HTTP_IMPL
#    define HTTP_PARSER_IMPL
#    define HTTP_REQRESP_IMPL
#    define HTTP_SERVER_IMPL
#    define HTTP_PATH_IMPL
#    define HTTP_SOCK_IMPL
#  endif

#  include "include/common.h"
#  include "include/da.h"
#  include "include/err.h"
#  include "include/log.h"
#  include "include/parser.h"
#  include "include/path.h"
#  include "include/reqresp.h"
#  include "include/server.h"
#  include "include/socket.h"
#endif // HTTP_H
