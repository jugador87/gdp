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
LIBJANSSON=	-ljansson
#EVENT2ROOT=	${HOME}/src
#INCEVENT2=	-I${EVENT2ROOT}
LIBEVENT2=	-levent -levent_pthreads
INCS=		${INCGDP} ${INCEP} ${INCJANSSON} ${INCEVENT2}
LDFLAGS=	${LIBGDP} ${LIBEP} ${LIBJANSSON} ${LIBEVENT2}
PG=
O=		-O
COPTS=		${PG} -g -Wall $O
CFLAGS=		${COPTS} ${INCS}

OBJS=	\
		gdpd.o \
		gdpd_gcl.o \
		gdpd_physlog.o \

HDEPS=	\
		gdpd.h \
		gdpd_gcl.h \
		gdpd_physlog.h \
		../gdp/gdp.h \
		../gdp/gdp_protocol.h \

LIBDEPS= \
	 	../gdp/libgdp.a \

all: ${ALL}

gdpd: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

${OBJS}: ${HDEPS} ${LIBDEPS}

clean:
	-rm ${ALL} *.o *.core
