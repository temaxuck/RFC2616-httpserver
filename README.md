# httplib

Small HTTP library, written in C from scratch.

> [!WARNING]
> This project was created **solely for learning purposes** — to explore how
> one might implement HTTP library from scratch in C.

> [!WARNING]
> This project is still WIP.

## Quick start

```c
#define HTTP_IMPL
#include "http.h"

void hello_handler(HTTP_Response *resp, HTTP_Request *req) {
    HTTP_INFO("Request to: %s\n", req->url.path);
    
    char *body = "Hello, World!\r\n";
    size_t body_sz = strlen(body);
    http_response_set_content_length(resp, body_sz);
    http_response_add_header(resp, "Content-Type", "text/plain");
    
    http_response_send(resp, HTTP_Status_OK);
    http_response_write_body_chunk(resp, body, body_sz);
}

int main(void) {
    HTTP_Err err;
    HTTP_Server s = {0};
    http_server_init(&s, "localhost:8080");

    http_server_add_handler(&s, "/*", hello_handler);

    HTTP_INFO("Running server...");
    if ((err = http_server_run(&s))) {
        HTTP_ERROR("Failed to run server: %s", http_err_to_cstr(err));
    }

    http_server_free(&s);
    return 0;
}
```

## Usage

The library consists of the following header files:

```
include/
├── common.h  # common stuff
├── da.h      # dynamic array
├── err.h     # errors
├── io.h      # IO (from https://github.com/temaxuck/io.h) 
├── log.h     # logging
├── parser.h  # HTTP message parser
├── path.h    # Path pattern matching 
├── reqresp.h # HTTP Request / Response
├── server.h  # HTTP Server
├── socket.h  # Low-level socket operations
└── url.h     # URL parser
```

Each one of these header files contains implementation, which you can switch
on, usually, by defining some symbol (such as `#define HTTP_SERVER_IMPL`).

If you don't want to mess with each header file individually, you may just
include `http.h` (located in the root directory of this project), like this:

```c
// Uncomment line below, if you want to include implementation of the library
// #define HTTP_IMPL
#include "http.h"
```

### Logging

There are 4 levels of logs: `TODO`, `INFO`, `WARN`, `ERROR`. To disable any of
them you should simply redefine macro `HTTP_<LEVEL>(...)` to nothing at the
top of the script, e.g.:

```c
#define HTTP_TODO(...)
#define HTTP_INFO(...)
#define HTTP_WARN(...)
#define HTTP_ERROR(...)

#define HTTP_SERVER_IMPL
#include "server.h"

// ... rest of your code
```
