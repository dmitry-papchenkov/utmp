# utmp - simple login
# See LICENSE file for copyright and license details.

include config.mk

SRC = utmp.c setproctitle.c
OBJ = ${SRC:.c=.o}

all: options utmp 

options:
	@echo utmp build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

utmp: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f utmp ${OBJ} utmp-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	$(CURDIR)/create-tarball.sh

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f utmp ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/utmp
	@chgrp ${GROUP} ${DESTDIR}${PREFIX}/bin/utmp
	@chmod g+s ${DESTDIR}${PREFIX}/bin/utmp
	@echo installing manual page to ${DESTDIR}${PREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < utmp.1 > ${DESTDIR}${MANPREFIX}/man1/utmp.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/utmp.1

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/utmp
	@echo removing manual page from ${DESTDIR}${PREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/utmp.1

deb-src: clean
	@echo Creating debian source package...
	@echo Creating .orig tarball...
	$(CURDIR)/create-orig-tarball.sh
	debuild -k$(SIGN) -S

deb-pkg: clean
	@echo Creating debian package...
	@echo Creating .orig tarball...
	$(CURDIR)/create-orig-tarball.sh
	debuild -k$(SIGN) -b
		
.PHONY: all options clean dist install uninstall deb-src deb-pkg
