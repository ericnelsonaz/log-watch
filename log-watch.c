/*
 * Copyright (C) 2017  Nelson Integration, LLC
 */

#include "boyer-moore.h"
#include "inotify-log.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#define NUMBUFS 2
#ifndef LOGBUFSIZE
#define LOGBUFSIZE 256
#endif
#define PID_INVALID -1

struct log_watch_t {
	char const		       *cmdline;
	unsigned 			cur;
	unsigned 			prev;
	ssize_t 			prevlen;
	pid_t				cmd_pid;
	struct boyer_moore_context	bm_ctx;
	struct inotify_log_t		log;
	char			       *readbufs[NUMBUFS];
	char				readbuf[NUMBUFS*LOGBUFSIZE];
};

#define ARRAY_SIZE(__arr) (sizeof(__arr)/sizeof(__arr[0]))
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

static void start_child(struct log_watch_t *logwatch)
{
	pid_t child = fork();
	if (!child) {
		syslog(LOG_USER|LOG_INFO, "matched %s, execute %s\n",
		       logwatch->bm_ctx.needle,
                       logwatch->cmdline);
                int rval = execl(logwatch->cmdline, logwatch->cmdline,
				 (char *)0);
		if (rval)
			syslog(LOG_USER|LOG_INFO, "Error executing %s: %m\n",
			       logwatch->cmdline);
		exit(0);
	} else {
		/* parent process */
		logwatch->cmd_pid = child;
	}
}

static void read_callback(struct inotify_log_t *log)
{
	struct log_watch_t *logwatch = container_of(log, struct log_watch_t,
						    log);

	ssize_t numread;
	while(0 < (numread=read(log->fdlog, logwatch->readbufs[logwatch->cur],
				LOGBUFSIZE))) {
		unsigned pos = 0;
		logwatch->readbufs[logwatch->cur][numread] = 0;
		while ((pos < numread) &&
		       boyer_moore_search
		       (&logwatch->bm_ctx,
			logwatch->readbufs[logwatch->cur]+pos,
			numread-pos,
			logwatch->readbufs[logwatch->prev],
			logwatch->prevlen, &pos)) {
			if (PID_INVALID == logwatch->cmd_pid ) {
				start_child(logwatch);
			} else {
				syslog(LOG_USER|LOG_INFO,
				       "cmd %s is still running\n",
				       logwatch->cmdline);
			}
			pos++;
		}
		logwatch->prevlen = numread;
		logwatch->prev = logwatch->cur;
		logwatch->cur ^= 1;
	}
}

static int volatile do_exit = 0;
static void ctrlc_handler(int signo)
{
	do_exit = 1;
}

static struct log_watch_t *watch = 0;
static void sigchild(int sig)
{
	if(SIGCHLD == sig) {
		int exit_code;
		struct rusage usage;

		int pid;
		while (0 < ( pid = wait3(&exit_code, WNOHANG, &usage))) {
			if (watch && watch->cmd_pid == pid) {
				watch->cmd_pid = PID_INVALID;
			}
		}
	}
}

int main(int argc, char const * const argv[])
{
	if (4 > argc) {
		fprintf(stderr,
			"Usage: %s pattern command /path/to/logfile\n",
			argv[0]);
		return -1;
	}

	struct log_watch_t logwatch;
	logwatch.cur = 0;
	logwatch.prev = 1;
	logwatch.cmdline = argv[2];
	logwatch.cmd_pid = PID_INVALID;
	logwatch.readbufs[0] = logwatch.readbuf;
	logwatch.readbufs[1] = logwatch.readbuf+LOGBUFSIZE;

	if (inotify_log_init(argv[3],
			     &logwatch.log,
			     &logwatch.log,
			     read_callback)){
		fprintf(stderr, "Error creating log watcher\n");
		return -1;
	}

	boyer_moore_init(argv[1], &logwatch.bm_ctx);

	struct pollfd fds[1];
	fds[0].fd = logwatch.log.inot;
	fds[0].events = POLLIN;

	signal(SIGINT, ctrlc_handler);
	watch = &logwatch;
	signal(SIGCHLD, sigchild);

	while (!do_exit) {
		int numready = poll(fds, ARRAY_SIZE(fds), -1);
		if (0 < numready) {
			if (fds[0].revents & POLLIN) {
				inotify_log_process(&logwatch.log);
			}
		}
	}

	inotify_log_cleanup(&logwatch.log);
	return 0;
}
