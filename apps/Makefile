#ALL=	gcl-create writer-test reader-test gdp-rest rw-bench log-view
BINALL=		gcl-create \
		log-mirror \
		log-view \
		reader-test \
		writer-test\

SBINALL=	gdp-rest \
		log-view \
		gdp-name-xlate\

ALL=		${BINALL} ${SBINALL}

DESTDIR=
LOCALROOT=	/usr
INSTALLROOT=	${DESTDIR}${LOCALROOT}
CRYPTOROOT=	/usr/local/ssl
BINDIR=		${INSTALLROOT}/bin
SBINDIR=	${INSTALLROOT}/sbin
INSTALL=	install

LOCAL1=		/usr/local
LOCAL2=		/opt/local
INCROOT=	..
LIBROOT=	${INCROOT}
LIBSEARCH=	-L${CRYPTOROOT}/lib \
		-L${LIBROOT}/ep \
		-L${LIBROOT}/gdp
INCGDP=		-I${INCROOT}
LIBGDP=		-lgdp
INCEP=
LIBEP=		-lep
INCJANSSON=	-I${LOCAL1}/include -I${LOCAL2}/include
LIBJANSSON=	-L${LOCAL1}/lib -L${LOCAL2}/lib -ljansson
INCEVENT2=	-I${LOCAL1}/include -I${LOCAL2}/include
LIBEVENT2=	-levent -levent_pthreads -pthread
INCCRYPTO=	-I${CRYPTOROOT}/include
LIBCRYPTO=	-lcrypto
INCS=		${INCGDP} ${INCEP} ${INCJANSSON} ${INCEVENT2} ${INCCRYPTO}
LDFLAGS=	${LIBSEARCH} ${LIBGDP} ${LIBEP} ${LIBJANSSON} ${LIBEVENT2} ${LIBCRYPTO}
PG=
O=		-O
WALL=		-Wall
STD=
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} ${INCS}
MKDIR=		mkdir -p

all:	${ALL}

gcl-create:	gcl-create.o
	${CC} -o $@ gcl-create.o ${LDFLAGS}

writer-test:	writer-test.o
	${CC} -o $@ writer-test.o ${LDFLAGS}

reader-test:	reader-test.o
	${CC} -o $@ reader-test.o ${LDFLAGS}

gdp-rest:	gdp-rest.o ../scgilib/scgilib.o
	${CC} -o $@ gdp-rest.o ${LDFLAGS} ../scgilib/scgilib.o

rw-bench:	rw-bench.o
	$(CC) -o $@ rw-bench.o $(LDFLAGS)

log-mirror:	log-mirror.o
	$(CC) -o $@ log-mirror.o $(LDFLAGS)

log-view:	log-view.o
	$(CC) -o $@ log-view.o $(LDFLAGS)

gdp-name-xlate:	gdp-name-xlate.o
	$(CC) -o $@ gdp-name-xlate.o $(LDFLAGS)

${ALL}:		${LIBROOT}/gdp/libgdp.a ${LIBROOT}/ep/libep.a

clean:
	-rm -f ${ALL} *.o *.core

install:	${BINDIR} ${SBINDIR} ${ALL}
	${INSTALL} ${BINALL} ${BINDIR}
	${INSTALL} ${SBINALL} ${SBINDIR}

${BINDIR} ${SBINDIR}:
	${MKDIR} $@
