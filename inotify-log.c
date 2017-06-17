/*
 * Copyright (C) 2017  Nelson Integration, LLC
 */

#include "inotify-log.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define ARRAY_SIZE(__arr) (sizeof(__arr)/sizeof(__arr[0]))

#ifdef INOTIFY_LOG_TEST
#define DPRINT(...) do{ fprintf( stderr, __VA_ARGS__ ); } while(0)
#else
#define DPRINT(...) do{ } while (0)
#endif

static void inotify_reopen(struct inotify_log_t *watcher)
{
	if (0 <= watcher->lognot) {
		inotify_rm_watch(watcher->inot, watcher->lognot);
		watcher->lognot = -1;
	}

	if (0 <= watcher->fdlog)
		close(watcher->fdlog);

	watcher->fdlog = open(watcher->logpath, O_RDONLY);
	if (0 <= watcher->fdlog) {
		fcntl(watcher->fdlog, O_NONBLOCK);
		watcher->state = INLOG_DIR_AND_FILE;
		watcher->lognot = inotify_add_watch(watcher->inot, watcher->logpath,
						   IN_MODIFY | IN_CLOSE_WRITE);
		if (0 > watcher->lognot) {
			perror("inotify_add_watch(file)");
		}
	} else {
		watcher->state = INLOG_DIR_NOFILE;
	}
}

static void inotify_log_create
	(struct inotify_log_t *watcher,
	 struct inotify_event *event)
{
	if ((event->len >= watcher->fnlen) &&
		!strncmp(event->name, watcher->filename, event->len)) {
		inotify_reopen(watcher);
	}
}

static void inotify_log_modify
	(struct inotify_log_t *watcher,
	 struct inotify_event *event)
{
	if (watcher->read_callback)
		watcher->read_callback(watcher);
}

void inotify_log_process(struct inotify_log_t *watcher)
{
	char buf[4096] __attribute__ ((aligned(8)));
	ssize_t numRead;
	while (0 < (numRead = read(watcher->inot, buf, sizeof(buf)))) {
		char *p;
		for (p = buf; p < buf + numRead; ) {
			struct inotify_event *event = (struct inotify_event *)p;
			DPRINT("event 0x%x\n", event->mask);
			if (event->mask & IN_CREATE)        inotify_log_create(watcher, event);
			if (event->mask & IN_MODIFY)        inotify_log_modify(watcher, event);
			p += sizeof(struct inotify_event) + event->len;
		 }
	}
}

int inotify_log_init(char const *logpath,	/* e.g. /var/log/syslog */
		     void 	*opaque, 	/* for application use */
		     struct inotify_log_t *watcher,
		     void (*read_callback)(struct inotify_log_t *watcher))
{
	memset(watcher, 0, sizeof(*watcher));

	watcher->dirname = strdup(logpath);
	watcher->inot = -1;
	watcher->lognot = -1;
	watcher->fdlog = -1;

	char *slash = strrchr(watcher->dirname, '/');
	if (!slash) {
		fprintf(stderr, "Invalid path %s (must contain directory)\n",
			logpath);
		return -EINVAL;
	} else {
		*slash = '\0';
	}

        struct stat st;
	if (0 != stat(watcher->dirname, &st)) {
                fprintf(stderr, "directory %s not accessible: %m\n",
			watcher->dirname);
		return -ENOENT;
	} else if (0 == (st.st_mode & S_IFDIR)) {
                fprintf(stderr, "%s is not a directory\n",
			watcher->dirname);
		return -ENOENT;
	}

	watcher->filename = strdup(slash+1);
	watcher->fnlen = strlen(watcher->filename);
	watcher->logpath = strdup(logpath);

	watcher->inot =  inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
	if (0 > watcher->inot) {
		perror("inotify");
		return -1;
	}
        
	watcher->dirnot = inotify_add_watch(watcher->inot, watcher->dirname,
					    IN_CREATE | IN_DELETE);
	if (0 > watcher->dirnot) {
		perror("inotify_add_watch(dir)");
	}

	if (0 == stat(watcher->logpath, &st)) {
		inotify_reopen(watcher);
		/*
		 * start from end on startup
		 */
		if (watcher->state == INLOG_DIR_AND_FILE)
			lseek(watcher->fdlog, 0, SEEK_END);
	}

	watcher->read_callback = read_callback;

	return 0;
}

void inotify_log_cleanup(struct inotify_log_t *watcher)
{
	if (0 <= watcher->lognot) {
		inotify_rm_watch(watcher->inot, watcher->lognot);
		watcher->lognot = -1;
	}
	if (0 <= watcher->inot)
		close(watcher->inot);
	if (watcher->logpath)
		free(watcher->logpath);
	if (watcher->dirname)
		free(watcher->dirname);
}

#ifdef INOTIFY_LOG_TEST
#include <poll.h>
#include <signal.h>

static int volatile do_exit = 0;

static void ctrlc_handler(int signo)
{
	printf("<ctrl-c>\r\n");
	do_exit = 1;
}

static void read_callback(struct inotify_log_t *watcher)
{
	printf("%s:\n", __func__);
	char inbuf[2048];
	ssize_t numread;
	while (0 < (numread = read(watcher->fdlog, inbuf, sizeof(inbuf)))) {
		printf("read %ld:", numread);
		fwrite(inbuf, 1, numread, stdout);
		printf("\n");
		fflush(stdout);
	}
}

int main(int argc, char const * const argv[])
{
	if (2 > argc) {
		fprintf(stderr, "Usage: %s /path/to/logfile\n", argv[0]);
		return -1;
	}
        struct inotify_log_t watcher;
	if (inotify_log_init(argv[1],
			     &watcher,
			     &watcher,
			     read_callback)){
		fprintf(stderr, "Error creating log watcher\n");
		return -1;
	}

	struct pollfd fds[1];
	fds[0].fd = watcher.inot;
	fds[0].events = POLLIN;

	signal(SIGINT, ctrlc_handler);

	while (!do_exit) {
		int numready = poll(fds, ARRAY_SIZE(fds), -1);
		if (numready) {
			if (fds[0].revents & POLLIN) {
				inotify_log_process(&watcher);
			}
			else
				fprintf(stderr, "Spurious wake 0x%x\n",
					fds[0].revents);
		}
		else
			fprintf(stderr, "idle\n");
	}

	inotify_log_cleanup(&watcher);
	return 0;
}

#endif

