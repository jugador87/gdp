SOURCES=$(shell find . -name "*.dbk")
OBJECTS=$(patsubst %.dbk,%.fo,$(SOURCES))
TARGETS=$(patsubst %.fo,%.pdf,$(OBJECTS))
STYLESHEET=/usr/share/xml/docbook/stylesheet/docbook-xsl-ns/fo/docbook.xsl

all: $(TARGETS)

%.pdf: %.fo
	fop $^ -pdf $@

%.fo: %.dbk
	xsltproc -xinclude -o $@ $(STYLESHEET) $^

clean:
	rm -f *.pdf
	rm -f *.fo

.PHONY: clean
