ALL=		machine-mon

DESTDIR=
LOCALROOT=	/usr
INSTALLROOT=	${DESTDIR}${LOCALROOT}
BINDIR=		${INSTALLROOT}/bin
SBINDIR=	${INSTALLROOT}/sbin
INSTALL=	install

LOCAL1=		/usr/local
LOCAL2=		/opt/local
INCROOT=	..
INCSEARCH=	-I${LOCAL1}/include -I${LOCAL2}/include
LIBROOT=	${INCROOT}
#LIBSEARCH=	-L${INCROOT}/libs
LIBSEARCH=	-L${LIBROOT}/ep -L${LIBROOT}/gdp
INCGDP=		-I${INCROOT}
LIBGDP=		-lgdp
INCEP=
LIBEP=		-lep
INCJANSSON=
LIBJANSSON=	-L${LOCAL1}/lib -L${LOCAL2}/lib -ljansson
INCEVENT2=
LIBEVENT2=	-levent -levent_pthreads -pthread
INCCRYPTO=
LIBCRYPTO=	-lcrypto
LIBAVAHI=	-lavahi-client -lavahi-common
INCS=		${INCSEARCH} ${INCGDP} ${INCEP} \
		${INCJANSSON} ${INCEVENT2} ${INCCRYPTO}
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

machine-mon:	machine-mon.o
	${CC} -o $@ machine-mon.o ${LDFLAGS}

clean:
	-rm -f ${ALL} *.o *.core

${BINDIR} ${SBINDIR}:
	${MKDIR} $@
