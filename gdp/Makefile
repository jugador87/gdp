
OBJS=	\
	gdp_gcl.o \
	gdp_log.o \
	gdp_protocol.o \
	gdp_stat.o \
	gdp_timestamp.o \

HFILES=	\
	gdp.h \
	gdp_protocol.h \
	gdp_stat.h \
	gdp_timestamp.h \

ALL=	libgdp.a

PG=
O=	-O
WALL=	-Wall
COPTS=	${PG} -g ${WALL} $O -std=c99
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
