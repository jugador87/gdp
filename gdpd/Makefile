ALL=		gdpd

INCROOT=	..
LIBROOT=	${INCROOT}
SOLIBROOT=	${INCROOT}/libs
INCGDP=		-I${INCROOT}
LIBGDP=		-L${SOLIBROOT} -lgdp
INCEP=
LIBEP=		-L${SOLIBROOT} -lep
INCEVENT2=	-I/usr/local/include
LIBEVENT2=	-levent -levent_pthreads -pthread
INCCRYPTO=
LIBCRYPTO=	-lcrypto
INCS=		${INCGDP} ${INCEP} ${INCEVENT2} ${INCCRYPTO}
LDFLAGS=	${LIBGDP} ${LIBEP} ${LIBEVENT2} ${LIBCRYPTO}
PG=
O=		-O
WALL=		-Wall
STD=
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} ${INCS}

OBJS=	\
		gdpd.o \
		gdpd_gcl.o \
		gdpd_physlog.o \
		gdpd_proto.o \
		gdpd_pubsub.o \
		gdp_circular_buffer.o \
		thread_pool.o \

HDEPS=	\
		gdpd.h \
		gdpd_physlog.h \
		gdpd_pubsub.h \
		gdp_circular_buffer.h \
		thread_pool.h \
		../gdp/gdp.h \
		../gdp/gdp_pkt.h \

LIBDEPS= \
	 	../gdp/libgdp.a \

all: ${ALL}

gdpd: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

${OBJS}: ${HDEPS} ${LIBDEPS}

clean:
	-rm -f ${ALL} *.o *.core
