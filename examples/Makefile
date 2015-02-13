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
LIBROOT=	${INCROOT}
#LIBSEARCH=	-L${INCROOT}/libs
LIBSEARCH=	-L${LIBROOT}/ep -L${LIBROOT}/gdp
INCGDP=		-I${INCROOT}
LIBGDP=		-lgdp
INCEP=
LIBEP=		-lep
INCJANSSON=	-I${LOCAL1}/include -I${LOCAL2}/include
LIBJANSSON=	-L${LOCAL1}/lib -L${LOCAL2}/lib -ljansson
INCEVENT2=	-I${LOCAL1}/include -I${LOCAL2}/include
LIBEVENT2=	-levent -levent_pthreads -pthread
INCCRYPTO=
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

machine-mon:	machine-mon.o
	${CC} -o $@ machine-mon.o ${LDFLAGS}

${ALL}:		${LIBROOT}/gdp/libgdp.a ${LIBROOT}/ep/libep.a

clean:
	-rm -f ${ALL} *.o *.core

${BINDIR} ${SBINDIR}:
	${MKDIR} $@
