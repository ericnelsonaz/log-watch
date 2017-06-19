all: boyer-moore inotify-log log-watch

boyer-moore: boyer-moore.c boyer-moore.h
	${CC} -o boyer-moore boyer-moore.c -ggdb -fprofile-arcs -ftest-coverage -Wall -DBOYER_MOORE_TEST=1

inotify-log: inotify-log.c inotify-log.h
	${CC} -o inotify-log inotify-log.c -ggdb -fprofile-arcs -ftest-coverage -Wall -DINOTIFY_LOG_TEST=1

log-watch: log-watch.c boyer-moore.c boyer-moore.h inotify-log.c inotify-log.h
	${CC} ${CFLAGS} -o log-watch log-watch.c boyer-moore.c inotify-log.c -O3 -Wall

.PHONY: clean install

clean:
	rm -f boyer-moore inotify-log log-watch *.o *.gcda *.gcno *.gcov

install: all
	mkdir -p ${DESTDIR}/usr/bin
	cp -fv boyer-moore ${DESTDIR}/usr/bin
