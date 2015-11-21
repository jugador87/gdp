#
#  ----- BEGIN LICENSE BLOCK -----
#	GDP: Global Data Plane Support Library
#	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
#
#	Copyright (c) 2015, Regents of the University of California.
#
#	Redistribution and use in source and binary forms, with or without
#	modification, are permitted provided that the following conditions
#	are met:
#
#	1. Redistributions of source code must retain the above copyright
#	notice, this list of conditions and the following disclaimer.
#
#	2. Redistributions in binary form must reproduce the above copyright
#	notice, this list of conditions and the following disclaimer in the
#	documentation and/or other materials provided with the distribution.
#
#	3. Neither the name of the copyright holder nor the names of its
#	contributors may be used to endorse or promote products derived
#	from this software without specific prior written permission.
#
#	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
#	COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
#	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
#	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
#	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#	POSSIBILITY OF SUCH DAMAGE.
#  ----- END LICENSE BLOCK -----
#

DESTDIR=
LOCALROOT=	/usr
CRYPTOROOT=	/usr/local/ssl
INSTALLROOT=	${DESTDIR}${LOCALROOT}
INCDIR=		${INSTALLROOT}/include
LIBDIR=		${INSTALLROOT}/lib
INSTALL=	install

OBJS=	\
	gdp_api.o \
	gdp_buf.o \
	gdp_chan.o \
	gdp_crypto.o \
	gdp_event.o \
	gdp_gcl_cache.o \
	gdp_gcl_ops.o \
	gdp_gclmd.o \
	gdp_datum.o \
	gdp_main.o \
	gdp_pdu.o \
	gdp_proto.o \
	gdp_req.o \
	gdp_stat.o \
	gdp_subscr.o \
	gdp_util.o \
	gdp_zc_client.o \
	gdp_zc_server.o \

PUBHFILES=	\
	gdp.h \
	gdp_buf.h \
	gdp_stat.h \
	gdp_version.h \
	gdp_zc_client.h \
	gdp_zc_server.h \

PRIVHFILES=	\
	gdp_event.h \
	gdp_gclmd.h \
	gdp_pdu.h \
	gdp_priv.h \

HFILES=		${PUBHFILES} ${PRIVHFILES}

GDPLIBMAJVER=	0
GDPLIBMINVER=	2
GDPLIBVER=	${GDPLIBMAJVER}.${GDPLIBMINVER}
INCALL=		${PUBHFILES}
LIBALL=		libgdp.a libgdp.so.${GDPLIBVER}
ALL=		${INCALL} ${LIBALL}

LOCAL1=		/usr/local
LOCAL2=		/opt/local
INCROOT=	..
INCSEARCH=	-I${LOCAL1}/include -I${LOCAL2}/include
LIBROOT=	${INCROOT}
INCCRYPTO=	-I${CRYPTOROOT}/include
LIBCRYPTO=	-lcrypto
INCEVENT2=
LIBEVENT2=	-levent -levent_pthreads
INCAVAHI=
LIBAVAHI=	-lavahi-client -lavahi-common
INCEP=		-I${INCROOT}
LIBEP=		-lep
INCS=		${INCSEARCH} ${INCEP} ${INCEVENT2} ${INCCRYPTO} ${INCAVAHI}
LIBSEARCH=	-L${CRYPTOROOT}/lib \
		-L${LIBROOT}/ep \
		-L${LOCAL1}/lib \
		-L${LOCAL2}/lib
LDFLAGS= 	${LIBSEARCH} ${LIBEP} ${LIBEVENT2} ${LIBCRYPTO} ${LIBAVAHI}
PG=
O=		-O
WALL=		-Wall
STD=
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} ${INCS} -fPIC

LN=		ln
MV=		mv
CP=		cp
RM=		rm
MKDIR=		mkdir -p
RANLIB=		ranlib
SHARED=		-shared
#SHARED=		-shared -Wl,-soname,$@

all: ${ALL}

libgdp.a: ${OBJS}
	${AR} -r $@ ${OBJS}
	${RANLIB} $@

libgdp.so.${GDPLIBVER}: ${OBJS}
	${CC} ${SHARED} -o $@ ${OBJS} ${LDFLAGS}

${OBJS}: ${HFILES}

GDP_LIB_VERSION=	`date +%Y%m%d`

newversion:	FORCE
	sed -e "s/@GDP_LIB_VERSION@/${GDP_LIB_VERSION}/" < gdp_version.template > gdp_version.h

clean:
	-rm -f ${LIBALL} *.o *.core

install:	${INCDIR}/gdp ${LIBDIR} ${ALL}
	${CP} ${INCALL} ${INCDIR}/gdp
	${INSTALL} ${LIBALL} ${LIBDIR}
	(cd ${LIBDIR}; rm -f libgdp.so.${GDPLIBMAJVER}; ln -s libgdp.so.${GDPLIBVER} libgdp.so.${GDPLIBMAJVER})
	(cd ${LIBDIR}; rm -f libgdp.so; ln -s libgdp.so.${GDPLIBMAJVER} libgdp.so)
	(cd ${LIBDIR}; rm -f libgdp.${GDPLIBVER}.dylib; ln -s libgdp.so.${GDPLIBVER} libgdp.${GDPLIBVER}.dylib)

${INCDIR}/gdp ${LIBDIR}:
	${MKDIR} $@

ADM=		../adm
UPDATE_LICENSE=	${ADM}/update-license.sh

update-license:
	${UPDATE_LICENSE} Makefile *.[ch]

FORCE:
