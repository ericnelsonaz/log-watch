#ifndef __INOTIFY_LOG_H__
#define __INOTIFY_LOG_H__

/*
 * monitor for additions to, and renames of a log file
 * (e.g. syslog).
 *
 * Usage is generally to call inotify_log_init with the
 * name of a log file to watch and a callback for processing
 * the log file and use the 'inot' file descriptor in
 * a poll() or select() loop with POLLIN. When events 
 * are signalled, call the inotify_log_process() routine
 * to process inotify events.
 *
 * The callback should read and process data from the 'fdlog'
 * file handle.
 *
 * At startup, only new content added to the log file will
 * be available (i.e. file is positioned at end of file).
 *
 * When done, the inotify_log_cleanup routine should be used
 * to free resources.
 *
 *
 * Copyright (C) 2017  Nelson Integration, LLC
 */

enum inotify_log_state_e {
	INLOG_DIR_NOFILE,
	INLOG_DIR_AND_FILE
};

struct inotify_log_t {
	char *dirname;
	char *filename;
	unsigned fnlen;
	char *logpath;
        enum inotify_log_state_e state;
	int inot;
	int dirnot;
	int lognot;
	int fdlog;
	void *opaque; /* use for your own purposes */
        void (*read_callback)(struct inotify_log_t *watcher);
};

/* returns 0 if successful */
int inotify_log_init(char const *logpath,	/* e.g. /var/log/syslog */
		     void 	*opaque, 	/* for application use */
		     struct inotify_log_t *watcher,
		     void (*read_callback)(struct inotify_log_t *watcher));

/* call when POLLIN is signalled on watcher->inot */
void inotify_log_process(struct inotify_log_t *watcher);

/* call when done to free resources */
void inotify_log_cleanup(struct inotify_log_t *watcher);

#endif
