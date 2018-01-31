
#include <arpa/inet.h>
#include <errno.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <stdlib.h>
#include <string.h>
#include "pwpi.h"

static int pwpi_enabled;
static struct event_base *pwpi_base;
static struct evconnlistener *pwpi_listener;
static PWPIMessageHandler pwpi_message_handler;

static void pwpi_read_cb(struct bufferevent *bev, void *ctx);
static void pwpi_accept_conn_cb(struct evconnlistener *listener,
    evutil_socket_t fd, struct sockaddr *address, int socklen, void *ctx);
static void pwpi_accept_error_cb(struct evconnlistener *listener, void *ctx);

void pwpi_init(int port, PWPIMessageHandler message_handler)
{
    struct sockaddr_in sin;

    pwpi_enabled = 1;
    pwpi_base = event_base_new();
    pwpi_message_handler = message_handler;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    inet_aton("127.0.0.1", &sin.sin_addr);
    sin.sin_port = htons(port);

    pwpi_listener = evconnlistener_new_bind(pwpi_base, pwpi_accept_conn_cb,
        NULL, LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
        (struct sockaddr*)&sin, sizeof(sin));
    if (!pwpi_listener) {
        perror("Couldn't create listener");
        pwpi_disable();
        return;
    }
    evconnlistener_set_error_cb(pwpi_listener, pwpi_accept_error_cb);
}

void pwpi_enable() {
    pwpi_enabled = 1;
}

void pwpi_disable() {
    pwpi_enabled = 0;
}

static void pwpi_read_cb(struct bufferevent *bev, void *ctx)
{
    struct evbuffer *input = bufferevent_get_input(bev);
    struct evbuffer *output = bufferevent_get_output(bev);
    char *request_line;
    size_t len;

    request_line = evbuffer_readln(input, &len, EVBUFFER_EOL_CRLF);
    while (request_line) {
        char return_message[MAX_PWPI_TEXT_LENGTH];
        return_message[0] = '\0';
        pwpi_message_handler(request_line, return_message);
        if (strlen(return_message) > 0) {
            evbuffer_add_printf(output, "%s", return_message);
        }
        free(request_line);

        request_line = evbuffer_readln(input, &len, EVBUFFER_EOL_CRLF);
    }

    // Break the libevent loop to get control back to the main event loop.
    // This stops a long pwpi process with constant requests from freezing out
    // the player.
    event_base_loopbreak(pwpi_base);
}

static void pwpi_event_cb(struct bufferevent *bev, short events, void *ctx)
{
    if (events & BEV_EVENT_ERROR) {
        perror("Error from bufferevent");
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        bufferevent_free(bev);
    }
}

static void pwpi_accept_conn_cb(struct evconnlistener *listener,
    evutil_socket_t fd, struct sockaddr *address, int socklen, void *ctx)
{
    struct bufferevent *bev = bufferevent_socket_new(
        pwpi_base, fd, BEV_OPT_CLOSE_ON_FREE);

    bufferevent_setcb(bev, pwpi_read_cb, NULL, pwpi_event_cb, NULL);

    bufferevent_enable(bev, EV_READ|EV_WRITE);
}

static void pwpi_accept_error_cb(struct evconnlistener *listener, void *ctx)
{
    int err = EVUTIL_SOCKET_ERROR();
    fprintf(stderr, "Got an error %d (%s) on the listener. "
            "Shutting down.\n", err, evutil_socket_error_to_string(err));

    event_base_loopexit(pwpi_base, NULL);
}

void pwpi_handle_events() {
    if (!pwpi_enabled) {
        return;
    }
    event_base_loop(pwpi_base, EVLOOP_NONBLOCK);
}

void pwpi_shutdown()
{
    if (pwpi_listener) {
        evconnlistener_free(pwpi_listener);
    }
    event_base_free(pwpi_base);
}

