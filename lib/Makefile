CC=gcc
CFLAGS=--std=gnu99 -Wall -fPIC -D_GNU_SOURCE -pedantic
LDFLAGS=-lrt -ldl -lpthread
DEBUG=1

SOURCES=motor.c message.c driver.c query.c dispatch.c client.c locks.c \
	motion.c events.c profile.c trace.c firmware.c callback.c
HEADERS=query.h locks.h motor.h motion.h events.h profile.h trace.h firmware.h
OBJECTS=$(SOURCES:.c=.o)
LIBRARY=libmcontrol.so

DIRS=python

ifeq ($(DEBUG), 1)
CFLAGS += -g -pg
# If DEBUG is enabled on a PIE system, add -nopie to the linker flags
PIE_SYSTEM=$(shell $(CC) -v 2>&1 | grep -- "--enable-esp" | wc -l)
ifeq ($(PIE_SYSTEM), 1)
LDFLAGS += -nopie
endif
endif

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
