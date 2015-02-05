
OBJS=	\
	gdp_api.o \
	gdp_buf.o \
	gdp_chan.o \
	gdp_event.o \
	gdp_gcl_cache.o \
	gdp_gcl_ops.o \
	gdp_gclmd.o \
	gdp_datum.o \
	gdp_main.o \
	gdp_pdu.o \
	gdp_proto.o \
	gdp_req.o \
	gdp_stat.o \
	gdp_subscr.o \
	gdp_util.o \

HFILES=	\
	gdp.h \
	gdp_buf.h \
	gdp_event.h \
	gdp_gclmd.h \
	gdp_pdu.h \
	gdp_priv.h \
	gdp_stat.h \
	gdp_version.h \

GDPLIBMAJVER=	1
GDPLIBMINVER=	0
GDPLIBVER=	${GDPLIBMAJVER}.${GDPLIBMINVER}
ALL=		libgdp.a libgdp.so.${GDPLIBVER}

LOCAL1=		/usr/local
LOCAL2=		/opt/local
INCROOT=	..
LIBROOT=	${INCROOT}
LIBCRYPTO=	-lcrypto
LIBEVENT2=	-levent -levent_pthreads
LIBEP=		-L${LIBROOT}/ep -lep
INCEP=		-I${INCROOT}
INCEVENT2=	-I${LOCAL1}/include -I${LOCAL2}/include
INCS=		${INCEP} ${INCEVENT2} ${INCCRYPTO}
LDFLAGS= 	${LIBEP} ${LIBEVENT2} ${LIBCRYPTO}
LIBDIR= 	${LIBROOT}/libs
PG=
O=		-O
WALL=		-Wall
STD=
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} ${INCS} -fPIC

LN=		ln
MV=		mv
RM=		rm
RANLIB=		ranlib
SHARED=		-shared
#SHARED=		-shared -Wl,-soname,$@

all: ${ALL}

libgdp.a: ${OBJS}
	${AR} -r $@ ${OBJS}
	${RANLIB} $@

libgdp.so.${GDPLIBVER}: ${OBJS}
	${CC} ${SHARED} -o $@ ${OBJS} ${LDFLAGS}
	${MV} $@ ${LIBDIR}
	(cd ${LIBDIR}; rm -f libgdp.so.${GDPLIBMAJVER}; ln -s libgdp.so.${GDPLIBVER} libgdp.so.${GDPLIBMAJVER})
	(cd ${LIBDIR}; rm -f libgdp.so; ln -s libgdp.so.${GDPLIBMAJVER} libgdp.so)
	(cd ${LIBDIR}; rm -f libgdp.${GDPLIBVER}.dylib; ln -s libgdp.so.${GDPLIBVER} libgdp.${GDPLIBVER}.dylib)

${OBJS}: ${HFILES}

GDP_LIB_VERSION=	`date +%Y%m%d`

newversion:	FORCE
	sed -e "s/@GDP_LIB_VERSION@/${GDP_LIB_VERSION}/" < gdp_version.template > gdp_version.h

clean:
	-rm -f ${ALL} *.o *.core

FORCE:
