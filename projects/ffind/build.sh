#!/bin/sh

PROJDIR=$(dirname "$0")
CC="$1"
CFLAGS="$2"
OUTDIR="$3"
CLIBS="-lpthread"

echo "${CC} ${CFLAGS} ${CLIBS} ${PROJDIR}/main.c -o ${OUTDIR}"
$CC $CLIBS $CFLAGS $PROJDIR/main.c -o $OUTDIR
