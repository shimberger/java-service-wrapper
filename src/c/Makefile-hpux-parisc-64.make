# Copyright (c) 1999, 2011 Tanuki Software, Ltd.
# http://www.tanukisoftware.com
# All rights reserved.
#
# This software is the proprietary information of Tanuki Software.
# You shall use it only in accordance with the terms of the
# license agreement you entered into with Tanuki Software.
# http://wrapper.tanukisoftware.com/doc/english/licenseOverview.html

COMPILE = cc +z +DD64 -D_INCLUDE__STDC_A1_SOURCE -O3 -Wall -DHPUX -DJSW64 -DUNICODE -D_UNICODE


INCLUDE=$(JAVA_HOME)/include

DEFS = -I$(INCLUDE) -I$(INCLUDE)/hp-ux

wrapper_SOURCE = wrapper.c wrapperinfo.c wrappereventloop.c wrapper_unix.c property.c logger.c wrapper_i18n.c wrapper_file.c

libwrapper_sl_SOURCE = wrapper_i18n.c wrapperjni_unix.c wrapperinfo.c wrapperjni.c

BIN = ../../bin
LIB = ../../lib

all: init wrapper libwrapper.sl

clean:
	rm -f *.o

cleanall: clean
	rm -rf *~ .deps
	rm -f $(BIN)/wrapper $(LIB)/libwrapper.sl

init:
	if test ! -d .deps; then mkdir .deps; fi

wrapper: $(wrapper_SOURCE)
	$(COMPILE) $(wrapper_SOURCE) -lm -lpthread -o $(BIN)/wrapper

libwrapper.sl: $(libwrapper_sl_SOURCE)
	${COMPILE} ${DEFS} $(libwrapper_sl_SOURCE) -b -lm -lpthread -o $(LIB)/libwrapper.sl

%.o: %.c
	${COMPILE} -c ${DEFS} $<
