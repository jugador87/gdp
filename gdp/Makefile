
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

ALL=	libgdp.a libgdp.so

PG=
O=	-O
WALL=	-Wall
STD=	-std=gnu99
COPTS=	${PG} -g ${WALL} $O ${STD}
CFLAGS=	${COPTS} -I.. -fPIC
LIBEVENT=-levent -levent_pthreads
LIBEP=-L../ep -lep
LDFLAGS= ${LIBEVENT} ${LIBEP}
LIBDIR= ../libs
RANLIB=	ranlib
MV= mv

all: ${ALL}

libgdp.a: ${OBJS}
	${AR} -r $@ ${OBJS}
	${RANLIB} $@

libgdp.so: ${OBJS}
	${CC} -shared -Wl,-soname,$@ -o $@ ${OBJS} ${LDFLAGS}
	${MV} $@ ${LIBDIR}

${OBJS}: ${HFILES}

clean:
	-rm -f ${ALL} *.o *.core
