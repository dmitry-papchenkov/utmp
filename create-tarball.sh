#!/bin/sh
VERSION=$(egrep '^VERSION' config.mk)
VERSION=`echo ${VERSION#*=}` # ugly way to strip spaces
TARFILE=../utmp-$VERSION.tar.bz2
tar -c -j -f$TARFILE --exclude-vcs --exclude=debian/* .
_rc=$?
if [ $_rc -eq 0 ]; then
	echo "$TARFILE created"
fi

