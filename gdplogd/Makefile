SBINALL=	gdplogd
ALL=		${SBINALL}

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
#LIBSEARCH=	-L${INCROOT}/libs
LIBSEARCH=	-L${CRYPTOROOT}/lib \
		-L${LIBROOT}/gdp \
		-L${LIBROOT}/ep \
		-L${LOCAL1}/lib \
		-L${LOCAL2}/lib
INCGDP=		-I${INCROOT}
LIBGDP=		-lgdp
INCEP=		-I${INCROOT}
LIBEP=		-lep
INCEVENT2=
LIBEVENT2=	-levent -levent_pthreads -pthread
INCCRYPTO=	-I${CRYPTOROOT}/include
LIBCRYPTO=	-lcrypto
LIBAVAHI=	-lavahi-client -lavahi-common
INCS=		${INCSEARCH} ${INCGDP} ${INCEP} \
		${INCEVENT2} ${INCCRYPTO}
LDFLAGS=	${LIBSEARCH} ${LIBGDP} ${LIBEP} \
		${LIBEVENT2} ${LIBCRYPTO} ${LIBAVAHI}
PG=
O=		-O
WALL=		-Wall
STD=
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} ${INCS}
MKDIR=		mkdir -p

OBJS=	\
		logd.o \
		logd_adv.o \
		logd_gcl.o \
		logd_physlog.o \
		logd_proto.o \
		logd_pubsub.o \
		logd_circular_buffer.o \

HDEPS=	\
		logd.h \
		logd_physlog.h \
		logd_pubsub.h \
		logd_circular_buffer.h \
		${INCROOT}/gdp/gdp.h \
		${INCROOT}/gdp/gdp_pdu.h \

LIBDEPS= \
		${LIBROOT}/gdp/libgdp.a \
		${LIBROOT}/ep/libep.a \

all: ${ALL}

gdplogd: ${OBJS} ${LIBDEPS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

clean:
	-rm -f ${ALL} *.o *.core

install:	${SBINDIR} ${ALL}
	${INSTALL} ${SBINALL} ${SBINDIR}

${SBINDIR}:
	${MKDIR} $@

${OBJS}: ${HDEPS}
