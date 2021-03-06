#
#  Makefile for libep.
#
#  ----- BEGIN LICENSE BLOCK -----
#	LIBEP: Enhanced Portability Library (Reduced Edition)
#
#	Copyright (c) 2008-2015, Eric P. Allman.  All rights reserved.
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

DESTDIR=
LOCALROOT=	/usr
INSTALLROOT=	${DESTDIR}${LOCALROOT}
BINDIR=		${INSTALLROOT}/bin
SBINDIR=	${INSTALLROOT}/sbin
LIBDIR=		${INSTALLROOT}/lib
INCDIR=		${INSTALLROOT}/include
MANDIR=		${INSTALLROOT}/share/man
ALLDIRS=	${LIBDIR} \
		${INCDIR}/ep

INSTALL=	install
CRYPTOROOT=	/opt/local

CURDIR=	.

OBJS_MOST=	\
	ep_adm.o \
	ep_app.o \
	ep_assert.o \
	ep_b64.o \
	ep_crypto.o \
	ep_crypto_cipher.o \
	ep_crypto_key.o \
	ep_crypto_md.o \
	ep_crypto_sign.o \
	ep_crypto_vrfy.o \
	ep_dbg.o \
	ep_dumpfds.o \
	ep_fopensmem.o \
	ep_fread_unlocked.o \
	ep_funclist.o \
	ep_hash.o \
	ep_hexdump.o \
	ep_lib.o \
	ep_log.o \
	ep_mem.o \
	ep_net.o \
	ep_pcvt.o \
	ep_pprint.o \
	ep_prflags.o \
	ep_rpool.o \
	ep_stat.o \
	ep_statcodes.o \
	ep_string.o \
	ep_syslog.o \
	ep_thr.o \
	ep_thr_pool.o \
	ep_time.o \
	ep_xlate.o \

OBJS=	\
	${OBJS_MOST} \
	ep_version.o \

OBJS_NOTYET=	\
	ep_obj.o \
	ep_cfg.o \
	ep_crackargv.o \
	ep_init.o \
	ep_net_cvtaddr.o \
	ep_pat.o \
	ep_plist.o \
	ep_st_control.o \
	ep_st_getline.o \
	ep_st_getrec.o \
	ep_st_globs.o \
	ep_st_open.o \
	ep_st_printf.o \
	ep_st_putrec.o \
	ep_st_puts.o \
	ep_st_read.o \
	ep_st_seek.o \
	ep_st_support.o \
	ep_st_tgetline.o \
	ep_st_type_divider.o \
	ep_st_type_dmem.o \
	ep_st_type_smem.o \
	ep_st_type_syslog.o \
	ep_st_ungetc.o \
	ep_st_write.o \
	ep_tree.o \
	posix_event.o \
	posix_io.o \
	posix_support.o \
	unix_init.o \
	unix_syslog.o \

HFILES=	\
	ep.h \
	ep_app.h \
	ep_assert.h \
	ep_b64.h \
	ep_conf.h \
	ep_crypto.h \
	ep_dbg.h \
	ep_funclist.h \
	ep_hash.h \
	ep_hexdump.h \
	ep_log.h \
	ep_mem.h \
	ep_net.h \
	ep_pcvt.h \
	ep_prflags.h \
	ep_registry.h \
	ep_rpool.h \
	ep_stat.h \
	ep_statcodes.h \
	ep_string.h \
	ep_syslog.h \
	ep_thr.h \
	ep_time.h \
	ep_xlate.h \
	ep_version.h \

HFILES_NOTYET=	\
	ep_cdefs.h \
	ep_cfg.h \
	ep_conf.h \
	ep_crackargv.h \
	ep_ctype.h \
	ep_dbg_eplib.h \
	ep_easynames.h \
	ep_event.h \
	ep_gen.h \
	ep_netsock.h \
	ep_obj.h \
	ep_osdep_gen.h \
	ep_osdep_stream.h \
	ep_osdep_types.h \
	ep_pat.h \
	ep_plist.h \
	ep_posix.h \
	ep_prflags.h \
	ep_stream.h \
	ep_stream_type.h \
	ep_thr.h \
	ep_tree.h \
	ep_types.h \

BINALL= \
	decode-epstat

CC=	cc
PG=
O=	-O
WALL=	-Wall
STD=
CRYPTOCFLAGS=	-I${CRYPTOROOT}/include
CRYPTOLFLAGS=	-L${CRYPTOROOT}/lib
INCLS=		${CRYPTOCFLAGS}
COPTS=	${PG} -g ${WALL} $O ${STD}
CFLAGS=	${COPTS} ${INCLS} -I${CURDIR} -I.. -fPIC
RM=	rm
LD=	ld
LDFLAGS=	-L. ${CRYPTOLFLAGS}
LDLIBS=	libep.a -pthread
LIBS=	-lcrypto
LN=	ln
MV=	mv
CP=	cp
MKDIR=	mkdir -p
RANLIB=	ranlib
SHARED=	-shared ${CRYPTOLFLAGS}
#SHARED=	-shared ${CRYPTOLFLAGS} -Wl,-soname,$@
TIME=

EPLIBMAJVER=	2
EPLIBMINVER=	1
EPLIBVER=	${EPLIBMAJVER}.${EPLIBMINVER}
LIBALL=		libep.a libep.so.${EPLIBVER}
INCALL=		${HFILES}
ALL=		${LIBALL} ${INCALL} ${BINALL}

all: ${ALL}

libep.a: ${OBJS}
	${AR} -r $@ ${OBJS}
	${RANLIB} $@

libep.so.${EPLIBVER}: ${OBJS}
	${CC} ${SHARED} -o $@ ${OBJS} ${LIBS}

ep_version.o: ep_version.c ${OBJS_MOST} Makefile
	${CC} ${CFLAGS} -D_CURRENT_DATE_=\"`date +'%Y-%m-%d_%H:%M'`\" -c ep_version.c

#ep_stat.h: ep_status_codes.stat procstatus.py
#	-mv -f ep_stat.h ep_stat.h~
#	./procstatus.py < ep_status_codes.stat > ep_stat.h
#	chmod 444 ep_stat.h


#
#  Administrative stuff
#

clean:
	-${RM} -f ${BINALL} libep.* *.o *.core
	-${RM} -rf *.dSYM

install:	${ALLDIRS} ${PUBALL}
	${CP} ${INCALL} ${INCDIR}/ep
	${INSTALL} ${LIBALL} ${LIBDIR}
	(cd ${LIBDIR}; rm -f libep.so.${EPLIBMAJVER}; ln -s libep.so.${EPLIBVER} libep.so.${EPLIBMAJVER})
	(cd ${LIBDIR}; rm -f libep.so; ln -s libep.so.${EPLIBMAJVER} libep.so)
	(cd ${LIBDIR}; rm -f libep.${EPLIBVER}.dylib; ln -s libep.so.${EPLIBVER} libep.${EPLIBVER}.dylib)

${ALLDIRS}:
	${MKDIR} $@

${OBJS}: ${HFILES}

ADM=		../adm
UPDATE_LICENSE=	${ADM}/update-license.sh

update-license:
	${UPDATE_LICENSE} Makefile *.[ch]
