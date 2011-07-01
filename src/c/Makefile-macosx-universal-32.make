# Copyright (c) 1999, 2011 Tanuki Software, Ltd.
# http://www.tanukisoftware.com
# All rights reserved.
#
# This software is the proprietary information of Tanuki Software.
# You shall use it only in accordance with the terms of the
# license agreement you entered into with Tanuki Software.
# http://wrapper.tanukisoftware.com/doc/english/licenseOverview.html

UNIVERSAL_SDK_HOME=/Developer/SDKs/MacOSX10.5.sdk
INCLUDE = -I/opt/local/include 
COMPILE = gcc -O3 -Wall -DUSE_NANOSLEEP -DMACOSX -arch ppc -arch i386 -isysroot $(UNIVERSAL_SDK_HOME) $(INCLUDE) -mmacosx-version-min=10.4 -DUNICODE -D_UNICODE
COMPILET = gcc -O3 -Wall -DUSE_NANOSLEEP -DMACOSX -isysroot $(UNIVERSAL_SDK_HOME) $(INCLUDE) -mmacosx-version-min=10.4 -DUNICODE -D_UNICODE
#COMPILE = gcc -ggdb -O1 -Wall -DUSE_NANOSLEEP -DMACOSX -DVALGRIND -isysroot $(UNIVERSAL_SDK_HOME) $(INCLUDE) -mmacosx-version-min=10.4 -DUNICODE -D_UNICODE
# To debug:
# 1) Add "-ggdb"
# 2) Remove "-arch ppc -arch i386"
# 3) Change "-O3" to "-O1"

DEFS = -I$(UNIVERSAL_SDK_HOME)/System/Library/Frameworks/JavaVM.framework/Headers

wrapper_SOURCE = wrapper.c wrapperinfo.c wrappereventloop.c wrapper_unix.c property.c logger.c wrapper_file.c wrapper_i18n.c test.c

libwrapper_so_OBJECTS = wrapper_i18n.o wrapperjni_unix.o wrapperinfo.o wrapperjni.o

BIN = ../../bin
LIB = ../../lib
TEST = ../../test

all: init testsuite wrapper libwrapper.jnilib

clean:
	rm -f *.o

cleanall: clean
	rm -rf *~ .deps
	rm -f $(BIN)/wrapper $(LIB)/libwrapper.jnilib

init:
	if test ! -d .deps; then mkdir .deps; fi

wrapper: $(wrapper_SOURCE)
	$(COMPILE) -DMACOSX $(wrapper_SOURCE) -liconv -o $(BIN)/wrapper

libwrapper.jnilib: $(libwrapper_so_OBJECTS)
	$(COMPILE) -bundle -liconv -o $(LIB)/libwrapper.jnilib $(libwrapper_so_OBJECTS)

testsuite: $(wrapper_SOURCE)
	$(COMPILET) -DCUNIT $(wrapper_SOURCE) -liconv -lncurses -lcunit -o $(TEST)/testsuite

%.o: %.c
	$(COMPILE) -c $(DEFS) $<

