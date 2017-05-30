#!/bin/sh
ROOT_DIR=$(dirname "$0")
PROJECTS_DIR="${ROOT_DIR}/projects"
PROJECTS=("${PROJECTS_DIR}/ffind" "${PROJECTS_DIR}/print" "${PROJECTS_DIR}/ls-tool" "${PROJECTS_DIR}/chat")
BUILD_DIR="${ROOT_DIR}/build"


mkdir -p $BUILD_DIR

if [[ "${CC}" == "" ]]; then
	CC=gcc
fi


CFLAGS="-std=c11 -pedantic-errors -Wall -Wextra -I ${ROOT_DIR}/projects"
CFLAGS_DEBUG="-O0 -ggdb -fsanitize=address -DDEBUG_"
CFLAGS_RELEASE="-O3 -s"


if [[ $1 == "release" ]]; then
	CFLAGS="${CFLAGS} ${CFLAGS_RELEASE}"
else
	CFLAGS="${CFLAGS} ${CFLAGS_DEBUG}"
fi

for proj in ${PROJECTS[@]}; do
	${proj}/build.sh "${CC}" "${CFLAGS}" "${BUILD_DIR}/$(basename $proj)"
done

