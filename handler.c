#include <stdio.h>
#include <stdlib.h>

#include "backupd.h"

//err < 0 error 
//err > 0 control
//err = 0 succ
void reply(int cmd, int err, char *data, int len, ctx_t *ctx) 
{
    char msg_buf[1024] = {'\0'};

    header_t *hdr = (header_t*)msg_buf;
    hdr->cmd = cmd;
    hdr->len = sizeof(msg_t) + len;
    msg_t *msg = (msg_t*)hdr->data;
    msg->err = err;
    msg->data_len = len;
    memcpy(msg->data, data, len);

    bufferevent_write(ctx->bev, msg_buf, hdr->len+sizeof(header_t));
}

void handle_msg(void *data, uint8_t cmd, ctx_t *ctx)
{
    int err_code = 0;
    switch (cmd) {
    case upload_syn:
        {
            err_code = upload(data, ctx);
            reply(upload_ack, err_code, "", 0, ctx);
            break;
        }
    case download_syn:
        {
            err_code = download(data, ctx);
            if (err_code != Write)
                reply(download_ack, err_code, "", 0, ctx);
            break;
        }
    default:
        {
            zlog_error(zc, "[%s] unknow msg id: %x", ctx->peer, cmd);
            break;
        }
    }
}

//
//recv data from client, then write disk.
int upload(void *data, ctx_t *ctx)
{
    header_t *hdr = (header_t*)data;
    msg_t *msg = (msg_t*)hdr->data;

    if (msg->err < Succ) {
        zlog_error(zc, "[%s] upload_syn request err is not ZERO:%d", ctx->peer, msg->err);
        return -1;
    }

    if (strlen(msg->filename) == 0) {
        zlog_error(zc, "[%s] upload_syn request filename is empty", ctx->peer);
        return -1;
    }

    if (msg->err == Init) {
        snprintf(ctx->backup_file, 256, "%s/%s.gz", backup_path, msg->filename);
        snprintf(ctx->tmp_file, 256, "%s/%s.tmp", backup_path, msg->filename);

        if (access(ctx->tmp_file, F_OK) != 0) {
            if (remove(ctx->tmp_file) == -1)
                return -1;
        }

        ctx->fd = open(ctx->tmp_file, O_CREAT|O_APPEND|O_RDWR, 0666);
        if (ctx->fd == -1) {
            zlog_error(zc, "[%s] upload_syn open file failed:%s", ctx->peer, ctx->tmp_file);
            return -2;
        }
    }

    if (msg->err == Fini) {
        close(ctx->fd);
        ctx->fd = 0;
        rename(ctx->tmp_file, ctx->backup_file);
        zlog_info(zc, "[%s] upload success:%s", ctx->peer, ctx->backup_file);

        return Fini;
    }

    if (msg->err == Write) {
        ssize_t write_len = write(ctx->fd, msg->data, msg->data_len);
        if (write_len < 0) {
            remove(ctx->tmp_file);
            zlog_error(zc, "[%s] upload write file error:%s", ctx->peer, ctx->backup_file);
            return -1;
        }
    }

    return Succ;
}

//
//read backup file from disk, send to client
int download(void *data, ctx_t *ctx)
{
    header_t *hdr = (header_t*)data;
    msg_t *msg = (msg_t*)hdr->data;
    char buf[512*1024] = {'\0'};

    if (strlen(msg->filename) == 0) {
        zlog_error(zc, "[%s] download_syn request filename is empty", ctx->peer);
        return -1;
    }

    if (msg->err < Succ) {
        if (ctx->fd != 0) {
            close(ctx->fd);
            ctx->fd = 0;
        }
        zlog_error(zc, "[%s] download_syn request err is not ZERO:%d", ctx->peer, msg->err);
        return -1;
    }

    if (msg->err == Init) {
        snprintf(ctx->backup_file, 256, "%s/%s.gz", backup_path, msg->filename);

        if (access(ctx->backup_file, F_OK) != 0) {
            zlog_error(zc, "[%s] download_syn can not fount backup file:%s", ctx->peer, ctx->backup_file);
            return -9;
        }

        ctx->fd = open(ctx->backup_file, O_RDONLY);
        if (ctx->fd == -1) {
            zlog_error(zc, "[%s] download_syn open file failed:%s", ctx->peer, ctx->backup_file);
            return -8;
        }
    }

    header_t *download_hdr = (header_t*)buf;
    download_hdr->cmd = download_ack;
    msg_t *download_msg = (msg_t*)download_hdr->data;

    if (msg->err == Read) {
        ssize_t read_len = read(ctx->fd, download_msg->data, 256*1024);
        if (read_len > 0) {
            download_msg->data_len = read_len;
            download_msg->err = Write;

            download_hdr->len = sizeof(msg_t)+read_len;
            bufferevent_write(ctx->bev, buf, sizeof(header_t)+download_hdr->len);

            return Write;
        }
        else if (read_len == 0) {
            if (errno != Succ) {
                close(ctx->fd);
                return -1;
            }

            download_msg->data_len = read_len;
            close(ctx->fd);
            ctx->fd = 0;
            zlog_info(zc, "[%s] download success:%s", ctx->peer, ctx->backup_file);

            return Fini;
        }
        else {
            close(ctx->fd);
            ctx->fd = 0;
            zlog_error(zc, "[%s] download read file error:%s", ctx->peer, ctx->backup_file);
            return -1;
        }
    }

    return Succ;
}
