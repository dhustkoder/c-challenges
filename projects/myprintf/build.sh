
PROJDIR=$(dirname "$0")
CC="$1"
CFLAGS="$2"
OUTDIR="$3"

echo "${CC} ${CFLAGS} ${PROJDIR}/main.c -o ${OUTDIR}"
$CC $CFLAGS $PROJDIR/main.c -o $OUTDIR

