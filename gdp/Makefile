
OBJS=	\
	gdp_nexus.o \
	gdp_log.o \
	gdp_timestamp.o \

ALL=	libnexus.a

PG=
O=	-O
COPTS=	${PG} -g -Wall $O
CFLAGS=	${COPTS} -I..
LDFLAGS=-L.
RANLIB=	ranlib

all: ${ALL}

libnexus.a: ${OBJS}
	${AR} -r $@ ${OBJS}
	${RANLIB} $@

clean:
	-rm ${ALL} *.o *.core
