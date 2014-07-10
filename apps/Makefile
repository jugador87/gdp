ALL=	writer-test reader-test gdp-rest

INCROOT=..
LIBROOT=	${INCROOT}
INCGDP=		-I${INCROOT}
LIBGDP=		-L${LIBROOT}/gdp -lgdp
INCEP=
LIBEP=		-L${LIBROOT}/ep -lep
JANSSONROOT=	${HOME}/src/jansson-2.6
#INCJANSSON=	-I${JANSSONROOT}
#LIBJANSSON=	-L${JANSSONROOT} -ljansson
LIBJANSSON=	-ljansson
#EVENT2ROOT=	${HOME}/src
#INCEVENT2=	-I${EVENT2ROOT}
LIBEVENT2=	-levent -levent_pthreads -pthread
INCS=		${INCGDP} ${INCEP} ${INCJANSSON} ${INCEVENT2}
LDFLAGS=	${LIBGDP} ${LIBEP} ${LIBJANSSON} ${LIBEVENT2}
PG=
O=		-O
COPTS=		${PG} -g -Wall $O
CFLAGS=		${COPTS} ${INCS}

all:	${ALL}

writer-test:	writer-test.o
	${CC} -o $@ writer-test.o ${LDFLAGS}

reader-test:	reader-test.o
	${CC} -o $@ reader-test.o ${LDFLAGS}

gdp-rest:	gdp-rest.o ../scgilib/scgilib.o
	${CC} -o $@ gdp-rest.o ${LDFLAGS} ../scgilib/scgilib.o

${ALL}:		${LIBROOT}/gdp/libgdp.a ${LIBROOT}/ep/libep.a

clean:
	-rm ${ALL} *.o *.core
