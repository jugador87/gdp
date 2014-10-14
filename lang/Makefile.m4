`#
#  Makefile for building '_LANG_` binding for the GDP
#
#	This is auto-generated from ../Makefile.m4.  Do not update it!
#'
divert(-1)
ifdef(`_XTRA_INCLUDES_',, `define(`_XTRA_INCLUDES_', `')')

# tweaks based on language
ifelse(
    _LANG_, `python',
	`define(`_XTRA_INCLUDES_', `-I/usr/local/include/python2.7')',
    _LANG_, `javascript-jsc',
	`define(`_LANG_', `javascript -jsc')',
    _LANG_, `javascript-v8',
    	`define(`_LANG_', `javascript -v8')',
    _LANG_, `javascript-node',
    	`define(`_LANG_', `javascript -node')'
	`define(`_XTRA_INCLUDES_', `-I${OPTINC}/node')'
    _LANG_, `_LANG_',
	`errprint(`Must define _LANG_!
')',
    `errprint(Unrecognized language _LANG_
)')

# tweaks based on OS environment
define(`_OPT_', `/usr/local')
divert(0)

LANG=		_LANG_
GDPROOT=	ifdef(`_GDPROOT_', `_GDPROOT_', `../..')
GDPLIBROOT=	ifdef(`_GDPLIBROOT_', `_GDPLIBROOT_', `${GDPROOT}')
GDPINCROOT=	ifdef(`_GDPINCROOT_', `_GDPINCROOT_', `${GDPROOT}')
XTRA_INCLUDES=	_XTRA_INCLUDES_
SWIG=		ifdef(`_SWIG_', `_SWIG_', `swig')
SWIG_ARGS=	${LANG}
CFLAGS=		-fPIC
OPT=		_OPT_
OPTINC=		${OPT}/include
OPTLIB=		${OPT}/lib

gdp.so:	gdp_wrapper.o
	${CC} ${CFLAGS} -shared -L${GDPLIBROOT}/libs -o $@ gdp_wrapper.o

gdp_wrapper.o:	gdp_wrapper.c
	${CC} ${CFLAGS} -c -I${GDPINCROOT} ${XTRA_INCLUDES} -o $@ gdp_wrapper.c

gdp_wrapper.c: ../gdp.i
	${SWIG} -${SWIG_ARGS} -outdir . -o $@ ../gdp.i
