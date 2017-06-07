
PROJDIR=$(dirname "$0")
CC="$1"
CFLAGS="$2"
OUTDIR="$3"


echo "${CC} ${CFLAGS} ${LIBS} ${PROJDIR}/main.c -o ${OUTDIR}"
$CC $CFLAGS $LIBS $PROJDIR/main.c -o $OUTDIR

