#ALL=	gcl-create gdp-writer gdp-reader gdp-rest rw-bench log-view
BINALL=		gcl-create \
		log-mirror \
		log-view \
		gdp-reader \
		gdp-writer \
		gdp-zcbrowse \

SBINALL=	gdp-rest \
		log-view \
		gdp-name-xlate \
		gdp-zcpublish \

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
INCSEARCH=	-I${LOCAL1}/include -I${LOCAL2}/include
LIBROOT=	${INCROOT}
LIBSEARCH=	-L${CRYPTOROOT}/lib \
		-L${LIBROOT}/ep \
		-L${LIBROOT}/gdp
INCGDP=		-I${INCROOT}
LIBGDP=		-lgdp
INCEP=
LIBEP=		-lep
INCJANSSON=
LIBJANSSON=	-L${LOCAL1}/lib -L${LOCAL2}/lib -ljansson
INCEVENT2=
LIBEVENT2=	-levent -levent_pthreads -pthread
INCCRYPTO=	-I${CRYPTOROOT}/include
LIBCRYPTO=	-lcrypto
INCAVAHI=
LIBAVAHI=	-lavahi-client -lavahi-common
INCS=		${INCSEARCH} ${INCGDP} ${INCEP} \
		${INCJANSSON} ${INCEVENT2} ${INCCRYPTO} ${INCAVAHI}
LDFLAGS=	${LIBSEARCH} ${LIBGDP} ${LIBEP} \
		${LIBJANSSON} ${LIBEVENT2} ${LIBCRYPTO} ${LIBAVAHI}
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

gdp-writer:	gdp-writer.o
	${CC} -o $@ gdp-writer.o ${LDFLAGS}

gdp-reader:	gdp-reader.o
	${CC} -o $@ gdp-reader.o ${LDFLAGS}

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

gdp-zcpublish:	gdp-zcpublish.o
	$(CC) -o $@ gdp-zcpublish.o $(LDFLAGS)

gdp-zcbrowse:	gdp-zcbrowse.o
	$(CC) -o $@ gdp-zcbrowse.o $(LDFLAGS)

${ALL}:		${LIBROOT}/gdp/libgdp.a ${LIBROOT}/ep/libep.a

clean:
	-rm -f ${ALL} *.o *.core

install:	${BINDIR} ${SBINDIR} ${ALL}
	${INSTALL} ${BINALL} ${BINDIR}
	${INSTALL} ${SBINALL} ${SBINDIR}

${BINDIR} ${SBINDIR}:
	${MKDIR} $@
