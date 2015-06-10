# Makefile org.terraswarm Global Data Plane sources
#
# @Version: $Id: makefile 72425 2015-05-27 21:01:46Z cxh $
# @Author: Christopher Brooks
#
# @Copyright (c) 2014 The Regents of the University of California.
# All rights reserved.
#
# Permission is hereby granted, without written agreement and without
# license or royalty fees, to use, copy, modify, and distribute this
# software and its documentation for any purpose, provided that the
# above copyright notice and the following two paragraphs appear in all
# copies of this software.
#
# IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY
# FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
# ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
# THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE
# PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY OF
# CALIFORNIA HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
# ENHANCEMENTS, OR MODIFICATIONS.
#
# 						PT_COPYRIGHT_VERSION_2
# 						COPYRIGHTENDKEY
##########################################################################

all: jclass

##############################
JNA_JAR = jna-4.1.0.jar
JNAERATOR_VERSION = 0.12
JNAERATOR_JAR = jnaerator-$(JNAERATOR_VERSION)-shaded.jar
JNAERATOR_RUNTIME_JAR = jnaerator-runtime-$(JNAERATOR_VERSION).jar

### Download required files. If there is an updated version of these, it needs
### to be manually updated.

$(JNA_JAR):
	wget 'https://maven.java.net/content/repositories/releases/net/java/dev/jna/jna/4.1.0/$(JNA_JAR)' -O $@

$(JNAERATOR_JAR):
	wget 'https://search.maven.org/remotecontent?filepath=com/nativelibs4java/jnaerator/$(JNAERATOR_VERSION)/$(JNAERATOR_JAR)' -O $@

$(JNAERATOR_RUNTIME_JAR):
	wget 'https://search.maven.org/remotecontent?filepath=com/nativelibs4java/jnaerator-runtime/$(JNAERATOR_VERSION)/$(JNAERATOR_RUNTIME_JAR)' -O $@

##############################

## Header files that we need to interpret the compiled library

ROOT = ../../
GDP_SRC = $(ROOT)
PWD = $(ROOT)/lang/java/

GDP_H_FILES = \
	$(GDP_SRC)/gdp/gdp.h \
	$(GDP_SRC)/gdp/gdp_buf.h \
	$(GDP_SRC)/ep/ep.h \
    $(GDP_SRC)/ep/ep_stat.h \
    $(GDP_SRC)/ep/ep_time.h \
	$(GDP_SRC)/ep/ep_dbg.h \
	$(GDP_SRC)/ep/ep_thr.h

LIBEVENT_H_FILES = \
	/usr/include/event2/buffer.h

### Auto-generating java files

# We define gdp_datum_t and gdp_datum as void * so that JNAerator does not generate gdp_datum.java, which would refer to pthreads.
jna: clean $(JNAERATOR_JAR) $(JNAERATOR_RUNTIME_JAR) $(GDP_H_FILES) $(LIBEVENT_H_FILES)
	java -jar $(JNAERATOR_JAR) -runtime JNA \
		-Dgdp_datum_t="void *" -Dgdp_datum="void *" \
		-library gdp.0.2 $(GDP_H_FILES) \
		-library event2 $(LIBEVENT_H_FILES) \
		-package org.terraswarm.gdp  -v -o . -noJar -noComp
	# Fix up some issues:
	for file in org/terraswarm/gdp/*.java; do \
	echo "Fixing $$file"; \
	sed \
	    -e 's@public static final Pointer EP_GEN_DEADBEEF = (Pointer)0xDEADBEEF;@public static final Pointer EP_GEN_DEADBEEF = new Pointer(0xDEADBEEF); // Fixed by cxh in makefile.@' \
	    -e 's@\(public static final GlobalInt\)@// \1 // Fixed by cxh in makefile.@' \
		$$file > /tmp/gdp.fix; \
	mv /tmp/gdp.fix $$file; \
        done


### Compiling .java files to .class files

JSRCS_LIB = $(wildcard org/terraswarm/gdp/*.java)
JSRCS_APPS = $(wildcard org/terraswarm/gdp/apps/*.java)
JSRCS = $(JSRCS_LIB) $(JSRCS_APPS)

# Java rules
CLASSPATH = $(JNA_JAR):$(JNAERATOR_RUNTIME_JAR):$(PWD)

JAVAC = javac

.SUFFIXES: .class .java
.java.class:
	rm -f `basename $< .java`.class
	CLASSPATH="$(CLASSPATH):$(PWD)" "$(JAVAC)" $(JFLAGS) $<

JCLASS = $(JSRCS:%.java=%.class)

jclass: jna $(JNA_JAR) $(JSRCS) $(JCLASS)


### apps

JAVA = java
LOG_NAME = edu.berkeley.eecs.mor.01
ROUTER_NAME = gdp-01.eecs.berkeley.edu:8007


runReaderTest: org/terraswarm/gdp/apps/ReaderTest.class
	DYLD_LIBRARY_PATH=$(GDP_SRC)/libs LD_LIBRARY_PATH=$(GDP_SRC)/libs $(JAVA) -Djna.debug_load=true -Djna.dump_memory=true -classpath $(CLASSPATH) org.terraswarm.gdp.apps.ReaderTest -G $(ROUTER_NAME) -D gdp.api=100 $(LOG_NAME)

runReaderTestPtIILib: org/terraswarm/gdp/apps/ReaderTest.class
	DYLD_LIBRARY_PATH=$(GDP_SRC)/libs LD_LIBRARY_PATH=$(GDP_SRC)/libs $(JAVA) -Djna.debug_load=true -Djna.dump_memory=true -classpath $(CLASSPATH) org.terraswarm.gdp.apps.ReaderTest -G $(ROUTER_NAME) -D gdp.api=100 $(LOG_NAME)

runReaderTestSubscriber: org/terraswarm/gdp/apps/ReaderTest.class
	DYLD_LIBRARY_PATH=$(GDP_SRC)/libs LD_LIBRARY_PATH=$(GDP_SRC)/libs $(JAVA) -Djna.debug_load=true -Djna.dump_memory=true -classpath $(CLASSPATH) org.terraswarm.gdp.apps.ReaderTest -G $(ROUTER_NAME) -s -D gdp.api=100 $(LOG_NAME)

runWriterTest: org/terraswarm/gdp/apps/WriterTest.class
	DYLD_LIBRARY_PATH=$(GDP_SRC)/libs LD_LIBRARY_PATH=$(GDP_SRC)/libs $(JAVA) -Djna.debug_load=true -Djna.dump_memory=true -classpath $(CLASSPATH) org.terraswarm.gdp.apps.WriterTest -G $(ROUTER_NAME) -D gdp.api=100 $(LOG_NAME)


### 

clean:
	rm -f _* org/terraswarm/gdp/*.class org/terraswarm/gdp/EP* org/terraswarm/gdp/e* org/terraswarm/gdp/*Library.java org/terraswarm/gdp/apps/*.class

distclean: clean
	rm -f *.jar
