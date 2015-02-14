ALL= \
		gdp-gcl-metadata.html \
		gdp-programmatic-api.html \
		gdp-protocol.html \
		gdp-rest-interface.html \

DESTDIR=
LOCALROOT=	/usr
INSTALLROOT=	${DESTDIR}${LOCALDIR}
DOCDIR=		${INSTALLROOT}/share/doc/gdp

all: ${ALL}
	echo "Nothing to do"

clean:
	echo "Nothing to do"

${DOCDIR}:
	mkdir -p $@

install: ${DOCDIR}
	cp ${ALL} ${DOCDIR}
