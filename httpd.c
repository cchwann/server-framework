#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define THREAD_COUNT 1

#include "protocol-server.h"
#include "http_parser.h"

struct http_user_data
{
    server_pt srv;
    int fd;
};

static http_parser_settings http_parser_cbs;

static int http_message_complete_cb(http_parser *parser)
{
    static char reply_with_keep_alive[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 12\r\n"
        "Connection: keep-alive\r\n"
        "Keep-Alive: timeout=2\r\n"
        "\r\n"
        "Hello World!";

    static char reply_with_close[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 12\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello World!";

    int do_keep_alive = 0;
    if (1 == parser->http_major) {
        if (1 == parser->http_minor || 0 == parser->http_minor) {
            do_keep_alive = http_should_keep_alive(parser);
        } else {
            assert(0 && "Unsupport version");
            return -1;
        }
    } else {
        assert(0 && "Unsupport version");
        return -1;
    }

    struct http_user_data *ud = (struct http_user_data *) parser->data;
    if (do_keep_alive) {
        Server.write(ud->srv, ud->fd, reply_with_keep_alive, sizeof(reply_with_keep_alive));
    } else {
        Server.write(ud->srv, ud->fd, reply_with_close, sizeof(reply_with_close));
        Server.close(ud->srv, ud->fd);
    }

    return 0;
}

static void http_on_open(server_pt srv, int fd)
{
    http_parser *parser = Server.get_udata(srv, fd);
    assert(!parser);

    parser = malloc(sizeof(http_parser));
    assert(parser && "OUT-OF-MEMORY!");

    struct http_user_data *ud = malloc(sizeof(struct http_user_data));
    assert(ud && "OUT-OF-MEMORY!");

    http_parser_init(parser, HTTP_REQUEST);
    ud->srv = srv;
    ud->fd = fd;
    parser->data = (void *) ud;

    Server.set_udata(srv, fd, parser);
}

static void http_on_close(server_pt srv, int fd)
{
    http_parser *parser = Server.get_udata(srv, fd);
    if (parser) {
        if (parser->data) {
            free(parser->data);
            parser->data = NULL;
        }
        free(parser);
        Server.set_udata(srv, fd, NULL);
    }
}

static void http_on_data(server_pt srv, int fd)
{
    char buff[1024];
    http_parser *parser = Server.get_udata(srv, fd);
    assert(parser);

    ssize_t nread = Server.read(srv, fd, buff, 1024);
    if (nread == -1) {
        assert(0 && "Unable to read!");
        Server.close(srv, fd);
    } else if (nread) {
        ssize_t nparsed = http_parser_execute(parser, &http_parser_cbs, buff, nread);
        if (nread != nparsed) {
            assert(0 && "How come? Unable to parse http stuff");
            Server.close(srv, fd);
        }
    }
}

void print_conn(server_pt srv, int fd, void *arg)
{
    printf("- Connection to FD: %d\n", fd);
}

void done_printing(server_pt srv, int fd, void *arg)
{
    fprintf(stderr, "# Total Clients: %lu\n", Server.count(srv, NULL));
}

void timer_task(server_pt srv)
{
    size_t count = Server.each(srv, 0,
                               NULL, print_conn,
                               NULL, done_printing);
    fprintf(stderr, "Clients: %lu\n", count);
}

void on_init(server_pt srv)
{
    Server.run_every(srv, 1000, -1, (void *) timer_task, srv);
}

int main(int argc, char *argv[])
{
    memset(&http_parser_cbs, 0, sizeof(http_parser_cbs));
    http_parser_cbs.on_message_complete = http_message_complete_cb;

    struct Protocol http_protocol = { .on_open = http_on_open,
                                      .on_close = http_on_close,
                                      .on_shutdown = http_on_close,
                                      .on_data = http_on_data };
    start_server(.protocol = &http_protocol,
                 .timeout = 2,
                 .on_init = on_init,
                 .threads = THREAD_COUNT);
    return 0;
}
