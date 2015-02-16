# DESTDIR is just for staging.  LOCALROOT should be /usr or /usr/local.
DESTDIR=
LOCALROOT=	/usr
INSTALLROOT=	${DESTDIR}${LOCALROOT}
DOCDIR=		${INSTALLROOT}/share/doc/gdp

CTAGS=		ctags

all:
	-rm libs/*
	(cd ep;		 make all)
	(cd gdp;	 make all)
	(cd scgilib;	 make all)
	(cd gdplogd;	 make all)
	(cd apps;	 make all)

clean:
	-rm libs/*
	(cd ep;		 make clean)
	(cd gdp;	 make clean)
	(cd scgilib;	 make clean)
	(cd gdplogd;	 make clean)
	(cd apps;	 make clean)

install: ${INSTALLROOT}/etc/ep_adm_params
	(cd ep;		make install DESTDIR=${DESTDIR} INSTALLROOT=${INSTALLROOT})
	(cd gdp;	make install DESTDIR=${DESTDIR} INSTALLROOT=${INSTALLROOT})
	(cd gdplogd;	make install DESTDIR=${DESTDIR} INSTALLROOT=${INSTALLROOT})
	(cd apps;	make install DESTDIR=${DESTDIR} INSTALLROOT=${INSTALLROOT})
	(cd doc;	make install DESTDIR=${DESTDIR} INSTALLROOT=${INSTALLROOT})
	mkdir -p ${DOCDIR}
	cp -rp examples ${DOCDIR}

install-etc: ${INSTALLROOT}/etc/ep_adm_params/gdp

${INSTALLROOT}/etc/ep_adm_params/gdp: ${INSTALLROOT}/etc/ep_adm_params
	echo "# Configuration parameters for the Global Data Plane" > $@

${INSTALLROOT}/etc/ep_adm_params:
	mkdir -p $@

GDPROOT=	~gdp
GDPALL=		adm/start-* \
		adm/run-* \
		apps/gcl-create \
		apps/gdp-rest \
		apps/reader-test \
		apps/writer-test \
		gdplogd/gdplogd \
		gdp_router/src/gdp_router.py \

init-gdp:
	sudo -u gdp adm/init-gdp.sh
	sudo -u gdp cp ${GDPALL} ${GDPROOT}/bin/.

CSRCS=		ep/*.[ch] \
		gdp/*.[ch] \
		gdplogd/*.[ch] \
		scgilib/scgilib.[ch] \
		apps/*.[ch] \

tags: .FORCE
	${CTAGS} ${CSRCS}

.FORCE:


# Build the Node.js/JavaScript GDP accessing apps and the Node.js/JS
# RESTful GDP interface.  Optional for the GDP per se.
all_JavaScript:
	(cd lang/js; make clean all)

clean_JavaScript:
	(cd lang/js; make clean)

# Build the debian-style package.  Must be done on the oldest system
# around because of dependencies.

VER=		XX
debian-package:
	@[ "${VER}" = "XX" ] && ( echo "Must include VER=<version>"; exit 1 )
	adm/gdp-debbuild.sh -v ${VER} -r ..
