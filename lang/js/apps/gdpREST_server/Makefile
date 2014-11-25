#
# Draft Makefile for gdpREST_server
# 2014-11-06
# Alec Dara-Abrams
#

# External makefile targets below: all, clean


# Internal makefile variables

# Support for loading and building necessary Node.js JS modules
# npm - Node.js Package Manager, currently included in the node system
NPM  = npm
# directory for modules
NMOD = node_modules
# placeholder, not currently used
NODE = node

all: install
	@echo 'Nothing to make here - only JavaScript programs.'
	@echo 'Just a node_modules directory to do an npm install into.'
	@echo 'We use package.json to drive the install.'

clean:
	-rm -fr -- $(NMOD)

install: $(NMOD)

$(NMOD): package.json
	$(NPM) install

