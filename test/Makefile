#
#  ----- BEGIN LICENSE BLOCK -----
#	Applications for the Global Data Plane
#	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
#
#	Copyright (c) 2015, Regents of the University of California.
#	All rights reserved.
#
#	Permission is hereby granted, without written agreement and without
#	license or royalty fees, to use, copy, modify, and distribute this
#	software and its documentation for any purpose, provided that the above
#	copyright notice and the following two paragraphs appear in all copies
#	of this software.
#
#	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
#	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
#	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
#	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
#	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
#	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
#	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
#	OR MODIFICATIONS.
#  ----- END LICENSE BLOCK -----
#

BINALL= \
		t_fwd_append \
		t_multimultiread \

SBINALL=

MAN1ALL=

MAN3ALL=

MAN5ALL=

MAN7ALL=

MAN8ALL=

MANALL=		${MAN1ALL} ${MAN3ALL} ${MAN5ALL} ${MAN7ALL} ${MAN8ALL}

# files that should be removed on "make clean"
CLEANALL=	${BINALL} ${SBINALL}
# files that should be made public
PUBALL=		${SCRIPTS} ${MANALL}
# "make all" targets (may be overlaps)
ALL=		${CLEANALL} ${PUBALL}

DESTDIR=
LOCALROOT=	/usr
INSTALLROOT=	${DESTDIR}${LOCALROOT}
CRYPTOROOT=	/opt/local
BINDIR=		${INSTALLROOT}/bin
SBINDIR=	${INSTALLROOT}/sbin
MANDIR=		${INSTALLROOT}/share/man
ALLDIRS=	${BINDIR} \
		${SBINDIR} \
		${MANDIR}/man1 \
		${MANDIR}/man8

INSTALL=	install

LOCAL1=		/usr/local
LOCAL2=		/opt/local
INCROOT=	..
INCSEARCH=	-I${LOCAL1}/include -I${LOCAL2}/include
LIBROOT=	${INCROOT}
LIBSEARCH=	-L${CRYPTOROOT}/lib \
		-L${LIBROOT}/ep \
		-L${LIBROOT}/gdp
INCGDP=		-I${INCROOT}
LIBGDP=		-lgdp
INCEP=
LIBEP=		-lep
INCJANSSON=
LIBJANSSON=	-L${LOCAL1}/lib -L${LOCAL2}/lib -ljansson
INCEVENT2=
LIBEVENT2=	-levent -levent_pthreads -pthread
INCCRYPTO=	-I${CRYPTOROOT}/include
LIBCRYPTO=	-lcrypto
INCAVAHI=
LIBAVAHI=	-lavahi-client -lavahi-common
INCS=		${INCSEARCH} ${INCGDP} ${INCEP} \
		${INCJANSSON} ${INCEVENT2} ${INCCRYPTO} ${INCAVAHI}
LDFLAGS=	${LIBSEARCH} ${LIBGDP} ${LIBEP} \
		${LIBJANSSON} ${LIBEVENT2} ${LIBCRYPTO} ${LIBAVAHI}
PG=
O=		-O
WALL=		-Wall
STD=
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} ${INCS}
MKDIR=		mkdir -p

all:	${ALL}

t_fwd_append:	t_fwd_append.o
	${CC} -o $@ t_fwd_append.o ${LDFLAGS}

t_multimultiread:	t_multimultiread.o
	${CC} -o $@ t_multimultiread.o ${LDFLAGS}

${CLEANALL}:	${LIBROOT}/gdp/libgdp.a ${LIBROOT}/ep/libep.a

clean:
	-rm -f ${CLEANALL} *.o *.core

${ALLDIRS}:
	${MKDIR} $@

ADM=		../adm
UPDATE_LICENSE=	${ADM}/update-license.sh

update-license:
	${UPDATE_LICENSE} Makefile *.[ch]

FORCE:
