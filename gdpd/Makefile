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
#EVENT2ROOT=	${HOME}/src
#INCEVENT2=	-I${EVENT2ROOT}
LIBEVENT2=	-levent -levent_pthreads -pthread
INCS=		${INCGDP} ${INCEP} ${INCJANSSON} ${INCEVENT2}
LDFLAGS=	${LIBGDP} ${LIBEP} ${LIBJANSSON} ${LIBEVENT2}
PG=
O=		-O
WALL=		-Wall
STD=		-std=gnu99
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} ${INCS}

OBJS=	\
		gdpd.o \
		gdpd_physlog.o \
		gdp_circular_buffer.o \
		thread_pool.o \

HDEPS=	\
		gdpd.h \
		gdpd_physlog.h \
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
