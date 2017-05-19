#!/bin/bash
ROOT_DIR=$(dirname "$0")
PROJECTS="${ROOT_DIR}/projects/*"
BUILD_DIR="${ROOT_DIR}/build"

mkdir -p $BUILD_DIR

if [[ "${CC}" == "" ]]; then
	CC=gcc
fi


CFLAGS="-std=c11 -Wall -Wextra"
CFLAGS_DEBUG="-O0 -ggdb"
CFLAGS_RELEASE="-O3 -s"

if [[ $1 == "release" ]]; then
	CFLAGS="${CFLAGS} ${CFLAGS_RELEASE}"
else
	CFLAGS="${CFLAGS} ${CFLAGS_DEBUG}"
fi

for proj in ${PROJECTS}; do
	echo "${CC} ${CFLAGS} ${proj}/*.c -o ${BUILD_DIR}/${proj}"
	$CC $CFLAGS $proj/*.c -o $BUILD_DIR/$(basename $proj)
done
