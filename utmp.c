/* See LICENSE for license details. */
#define _POSIX_C_SOURCE	200112L
#define _BSD_SOURCE /* getusershell() */

#ifdef TMUX_SUPPORT
#	define TMUX_SESSION "tmux display-message -p '#S'"
#	define TMUX_TERM_PREFIX "screen"
#endif /* TMUX_SUPPORT */

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <unistd.h>
#include <utmp.h>
#include <utmpx.h>
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>

#include <libgen.h>

#include "setproctitle.h"

static struct utmpx utmp;
static struct passwd *pass;
static gid_t egid, gid;
static char *me = 0;


void
die(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	exit(EXIT_FAILURE);
}

/*
 * From utmp(5)
 * xterm and other terminal emulators directly create a USER_PROCESS
 * record and generate the ut_id  by using the string that suffix part of
 * the terminal name (the characters  following  /dev/[pt]ty). If they find
 * a DEAD_PROCESS for this ID, they recycle it, otherwise they create a new
 * entry.  If they can, they will mark it as DEAD_PROCESS on exiting and it
 * is advised that they null ut_line, ut_time, ut_user, and ut_host as well.
 */

struct utmpx *
findutmp(int type)
{
	struct utmpx *r;

	utmp.ut_type = type;
	setutxent();
	for(;;) {
	       /*
		* we can not use getutxline because we can search in
		* DEAD_PROCESS to
		*/
	       if(!(r = getutxid(&utmp)))
		       break;
	       if(!strcmp(r->ut_line, utmp.ut_line))
		       break;
	       memset(r, 0, sizeof(*r)); /* for Solaris, IRIX64 and HPUX */
	}
	return r;
}

void
addutmp(int fd, pid_t pid, char *host)
{
	unsigned ptyid;
	char *pts, *cp, buf[5] = {'x'};

	if ((pts = ttyname(fd)) == NULL)
		die("%s: error getting pty name\n", me);

	for (cp = pts + strlen(pts) - 1; isdigit(*cp); --cp)
		/* nothing */;

	ptyid = atoi(++cp);
	if (ptyid > 999 || strlen(pts + 5) > sizeof(utmp.ut_line))
		die("%s: incorrect pts name %s\n", me, pts);
	sprintf(buf + 1, "%03d", ptyid);
	strncpy(utmp.ut_id, buf, 4);

	/* remove /dev/ part of the string */
	strcpy(utmp.ut_line, pts + 5);

	if(!findutmp(DEAD_PROCESS))
		findutmp(USER_PROCESS);

	utmp.ut_type = USER_PROCESS;
	strcpy(utmp.ut_user, pass->pw_name);
	utmp.ut_pid = pid;
	utmp.ut_tv.tv_sec = time(NULL);
	utmp.ut_tv.tv_usec = 0;
	if (host != NULL) {
		strncpy(utmp.ut_host, host, sizeof(utmp.ut_host));
	}
	setgid(egid);
	if(!pututxline(&utmp))
		perror("add utmp entry");
	setgid(gid);
	endutxent();
}

void
delutmp(void)
{
	struct utmpx *r;

	setutxent();
	if((r = getutxline(&utmp)) != NULL) {
		r->ut_type = DEAD_PROCESS;
		r->ut_tv.tv_usec = r->ut_tv.tv_sec = 0;
		setgid(egid);
		pututxline(r);
		setgid(gid);
	}
	endutxent();
}

#ifdef TMUX_SUPPORT
char
*get_tmux_session(void)
{
	char *b, *c;
	FILE *tmux;

	if (getenv("TMUX") == NULL ) /* TMUX variable not preset */
		return NULL;

	b = getenv("TERM");

	if (b == NULL) /* TERM is not preset */
		return NULL;
	
	if (strncmp((char *)TMUX_TERM_PREFIX, /* TERM != screen* */
				b, strlen((char *)TMUX_TERM_PREFIX)) != 0)
		return NULL;
	
	if((tmux = popen(TMUX_SESSION, "r")) /* popen() failed */
			== NULL) {
		die("%s: popen() failed: %s\n", me, strerror(errno));
		return NULL;
	}

	b = malloc(UT_HOSTSIZE-5);
	
	if (b == NULL) /* cannot allocate memory?! 8() */
		die("%s: could not create buffer: %s", me, strerror(errno));

	fgets(b, UT_HOSTSIZE-5, tmux); /* read tmux output */

	if(pclose(tmux) != 0) { /* tmux exited abnormaly */
		free(b);
		fprintf(stderr, "%s: pclose() failed: %s\n", me, strerror(errno));
		return NULL;
	}

	c = strchr(b,'\n'); /* find first newline if any */
	
	if(c != NULL) {
		*c=0;
	} else { /* Something's wrong. EOF, abnormal or truncated line */
		free(b);
		return NULL;
	}
	
	c = malloc(UT_HOSTSIZE); /* new buffer for sprintf */
	
	if (c == NULL) { /* cannot allocate memory?! 8() */
		free(b);
		die("%s: could not create buffer: %s", me, strerror(errno));
	}
	
	snprintf(c, UT_HOSTSIZE, "tmux:%s", b);
	
	free(b);
	return c;
}
#endif /* TMUX_SUPPORT */

int
main(int argc, char *argv[])
{
	int i;
	unsigned login = 0;
	uid_t uid;
	char *from = NULL, *b, *c, *d, *cmd = NULL, *shell = NULL;

	egid = getegid();
	gid = getgid();
	setgid(gid);

	b = argv[0];

	/* login shell has '-' as first argv-character */
	if (b[0] == '-') {
		b++; /* move pointer after '-' */
		login = 1; /* we're login shell */
	}

	/* /usr/bin/utmp -> utmp */
	c = basename(b);

	/* is program name is 'u..'? */
	if (!strlen(c) || c[0] !='u')
		die("%s: invalid argv[0]\n", argv[0]);

	/* duplicate string becouse we'll trash argv by setproctitle later */
	me = strdup(c);

	/* cannot allocate memory?! 8() */
	if (me == NULL)
		die("%s: could not duplicate argv[0]: %s\n",
				argv[0], strerror(errno));

	/* called as 'utmp' or as 'ush' (for example)? */

	i = strlen(me);
	/* argv[0] is longer than 1 and is not 'utmp' */
	if (i < 2 || (strncmp(me, (char *)"utmp", i)!=0))
		/* we're the shell, so do not try to parse options */
		if((shell = strdup(me+1))==NULL) /* ush -> sh */
			die("%s: could not duplicate argv[0]: %s\n",
					me, strerror(errno)); /* cannot allocate memory?! 8() */

	if (shell == NULL) { /* we're not a shell itself so parse the options */
		while ((i = getopt(argc, argv, "lc:f:h?")) != -1) {
			switch(i) {
			case 'c': /* -c ... */
				if((cmd = strdup(optarg)) == NULL)
					die("%s: could not duplicate argv\n", me); /* cannot allocate memory?! 8() */
				break;
			case 'l': /* -l */
				login = 2;
				break;
			case 'f': /* -f ... */
				if((from = strdup(optarg)) == NULL)
					die("%s: could not duplicate argv\n", me); /* cannot allocate memory?! 8() */
				break;
			default: /* also -h and -? */
				fprintf(stderr, "Usage: %s [-c cmd] [-f FROM]\n", me);
				fprintf(stderr, "-f will set utmp(5) host field to FROM\n");
				fprintf(stderr, "-c will call shell with '-c cmd'\n");
				fprintf(stderr, "-l will spawn login shell\n");
				fprintf(stderr, "if program is called as uSHELL, it will update utmp\n");
				fprintf(stderr, "and spawn SHELL. All arguments will be passed to shell.\n");
				fprintf(stderr, "If program is called as 'utmp', it will just add utmp-record\n");
				fprintf(stderr, "end exit.\n");
				fprintf(stderr, "Program respects '-' for login-shell spawning.\n");
				exit(EXIT_FAILURE);
				break; /* will never be reached */
			}
		}
	}
	
	pass = getpwuid(uid = getuid());
	
	if(!pass || !pass->pw_name ||
			strlen(pass->pw_name) + 1 > sizeof(utmp.ut_user)) {
		die("%s: Process is running with an incorrect uid %d\n", me, uid);
	}

#ifdef TMUX_SUPPORT
	if (from == NULL) /* no -f */
		from = get_tmux_session();
#endif /* TMUX_SUPPORT */

	if (shell == NULL && cmd == NULL && !login) { /* we're not a shell nor login-shell and no -c */
		addutmp(STDIN_FILENO, getppid(), from); /* add utmp-record */
		free(me); /* always used so we can free() it safely */
		if (from!=NULL)
			free(from); /* only if used */
		exit(EXIT_SUCCESS); /* we're done, bye-bye */
	}

	/* called without shell in program name, but as login-shell or with -l/-c 
	 * so we sould check the environment and set the real shell for later use */
	if ((shell == NULL) && (login || (cmd != NULL))) {
		b = getenv("SHELL"); /* check SHELL environment */
		c = basename(b); /* /bin/sh -> sh */
		if (c==NULL || ((int)strncmp(me, c, strlen(me)) == 0)) /* There is no SHELL-variable 
																					 or it's points to me */
			b = pass->pw_shell; /* get shell from /etc/passwd */
		if ((shell = strdup(b)) == NULL) /* duplicate shell becouse we can trash environment 
														by setproctitle */
			die("%s: could not duplicate shell\n", me); /* cannot allocate memory?! 8() */
	}
	/* If we reached here we'll try to spawn a shell
	 * let's set some environment needed by shell
	 * we'll override existing variables only if
	 * login shell should be spawned */
	setenv("LOGNAME", pass->pw_name, login);
	setenv("USER", pass->pw_name, login);
	setenv("HOME", pass->pw_dir, login);

	/* Now let's select correct shell with full path from /etc/shells */
	while((b = getusershell()) != NULL) {
		c = basename(b); /* /bin/sh -> sh */
		d = basename(shell); /* /bin/sh -> sh */
		if ((int)strncmp(d, c, strlen(d)) == 0) { /* shell found */
			free(shell); /* clean up old data */
			if((shell = strdup(b))==NULL)
				die("%s: could not duplicate shell\n", me); /* cannot allocate memory?! 8() */
		}
	}
	endusershell();
	if (strchr(shell, '/') == NULL) /* shell is not full-path so not found */
		die("%s: shell %s not found in /etc/shells\n", me, shell);

	setenv("SHELL", shell, 1); /* set the SHELL environment variable correctly */

	/* change the title before fork() so utmp-record will
	 * most likely show the shell, not us.
	 * BEWARE: this will trash argv on linux */
	utmp_initproctitle(argc, argv);
	if (cmd == NULL)
		utmp_setproctitle("[%s] (%s)", shell, me);
	else
		utmp_setproctitle("[%s -c %s] (%s)", shell, cmd, me);

	/* now we can fork and spawn a shell */
	switch (fork()) {
		case 0:
			if (login) { /* we're requested to spawn login-shell or IS the login-shell */
				b = basename(shell); /* take shell basename */
				c = malloc(strlen(b)+1); /* prepare room for '-' */
				snprintf(c, sizeof(c), "-%s", b); /* now c is '-sh' if b is 'sh' */
			}

			if (login == 1 || (cmd == NULL)) { /* we're IS the login-shell or we're IS the shell */
				if (login) {
					argv[0] = c; /* prepare arguments for shell */
				} else {
					argv[0] = shell; /* non-login shell takes just it's path as argv[0] */
				}
				execv(shell,argv); /* spawn a shell */
			} else if (login == 2) { /* we're not THE shell but requested to spawn login-shell by cmdline */
				if (cmd == NULL) /* no -c, just spawn the login-shell */
					execl(shell, c, 0);
				else /* spawn login-shell with -c cmd */
					execl(shell, c, "-c ", cmd, 0);
			} else if (cmd != NULL) { /* non-login with -c */
				execl(shell, shell, "-c ", cmd, 0);
			}
			die("%s: error executing shell: %s\n", me, strerror(errno));
		case -1:
			die("%s: error spawning child: %s\n", me, strerror(errno));
		default:
			addutmp(STDIN_FILENO, getpid(), from);
			if (from != NULL) /* free() if used */
				free(from);
			if (shell != NULL) /* free() if used */
				free(shell);
			/* wait for childrens */
			if (wait(&i) == -1) {
				die("%s: error waiting child:%s\n",
						me, strerror(errno));
			}
			delutmp();
			free(me); /* always set */
			return 0;
	}
}
