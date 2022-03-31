SHELL = /usr/local/bin/bash

# compiler and flags
CC = cc
CXX = g++
# FLAGS = -Wall -O2 -fvariable-expansion-in-unroller -ftree-loop-ivcanon -funroll-loops -fexpensive-optimizations -fomit-frame-pointer
FLAGS = -O2 -Wall -Werror -std=gnu99 -march=native 
CFLAGS = $(FLAGS)
CXXFLAGS = $(CFLAGS)
LDFLAGS = -lcam -ldevstat -lkvm -lm -lpthread
RCPREFIX = /usr/local/etc/rc.d
PREFIX = /usr/local
RCFILE = hpex49xled.rc
CFILES = hpex49xled_run.c hpex49xled_led.c
OBJS = hpex49xled_run.o hpex49xled_led.o
TARGETS = hpex49xled


# build libraries and options

all: clean $(OBJS) $(TARGETS)

${OBJS}: ${CFILES}
	${CC} ${CFLAGS} ${INCLUDES} -c $?

${TARGETS}: ${OBJS}
	${CC} -o $@ $? ${CFLAGS} ${LDFLAGS}

camtest: camtest.c
	${CC} -o $@ $? ${CFLAGS} ${LDFLAGS}

.PHONY: clean

clean:
	rm -f *.o hpex49xled *.core 

.PHONY: install

install: all
	test -f $(RCPREFIX)/hpex49xled || install -m 755 $(RCFILE) $(RCPREFIX)/hpex49xled
	install -s -m 700 $(TARGETS) $(PREFIX)/bin/
