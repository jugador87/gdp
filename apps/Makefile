ALL=	writer-test reader-test gdp-rest rw-bench log-view

LOCAL=		/usr/local
INCROOT=	..
LIBROOT=	${INCROOT}
INCGDP=		-I${INCROOT}
LIBGDP=		-L${LIBROOT}/gdp -lgdp
INCEP=
LIBEP=		-L${LIBROOT}/ep -lep
INCJANSSON=	-I${LOCAL}/include
LIBJANSSON=	-ljansson
INCEVENT2=	-I${LOCAL}/include
LIBEVENT2=	-levent -levent_pthreads -pthread
INCCRYPTO=
LIBCRYPTO=	-lcrypto
INCS=		${INCGDP} ${INCEP} ${INCJANSSON} ${INCEVENT2} ${INCCRYPTO}
LDFLAGS=	${LIBGDP} ${LIBEP} ${LIBJANSSON} ${LIBEVENT2} ${LIBCRYPTO}
PG=
O=		-O
WALL=		-Wall
STD=
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} ${INCS}

all:	${ALL}

writer-test:	writer-test.o
	${CC} -o $@ writer-test.o ${LDFLAGS}

reader-test:	reader-test.o
	${CC} -o $@ reader-test.o ${LDFLAGS}

gdp-rest:	gdp-rest.o ../scgilib/scgilib.o
	${CC} -o $@ gdp-rest.o ${LDFLAGS} ../scgilib/scgilib.o

rw-bench:	rw-bench.o
	$(CC) -o $@ rw-bench.o $(LDFLAGS)

log-view:	log-view.o
	$(CC) -o $@ log-view.o $(LDFLAGS)

${ALL}:		${LIBROOT}/gdp/libgdp.a ${LIBROOT}/ep/libep.a

clean:
	-rm -f ${ALL} *.o *.core
