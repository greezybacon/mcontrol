CC=gcc
CFLAGS=--std=gnu99 -Wall -fPIC -D_GNU_SOURCE -pedantic -g -pg
LDFLAGS=-lrt -ldl -nopie

SOURCES=motor.c message.c driver.c query.c dispatch.c client.c locks.c \
	motion.c events.c profile.c trace.c firmware.c callback.c
HEADERS=query.h locks.h motor.h motion.h events.h profile.h trace.h firmware.h
OBJECTS=$(SOURCES:.c=.o)
LIBRARY=libmcontrol.so

DIRS=python

.PHONY: all
all: $(HEADERS) dispatch.h client.h $(SOURCES) $(LIBRARY)
	set -e; for d in $(DIRS); do $(MAKE) -C $$d all; done

client.c: $(HEADERS) client.c.py
	python client.c.py $(HEADERS) > client.c

client.h: $(HEADERS) client.h.py
	python client.h.py $(HEADERS) > client.h

dispatch.h: $(HEADERS) dispatch.h.py
	python dispatch.h.py $(HEADERS) > dispatch.h

$(LIBRARY): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -shared -o $@

.PHONY: clean
clean:
	$(RM) $(OBJECTS)
	$(RM) $(LIBRARY)
	set -e; for d in $(DIRS); do $(MAKE) -C $$d clean; done
