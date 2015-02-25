#
#  Makefile for gdp/lang/js/  (based on gdp/Makefile)
#
#	$Id$
#
#	Copyright (c) 2014, <TBD>.  All rights reserved.
#

# Alec Dara-Abrams
# 2014-11-04
#
# TBD: Copyrights, more complete one-button tests
#
# Caveats:
#
#    This Makefile assumes that it is located in gdp/lang/js/ ,
#    where gdp/ is a GDP Git local repository.
#
#    It will not force a re-build of the system up in gdp/ .
#    gdp/ must be re-built and kept up to date separately.
#
#    It invokes corresponding builds in sub-directories gdpjs/, apps/,
#    and tests/ .
#
#    Products of the build:
#      1) See sub-directories gdpjs/, apps/, and tests/ for their build products
#      2) a sub-directory, node_modules, is managed here.  It contains Node.js
#         packages needed by Node.js JS code in gdpjs/ apps/ and tests/ .
#         These packages are likely platform-specific and need to be installed
#         and build dynamically.


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
               $(NMOD)/ref $(NMOD)/ref-array $(NMOD)/ref-struct \
               $(NMOD)/sleep


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
tags: .FORCE
	ctags `find . -name '*.[ch]' -o -name '*.js' -o -name '*.cc'`

.FORCE:

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
$(NMOD)/ref-struct:
	$(NPM) install ref-struct
$(NMOD)/sleep:
	$(NPM) install sleep
