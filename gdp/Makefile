
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

ALL=	libgdp.a

LIBCRYPTO=	-lcrypto

PG=
O=	-O
WALL=	-Wall
STD=
COPTS=	${PG} -g ${WALL} $O ${STD}
CFLAGS=	${COPTS} -I..
LDFLAGS=-L. ${LIBCRYPTO}
RANLIB=	ranlib

all: ${ALL}

libgdp.a: ${OBJS}
	${AR} -r $@ ${OBJS}
	${RANLIB} $@

${OBJS}: ${HFILES}

clean:
	-rm -f ${ALL} *.o *.core
