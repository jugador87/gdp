GDPROOT=	../..
LIBROOT=	${GDPROOT}
LIBSEARCH=	-L${LIBROOT}/ep -L${LIBROOT}/gdp
LDFLAGS=	${LIBSEARCH} -lgdp -lep -levent -levent_pthreads -pthread \
		-lcrypto -ljansson -lwiringPi
INCADD=
INCS=		-I${GDPROOT} ${INCADD}

ALL=		pi-reader pi-writer

all:		${ALL}

pi-reader:	pi-reader.o
	cc -o $@ pi-reader.o ${LDFLAGS}

pi-reader.o:	pi-reader.c
	cc -c ${INCS} pi-reader.c

pi-writer:	pi-writer.o
	cc -o $@ pi-writer.o ${LDFLAGS}

pi-writer.o:	pi-writer.c
	cc -c ${INCS} pi-writer.c
