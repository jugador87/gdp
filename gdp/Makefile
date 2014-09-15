
OBJS=	\
	gdp_api.o \
	gdp_buf.o \
	gdp_event.o \
	gdp_gcl.o \
	gdp_log.o \
	gdp_datum.o \
	gdp_pkt.o \
	gdp_proto.o \
	gdp_req.o \
	gdp_stat.o \

HFILES=	\
	gdp.h \
	gdp_buf.h \
	gdp_event.h \
	gdp_pkt.h \
	gdp_priv.h \
	gdp_stat.h \

GDPLIBVER=	1.0
ALL=		libgdp.a libgdp.so.${GDPLIBVER}

INCROOT=	..
LIBROOT=	${INCROOT}
LIBCRYPTO=	-lcrypto
LIBEVENT2=	-levent -levent_pthreads
LIBEP=		-L${LIBROOT}/ep -lep
INCEP=		-I${INCROOT}
INCEVENT2=	-I/usr/local/include
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
	${RM} -f ${LIBDIR}/libgdp.${GDPLIBVER}.dylib ${LIBDIR}/libgdp.${GDPLIBVER}.so
	${LN} ${LIBDIR}/$@ ${LIBDIR}/libgdp.${GDPLIBVER}.dylib
	${LN} ${LIBDIR}/$@ ${LIBDIR}/libgdp.${GDPLIBVER}.so 

${OBJS}: ${HFILES}

clean:
	-rm -f ${ALL} *.o *.core
