CTAGS=		ctags

DESTDIR=	/usr/local

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

install:
	(cd ep;		make install DESTDIR=${DESTDIR})
	(cd gdp;	make install DESTDIR=${DESTDIR})
	(cd gdplogd;	make install DESTDIR=${DESTDIR})
	(cd apps;	make install DESTDIR=${DESTDIR})

CSRCS=		ep/*.[ch] \
		gdp/*.[ch] \
		gdplogd/*.[ch] \
		scgilib/scgilib.[ch] \
		apps/*.[ch] \

tags:		.FORCE
	${CTAGS} ${CSRCS}

.FORCE:


# Build the Node.js/JavaScript GDP accessing apps and the Node.js/JS
# RESTful GDP interface.  Optional for the GDP per se.
all_JavaScript:
	(cd lang/js; make clean all)

clean_JavaScript:
	(cd lang/js; make clean)
