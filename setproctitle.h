#ifndef HAVE_SETPROCTITLE_H
#define HAVE_SETPROCTITLE_H

#ifdef HAVE_BSD_SETPROCTITLE
#	define SETPROCTITLE_MAX_LENGTH 256
#endif

void utmp_initproctitle(int argc, char **argv);
void utmp_setproctitle(const char *fmt, ...);

#endif
