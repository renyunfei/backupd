#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "backupd.h"

#define CHECK_PORT(port) ((port>1024)&&(port<65535))

zlog_category_t *zc;
char *progname = "backupd";
static int port = 10006;
struct event_base *base;
char *backup_path = "/Users/ren/GitHub/backupd/backup";
server_t *serv;

static int daemonize();
static void init_server();

static int daemonize()  
{
    pid_t pid = fork();  
    if (pid < 0)
        exit(-1);

    if (pid != 0) 
        exit(0);

    if (setsid() == -1)
       exit(-1);

    umask(0);  
    pid = fork();  
    if (pid != 0) 
        exit(0);  

    //chdir(backup_path);  
    for (int i = 0; i < 3; i++)
        close (i);  

    int nullfd = open("/dev/null", O_RDWR);  
    dup2(nullfd, STDOUT_FILENO);  
    dup2(nullfd, STDERR_FILENO);  

    return 0;  
}

static void init_server()
{
    serv = (server_t*)malloc(sizeof(server_t));

    signal(SIGPIPE,SIG_IGN);
    signal(SIGHUP,SIG_IGN);
    
    //init log
    if (zlog_init("zlog.conf")) {
        fprintf(stderr, "init zlog failed\n");
        exit(1);
    }
    zc = zlog_get_category("default");
    if (!zc) {
        fprintf(stderr, "get zlog category failed\n");
        exit(1);
    }
    zlog_info(zc, "init server success...");
}

void usage()
{
    printf("Usage: %s [-p port] [-d]\n"
            "-p server port\n"
            "-d daemon\n", progname);
    exit(-1);
}

int main(int argc, char **argv)
{
    int opt;
    int d = 0;
	struct evconnlistener *listener;
	struct event *signal_event;
	struct sockaddr_in sin;

    while ((opt = getopt(argc, argv, "p:d")) != -1) {
        switch (opt) {
            case 'd':
                d = 1;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            default:
                usage();
        }
    }

    if (d) daemonize();

    if (!CHECK_PORT(port)) {
        fprintf(stderr, "Port must be between 1024 ~ 65535\n");
        usage();
    }

    init_server();

	base = event_base_new();
	if (!base) {
		zlog_error(zc, "Could not initialize libevent");
		return -1;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	listener = evconnlistener_new_bind(base, listener_cb, (void *)base,
	    LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
	    (struct sockaddr*)&sin, sizeof(sin));

	if (!listener) {
		zlog_error(zc, "Could not create a listener");
		return -1;
	}

	signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);
	if (!signal_event || event_add(signal_event, NULL) < 0) {
		zlog_error(zc, "Could not create/add a signal event");
		return -1;
	}

	event_base_dispatch(base);
	evconnlistener_free(listener);
	event_free(signal_event);
	event_base_free(base);

	return 0;
}
