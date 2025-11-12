#define HTTP_IMPL
#include "../http.h"

void echo_handler(HTTP_Response *resp, HTTP_Request *req) {
    printf("Request to: %s\n", req->url.path);
    http_response_set_content_length(resp, req->content_length);
    http_response_send(resp, HTTP_Status_OK);

    HTTP_Err err;
    while (true) {
        char chunk[20] = {0};
        err = http_request_read_body_chunk(req, chunk, 20);
        if (err != HTTP_ERR_CONT) {
            if (err != HTTP_ERR_OK) {
                http_response_send(resp, HTTP_Status_INTERNAL_SERVER_ERROR);
                printf("ERROR: Failed to read body chunk: %s\n", http_err_to_cstr(err));
                return;
            }
            break;
        }
        
        err = http_response_write_body_chunk(resp, chunk, 20);
        if (err != HTTP_ERR_OK) {
            http_response_send(resp, HTTP_Status_INTERNAL_SERVER_ERROR);
            printf("ERROR: Failed to write body chunk: %s\n", http_err_to_cstr(err));
            return;
        }
    }

}

int main(void) {
    HTTP_Err err;
    HTTP_Server s = {0};
    http_server_init(&s, "localhost:8080");

    http_server_add_handler(&s, "/*", echo_handler);

    HTTP_INFO("Running server...");
    if ((err = http_server_run(&s))) {
        HTTP_ERROR("Failed to run server: %s", http_err_to_cstr(err));
    }

    http_server_free(&s);
    return 0;
}
