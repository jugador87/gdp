O=		-O
PG=
WALL=		-Wall
COPTS=		${PG} -g ${WALL} $O -std=c99
CFLAGS=		${COPTS} ${INCS}
ALL=		helloworld

all:		${ALL}

helloworld:	helloworld.o scgilib.o
	${CC} -o helloworld helloworld.o scgilib.o

scgilib.o:	scgilib.h

clean:
	-rm -f ${ALL} *.o
