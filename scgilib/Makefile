O=		-O
PG=
COPTS=		${PG} -g -Wall $O
CFLAGS=		${COPTS} ${INCS}
ALL=		helloworld

all:		${ALL}

helloworld:	helloworld.c scgilib.o

scgilib.o:	scgilib.h

clean:
	rm ${ALL} *.o
