# utmp version
VERSION = 0.3

# PGP signature for Debian packaging
# Only needed by package maintainers
SIGN=036CADA4

# Customize below to fit your system.

GROUP = utmp

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# includes and libs
INCS = -I. -I/usr/include
LIBS = -L/usr/lib -lc

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\"
CFLAGS += -std=c99 -pedantic -Wall -Os ${INCS} ${CPPFLAGS} -DTMUX_SUPPORT
LDFLAGS += ${LIBS}

# compiler and linker
CC ?= cc

