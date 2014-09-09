ALL=		gdpd

INCROOT=	..
LIBROOT=	${INCROOT}
INCGDP=		-I${INCROOT}
LIBGDP=		-L${LIBROOT}/gdp -lgdp
INCEP=
LIBEP=		-L${LIBROOT}/ep -lep
JANSSONROOT=    ${HOME}/src/jansson-2.6
#INCJANSSON=	-I${JANSSONROOT}
#LIBJANSSON=	-L${JANSSONROOT} -ljansson
INCEVENT2=	-I/usr/local/include
LIBEVENT2=	-levent -levent_pthreads -pthread
INCCRYPTO=
LIBCRYPTO=	-lcrypto
INCS=		${INCGDP} ${INCEP} ${INCJANSSON} ${INCEVENT2} ${INCCRYPTO}
LDFLAGS=	${LIBGDP} ${LIBEP} ${LIBJANSSON} ${LIBEVENT2} ${LIBCRYPTO}
PG=
O=		-O
WALL=		-Wall
STD=
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} ${INCS}

OBJS=	\
		gdpd.o \
		gdpd_physlog.o \
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
