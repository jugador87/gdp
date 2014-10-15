all:
	-rm libs/*
	(cd ep; make clean all)
	(cd gdp; make clean all)
	(cd scgilib; make clean all)
	(cd gdpd; make clean all)
	(cd apps; make clean all)

clean:
	-rm libs/*
	(cd ep; make clean)
	(cd gdp; make clean)
	(cd scgilib; make clean)
	(cd gdpd; make clean)
	(cd apps; make clean)

tags:
	ctags ep/*.[ch] gdp/*.[ch] gdpd/*.[ch] scgilib/scgilib.[ch] apps/*.[ch]
