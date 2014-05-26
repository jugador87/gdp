all:
	(cd ep; make clean all)
	(cd gdp; make clean all)
	(cd scgilib; make clean all)
	(cd apps; make clean all)

clean:
	(cd ep; make clean)
	(cd gdp; make clean)
	(cd scgilib; make clean)
	(cd apps; make clean)
