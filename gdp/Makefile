
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

LIBCRYPTO=	-lcrypto

PG=
O=		-O
WALL=		-Wall
STD=
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} -I.. -fPIC
LIBCRYPTO=	-lcrypto
LIBEVENT=	-levent -levent_pthreads
LIBEP=		-L../ep -lep
LDFLAGS= 	${LIBEVENT} ${LIBEP} ${LIBCRYPTO}
LIBDIR= 	../libs
MV=		mv
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

${OBJS}: ${HFILES}

clean:
	-rm -f ${ALL} *.o *.core
