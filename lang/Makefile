#
#  Makefile for other (non-C) language bindings
#
#  	All based on SWIG
#
#

ALL=		python javascript-node

BINDLANG=	sh bindlang.sh
DEPS=		Makefile.m4 gdp.i

python:	${DEPS}
	rm -rf $@
	${BINDLANG} $@

javascript-node:	${DEPS}
	rm -rf $@
	${BINDLANG} $@

clean:
	rm -rf ${ALL}
