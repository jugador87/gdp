O=		-O
PG=
WALL=		-Wall
STD=		-std=c99
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} ${INCS}
ALL=		helloworld

all:		${ALL}

helloworld:	helloworld.o scgilib.o
	${CC} -o helloworld helloworld.o scgilib.o

scgilib.o:	scgilib.h

clean:
	-rm -f ${ALL} *.o
