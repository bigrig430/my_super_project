#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <uv.h>
#include <llhttp.h>

// --- client state ---
struct client_t {
    uv_tcp_t handle;
    llhttp_t parser;
    std::string url;
    std::string body;
};

// --- write request wrapper ---
struct write_req_t {
    uv_write_t req;
    char* buf;
    size_t len;
    client_t* client;
};

// --- forward declarations ---
void on_new_connection(uv_stream_t* server, int status);
void on_client_close(uv_handle_t* handle);
void on_write_complete(uv_write_t* req, int status);

// --- llhttp callbacks ---
int on_url(llhttp_t* parser, const char* at, size_t length) {
    client_t* client = (client_t*) parser->data;
    client->url.append(at, length);
    return 0;
}

int on_body(llhttp_t* parser, const char* at, size_t length) {
    client_t* client = (client_t*) parser->data;
    client->body.append(at, length);
    return 0;
}

int on_message_complete(llhttp_t* parser) {
    client_t* client = (client_t*) parser->data;

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello Mitchell! You requested: " + client->url + "\n";

    // allocate wrapper and copy response to heap
    write_req_t* wr = (write_req_t*) malloc(sizeof(write_req_t));
    if (!wr) {
        uv_close((uv_handle_t*)&client->handle, on_client_close);
        return 0;
    }

    wr->len = response.size();
    wr->buf = (char*) malloc(wr->len);
    if (!wr->buf) {
        free(wr);
        uv_close((uv_handle_t*)&client->handle, on_client_close);
        return 0;
    }
    memcpy(wr->buf, response.data(), wr->len);
    wr->client = client;
    wr->req.data = wr;

    uv_buf_t buf = uv_buf_init(wr->buf, (unsigned int)wr->len);
    int rc = uv_write(&wr->req, (uv_stream_t*)&client->handle, &buf, 1, on_write_complete);
    if (rc) {
        std::cerr << "uv_write failed: " << uv_strerror(rc) << std::endl;
        free(wr->buf);
        free(wr);
        uv_close((uv_handle_t*)&client->handle, on_client_close);
    }

    return 0;
}

// --- llhttp settings (shared) ---
static llhttp_settings_t ll_settings;

void init_llhttp_settings() {
    llhttp_settings_init(&ll_settings);
    ll_settings.on_url = on_url;
    ll_settings.on_body = on_body;
    ll_settings.on_message_complete = on_message_complete;
}

// --- uv helpers ---
void on_client_close(uv_handle_t* handle) {
    client_t* client = (client_t*) handle->data;
    delete client;
}

void on_write_complete(uv_write_t* req, int status) {
    write_req_t* wr = (write_req_t*) req->data;
    if (status) {
        std::cerr << "Write error: " << uv_strerror(status) << std::endl;
    }
    if (wr->buf) free(wr->buf);
    client_t* client = wr->client;
    free(wr);
    uv_close((uv_handle_t*)&client->handle, on_client_close);
}

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = (char*) malloc(suggested_size);
    buf->len = (unsigned int) suggested_size;
}

void echo_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    client_t* client = (client_t*) stream->data;

    if (nread > 0) {
        // feed data to llhttp parser
        llhttp_errno_t err = llhttp_execute(&client->parser, buf->base, (size_t)nread);
        if (err != HPE_OK) {
            std::cerr << "HTTP parse error: " << llhttp_errno_name(err) << std::endl;
            uv_close((uv_handle_t*)&client->handle, on_client_close);
        }
    } else if (nread < 0) {
        // EOF or error
        if (nread != UV_EOF) {
            std::cerr << "Read error: " << uv_err_name((int)nread) << std::endl;
        }
        uv_close((uv_handle_t*)&client->handle, on_client_close);
    }

    if (buf->base) free(buf->base);
}

void on_new_connection(uv_stream_t* server, int status) {
    if (status < 0) {
        std::cerr << "New connection error: " << uv_strerror(status) << std::endl;
        return;
    }

    client_t* client = new client_t();
    uv_tcp_init(server->loop, &client->handle);
    client->handle.data = client;

    if (uv_accept(server, (uv_stream_t*)&client->handle) == 0) {
        // init parser per client
        llhttp_init(&client->parser, HTTP_REQUEST, &ll_settings);
        client->parser.data = client;

        uv_read_start((uv_stream_t*)&client->handle, alloc_buffer, echo_read);
    } else {
        uv_close((uv_handle_t*)&client->handle, on_client_close);
    }
}

int main() {
    init_llhttp_settings();

    // --- libuv TCP server ---
    uv_loop_t* loop = uv_default_loop();

    uv_tcp_t server;
    uv_tcp_init(loop, &server);

    struct sockaddr_in addr;
    int port = 9002; // change here if needed
    uv_ip4_addr("127.0.0.1", port, &addr);

    int r = uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    if (r) {
        std::cerr << "Bind error: " << uv_strerror(r) << std::endl;
        return 1;
    }

    r = uv_listen((uv_stream_t*)&server, 128, on_new_connection);
    if (r) {
        std::cerr << "Listen error: " << uv_strerror(r) << std::endl;
        return 1;
    }

    std::cout << "HTTP server running on port " << port << std::endl;
    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
}
