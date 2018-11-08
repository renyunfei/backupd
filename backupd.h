#ifndef BACKUPD_H
#define BACKUPD_H

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

enum {
    upload_syn = 0x15,
    upload_ack,
    download_syn,
    download_ack,
};

enum {
    Succ = 0x0,
    Init,
    Read,
    Write,
    Fini,
};

#pragma pack(1)
typedef struct {
    uint8_t cmd;
    uint32_t len;
    char data[];
} header_t;

typedef struct {
    int err;
    int id;
    char filename[128];
    int data_len;
    char md5[16];
    char data[];
} msg_t;

//char *backup_path = "/home/ren/syncDoc/backup";
typedef struct {
    int fd;
    char peer[64];
    char tmp_file[256];
    char backup_file[256];
    struct bufferevent *bev;
} ctx_t;

//record server status
typedef struct {
    char *backup_path;
    int nclients;
    int status;
    struct event_base *base;
} server_t;

extern struct event_base *base; 
extern zlog_category_t *zc;
extern char *backup_path;
extern server_t *serv;

//---------------event.c----------------------
void time_cb(int fd,short _event,void *params);
void listener_cb(struct evconnlistener *, evutil_socket_t,
    struct sockaddr *, int socklen, void *);
void conn_writecb(struct bufferevent *, void *);
void conn_eventcb(struct bufferevent *, short, void *);
void signal_cb(evutil_socket_t, short, void *);
void conn_readcb(struct bufferevent *, void *);

//--------------handler.c--------------------
void reply(int cmd, int err, char *data, int len, ctx_t *ctx);
void handle_msg(void *data, uint8_t cmd, ctx_t *ctx);
int upload(void *data, ctx_t *ctx);
int download(void *data, ctx_t *ctx);

#endif
