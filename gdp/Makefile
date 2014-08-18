
OBJS=	\
	gdp_api.o \
	gdp_buf.o \
	gdp_gcl.o \
	gdp_log.o \
	gdp_msg.o \
	gdp_pkt.o \
	gdp_proto.o \
	gdp_req.o \
	gdp_stat.o \
	gdp_timestamp.o \

HFILES=	\
	gdp.h \
	gdp_buf.h \
	gdp_pkt.h \
	gdp_priv.h \
	gdp_stat.h \
	gdp_timestamp.h \

ALL=	libgdp.a

PG=
O=	-O
WALL=	-Wall
STD=	-std=gnu99
COPTS=	${PG} -g ${WALL} $O ${STD}
CFLAGS=	${COPTS} -I..
LDFLAGS=-L.
RANLIB=	ranlib

all: ${ALL}

libgdp.a: ${OBJS}
	${AR} -r $@ ${OBJS}
	${RANLIB} $@

${OBJS}: ${HFILES}

clean:
	-rm -f ${ALL} *.o *.core
