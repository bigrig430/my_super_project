#include <iostream>
#include <string>
#include <zlib.h>
#include <uv.h>
#include <llhttp.h>

struct client_t {
    uv_tcp_t handle;
    llhttp_t parser;
    std::string url;
    std::string body;
};

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

    uv_write_t* req = (uv_write_t*) malloc(sizeof(uv_write_t));
    uv_buf_t buf = uv_buf_init((char*)response.c_str(), response.size());
    uv_write(req, (uv_stream_t*)&client->handle, &buf, 1, nullptr);

    return 0;
}

// --- libuv alloc + read ---
void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = (char*) malloc(suggested_size);
    buf->len = suggested_size;
}

void echo_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    client_t* client = (client_t*) stream->data;

    if (nread > 0) {
        llhttp_errno_t err = llhttp_execute(&client->parser, buf->base, nread);
        if (err != HPE_OK) {
            std::cerr << "HTTP parse error: " 
                      << llhttp_errno_name(err) << std::endl;
            uv_close((uv_handle_t*) stream, nullptr);
        }
    }

    if (nread < 0) {
        uv_close((uv_handle_t*) stream, nullptr);
    }

    free(buf->base);
}

// --- new connection ---
void on_new_connection(uv_stream_t* server, int status) {
    if (status < 0) {
        std::cerr << "New connection error" << std::endl;
        return;
    }

    client_t* client = new client_t();
    uv_tcp_init(server->loop, &client->handle);
    client->handle.data = client;

    if (uv_accept(server, (uv_stream_t*)&client->handle) == 0) {

        llhttp_settings_t settings;
        llhttp_settings_init(&settings);
        settings.on_url = on_url;
        settings.on_body = on_body;
        settings.on_message_complete = on_message_complete;

        llhttp_init(&client->parser, HTTP_REQUEST, &settings);
        client->parser.data = client;

        uv_read_start((uv_stream_t*)&client->handle, alloc_buffer, echo_read);
    } else {
        uv_close((uv_handle_t*)&client->handle, nullptr);
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
        std::cerr << "Listen error: " << uv_strerror(r) << std::endl;
        return 1;
   }
    std::cout << "HTTP server running on port 9001" << std::endl; 

    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
}
