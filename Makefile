all:
	-rm libs/*
	(cd ep; make clean all)
	(cd gdp; make clean all)
	(cd scgilib; make clean all)
	(cd gdplogd; make clean all)
	(cd apps; make clean all)

clean:
	-rm libs/*
	(cd ep; make clean)
	(cd gdp; make clean)
	(cd scgilib; make clean)
	(cd gdplogd; make clean)
	(cd apps; make clean)

CSRCS=		ep/*.[ch] \
		gdp/*.[ch] \
		gdplogd/*.[ch] \
		scgilib/scgilib.[ch] \
		apps/*.[ch] \

tags:	.FORCE
	ctags ${CSRCS}

.FORCE:


# Build the Node.js/JavaScript GDP accessing apps and the Node.js/JS
# RESTful GDP interface.  Optional for the GDP per se.
all_JavaScript:
	(cd lang/js; make clean all)

clean_JavaScript:
	(cd lang/js; make clean)
