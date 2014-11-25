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

# External makefile targets below: all, clean, test


# Internal makefile variables

# Support for loading and building necessary Node.js JS modules
# npm - Node.js Package Manager, currently included in the node system
NPM  = npm
# directory for modules
NMOD = node_modules
# placeholder, not currently used
NODE = node
# Node modules necessary to run the JavaScript interface.
NODE_MODULES = $(NMOD)/ffi $(NMOD)/jsesc     $(NMOD)/posix-getopt \
               $(NMOD)/ref $(NMOD)/ref-array $(NMOD)/sleep


# External makefile targets: all, clean, test

all: $(NODE_MODULES)
	-rm libs/*
	(cd gdpjs; make clean all)
	(cd apps;  make clean all)
	(cd tests; make clean all)

clean:
	-rm libs/*
	-rm -fr -- $(NMOD)
	(cd gdpjs; make clean)
	(cd apps;  make clean)
	(cd tests; make clean)

test:
	(cd tests; make clean all run)

# TBD: Tag file builders exist for JavaScript - find one and include it.
tags:
	ctags gdpjs/*.[ch] apps/*.[ch]


# Internal makefile targets

$(NMOD)/ffi:
	$(NPM) install ffi
$(NMOD)/jsesc:
	$(NPM) install jsesc
$(NMOD)/posix-getopt:
	$(NPM) install posix-getopt
$(NMOD)/ref:
	$(NPM) install ref
$(NMOD)/ref-array:
	$(NPM) install ref-array
$(NMOD)/sleep:
	$(NPM) install sleep

