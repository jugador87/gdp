#
#  Makefile for libep.
#
#	$Id: Makefile 286 2014-04-29 18:15:22Z eric $
#
#	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
#

CURDIR=	.

OBJS=	\
	ep_adm.o \
	ep_app.o \
	ep_assert.o \
	ep_b64.o \
	ep_dbg.o \
	ep_fopensmem.o \
	ep_fread_unlocked.o \
	ep_funclist.o \
	ep_hash.o \
	ep_hexdump.o \
	ep_mem.o \
	ep_pcvt.o \
	ep_pprint.o \
	ep_prflags.o \
	ep_rpool.o \
	ep_stat.o \
	ep_string.o \
	ep_time.o \
	ep_xlate.o \
	ep_version.o \

OBJS_NOTYET=	\
	ep_obj.o \
	ep_cfg.o \
	ep_crackargv.o \
	ep_init.o \
	ep_log.o \
	ep_net_cvtaddr.o \
	ep_pat.o \
	ep_plist.o \
	ep_prflags.o \
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
	ep_dbg.h \
	ep_funclist.h \
	ep_hash.h \
	ep_hexdump.h \
	ep_mem.h \
	ep_pcvt.h \
	ep_prflags.h \
	ep_registry.h \
	ep_rpool.h \
	ep_stat.h \
	ep_string.h \
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
	ep_log.h \
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

CC=	cc
PG=
O=	-O
WALL=	-Wall
COPTS=	${PG} -g ${WALL} $O -std=c99
CFLAGS=	${COPTS} ${INCLS} -I${CURDIR} -I..
RM=	rm
LDFLAGS=-L.
LDLIBS=	libep.a
LN=	ln
RANLIB=	ranlib
TIME=

ALL=	libep.a

all: ${ALL}

libep.a: ${OBJS}
	${AR} -r $@ ${OBJS}
	${RANLIB} $@

ep_version.o: ep_version.c FORCE
	${CC} ${CFLAGS} -D_CURRENT_DATE_=\"`date +'%Y-%m-%d_%H:%M'`\" -c ep_version.c

#ep_stat.h: ep_status_codes.stat procstatus.py
#	-mv -f ep_stat.h ep_stat.h~
#	./procstatus.py < ep_status_codes.stat > ep_stat.h
#	chmod 444 ep_stat.h

FORCE:

#
#  Administrative stuff
#

clean:
	${RM} -f ${ALL} *.o *.core

${OBJS}: ${HFILES}
