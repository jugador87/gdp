all:
	-rm libs/*
	(cd gdpjs; make clean all)
	(cd apps;  make clean all)

clean:
	-rm libs/*
	(cd gdpjs; make clean)
	(cd apps;  make clean)

tags:
	ctags gdpjs/*.[ch] apps/*.[ch]
