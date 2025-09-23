#define HTTP_IMPL
#include "http.h"

int main(void) {
    HTTP_Server s = {.addr=":8080"};
    int err = http_server_run_forever(&s);
    if (err != HTTP_ERR_OK) {
        HTTP_ERROR("Failed to start server: %d", err);
    }; 
}
