#
#  ----- BEGIN LICENSE BLOCK -----
#	GDP: Global Data Plane
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

# DESTDIR is just for staging.  LOCALROOT should be /usr or /usr/local.
DESTDIR=
LOCALROOT=	/usr
INSTALLROOT=	${DESTDIR}${LOCALROOT}
DOCDIR=		${INSTALLROOT}/share/doc/gdp

CTAGS=		ctags

all:
	(cd ep;		 make all)
	(cd gdp;	 make all)
	(cd scgilib;	 make all)
	(cd gdplogd;	 make all)
	(cd apps;	 make all)
	(cd examples;	 make all)

clean:
	(cd ep;		 make clean)
	(cd gdp;	 make clean)
	(cd scgilib;	 make clean)
	(cd gdplogd;	 make clean)
	(cd apps;	 make clean)
	(cd examples;	 make clean)

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
		apps/gdp-reader \
		apps/gdp-writer \
		gdplogd/gdplogd \

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

ADM=		adm
UPDATE_LICENSE=	${ADM}/update-license.sh

update-license:
	${UPDATE_LICENSE} Makefile *.[ch]
	(cd ep;		 make update-license)
	(cd gdp;	 make update-license)
	(cd gdplogd;	 make update-license)
	(cd apps;	 make update-license)
