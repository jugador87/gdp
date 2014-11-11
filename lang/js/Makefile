#
#  Makefile for gdp/lang/js/  (based on Makefile for gdp/)
#
#	$Id$
#
#	Copyright (c) 2014, <TBD>.  All rights reserved.
#

# Alec Dara-Abrams
# 2014-11-04
#

all:
	-rm libs/*
	(cd gdpjs; make clean all)
	(cd apps;  make clean all)
	(cd tests; make clean all)

clean:
	-rm libs/*
	(cd gdpjs; make clean)
	(cd apps;  make clean)
	(cd tests; make clean)

test:
	(cd tests; make clean all run)

# TBD: Tag file builders exist for JavaScript - find one and include.
tags:
	ctags gdpjs/*.[ch] apps/*.[ch]
