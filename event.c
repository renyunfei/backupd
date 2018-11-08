#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <time.h>
#include "zlog.h"

#include "backupd.h"

//
//time event callback
void time_cb(int fd,short _event,void *params)
{
    //TODO
}

//
//read|write event callback 
void conn_readcb(struct bufferevent *bev, void *ctx)
{
    char *msg_buff = malloc(1*1024*1024);
    
    header_t hdr;
    struct evbuffer *buffer = bufferevent_get_input(bev);

    for (;;) {
        size_t len = evbuffer_get_length(buffer);
        if (len < sizeof(hdr))
            break;

        evbuffer_copyout(buffer, &hdr, sizeof(hdr));
        if (len < (hdr.len+sizeof(hdr)))
            break;
        
        evbuffer_remove(buffer, msg_buff, sizeof(hdr)+hdr.len);
        handle_msg(msg_buff, hdr.cmd, ctx);
    }

    free(msg_buff);
}

//
//listen fd event, accept connect
void listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *sa, int socklen, void *user_data)
{
	struct event_base *base = user_data;

    ctx_t *ctx = (ctx_t*)malloc(sizeof(ctx_t));
    memset(ctx, 0, sizeof(ctx_t));
    ctx->fd = 0;
    snprintf(ctx->peer, 64, "%s", inet_ntoa(((struct sockaddr_in*)sa)->sin_addr));

    zlog_info(zc, "Get connect from:%s", ctx->peer);

    ctx->bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!ctx->bev) {
        zlog_error(zc, "[%s] Error constructing bufferevent", ctx->peer);
        event_base_loopbreak(base);
        return;
    }

    bufferevent_setcb(ctx->bev, conn_readcb, NULL, conn_eventcb, (void*)ctx);
    bufferevent_enable(ctx->bev, EV_READ|EV_WRITE);

    serv->nclients++;
}

//
//event callback
void conn_eventcb(struct bufferevent *bev, short events, void *arg)
{
    ctx_t *ctx = (ctx_t*)arg;
    zlog_error(zc, "[%s] event:%x\n", ctx->peer, events);

	if (events & BEV_EVENT_EOF) {
		zlog_error(zc, "[%s] Connection closed", ctx->peer);
	} else if (events & BEV_EVENT_ERROR) {
		zlog_error(zc, "[%s] Got an error on the connection: %s", ctx->peer, 
		    strerror(errno));
	}

    if (events & BEV_EVENT_TIMEOUT) {
        zlog_error(zc, "[%s] Timeout", ctx->peer);
    }

	bufferevent_free(bev);
    free(ctx);

    serv->nclients--;
}

//
//signal callback
void signal_cb(evutil_socket_t sig, short events, void *user_data)
{
	struct event_base *base = user_data;
	struct timeval delay = {1,0};

	zlog_error(zc, "Caught an interrupt signal; exiting cleanly in two seconds.\n");

	event_base_loopexit(base, &delay);
}
