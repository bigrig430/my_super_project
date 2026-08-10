#include <iostream>
#include <string>
#include <zlib.h>
#include <uv.h>
#include <llhttp.h>

// --- libuv TCP server callback: when data arrives ---
void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = (char*) malloc(suggested_size);
    buf->len = suggested_size;
}

void echo_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
    if (nread > 0) {
        std::cout << "Received: " << std::string(buf->base, nread) << std::endl;

        // Echo back to client
        uv_write_t* req = (uv_write_t*) malloc(sizeof(uv_write_t));
        uv_buf_t wrbuf = uv_buf_init(buf->base, nread);
        uv_write(req, client, &wrbuf, 1, nullptr);
        return;
    }

    if (nread < 0) {
        uv_close((uv_handle_t*) client, nullptr);
    }

    free(buf->base);
}

void on_new_connection(uv_stream_t* server, int status) {
    if (status < 0) {
        std::cerr << "New connection error" << std::endl;
        return;
    }

    uv_tcp_t* client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    uv_tcp_init(server->loop, client);

    if (uv_accept(server, (uv_stream_t*) client) == 0) {
        uv_read_start((uv_stream_t*) client, alloc_buffer, echo_read);
    } else {
        uv_close((uv_handle_t*) client, nullptr);
    }
}

int main() {
    // --- zlib test ---
    const char* text = "Mitchell testing zlib!";
    char buffer[200];
    uLongf bufferSize = sizeof(buffer);
    compress((Bytef*)buffer, &bufferSize, (const Bytef*)text, strlen(text));
    std::cout << "zlib OK: compressed size = " << bufferSize << std::endl;

    // --- llhttp test ---
    llhttp_t parser;
    llhttp_init(&parser, HTTP_REQUEST, NULL);
    std::cout << "llhttp OK: parser initialized" << std::endl;

    // --- libuv TCP server ---
    uv_loop_t* loop = uv_default_loop();

    uv_tcp_t server;
    uv_tcp_init(loop, &server);

    struct sockaddr_in addr;
    uv_ip4_addr("127.0.0.1", 9001, &addr);

    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);

    int r = uv_listen((uv_stream_t*)&server, 128, on_new_connection);
    if (r) {
       std::cerr << "Listen error: " << uv_strerror(r) << std::endl;o

        return 1;
    }

    std::cout << "HTTP server running on port 9001" << std::endl;

    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
}
