#!/bin/sh
VERSION=$(egrep '^VERSION' config.mk)
VERSION=`echo ${VERSION#*=}` # ugly way to strip spaces
DIRVERS=utmp_$VERSION
TARFILE=../$DIRVERS.tar.gz
ORIGTARFILE=../$DIRVERS.orig.tar.gz
tar -c -z -f$TARFILE --exclude-vcs --exclude=debian --transform "s|^./|$DIRVERS/|" .
_rc=$?
if [ $_rc -eq 0 ]; then
	echo "$TARFILE created"
fi
