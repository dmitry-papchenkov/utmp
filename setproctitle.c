/*
 *  This file contains BSD-like setproctitle() function for linux and
 *  possibly other posix-compatible systems. It's based on avahi and 
 *  util-linux setproctitle wrappers and should work at least on Linux
 *  and *BSD systems.
 *  Clobbers argv of our main procedure so ps(1) will display the title.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "setproctitle.h"

#if !defined(HAVE_BSD_SETPROCTITLE) && defined(__linux__)
static char** argv_buffer = NULL;
static size_t argv_size = 0;
#	if !HAVE_DECL_ENVIRON
extern char **environ;
#	endif /* HAVE_DECL_ENVIRON */
#endif

/* Move argv and environment to another memory location,
 * returns new argv-pointer so we can use it later */
void utmp_initproctitle (int argc, char **argv)
{
#if !defined(HAVE_BSD_SETPROCTITLE) && defined(__linux__)

	unsigned i;
	char **new_environ, *endptr;

	/*
	 * This code is really really ugly. We make some memory layout
	 * assumptions and reuse the environment array as memory to store
	 * our process title in
	 * 
	 * Remember that this code *WILL* trash argv-array so you should
	 * process arguments before you call this function and strdup()
	 * needed strings.
	 */

	for (i = 0; environ[i]; i++);

	endptr = i ? environ[i-1] + strlen(environ[i-1]) : argv[argc-1] + strlen(argv[argc-1]);

	argv_buffer = argv;
	argv_size = endptr - argv_buffer[0];

	/* Make a copy of environ */

	new_environ = malloc(sizeof(char*) * (i + 1));
	for (i = 0; environ[i]; i++)
		new_environ[i] = strdup(environ[i]);
	new_environ[i] = NULL;

	environ = new_environ;
#endif
}

void utmp_setproctitle (const char *fmt, ...)
{
#ifdef HAVE_BSD_SETPROCTITLE
	char b[SETPROCTITLE_MAX_LENGTH];

	va_list ap;
	va_start(ap, fmt);
	va_end(ap);

	setproctitle("-%s", t);
#elif defined(__linux__)
	size_t l;
	va_list ap;
	if (!argv_buffer)
		return;
	
	va_start(ap, fmt);
	vsnprintf(argv_buffer[0], argv_size, fmt, ap);
	va_end(ap);
	
	l = strlen(argv_buffer[0]);

	memset(argv_buffer[0] + l, 0, argv_size - l);
	argv_buffer[1] = NULL;
#endif
}
