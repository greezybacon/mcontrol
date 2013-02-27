CC=gcc
CFLAGS=-I../../ --std=gnu99 -O2 -march=native -Wall -fPIC -D_GNU_SOURCE -pedantic 
LDFLAGS=-lrt -lm -lpthread -L../../lib -lmcontrol

SOURCES=driver.c serial.c queue.c config.c query.c motion.c search.c \
	events.c profile.c firmware.c microcode.c
OBJECTS=$(SOURCES:.c=.o)
LIBRARY=../mdrive.so

ifeq ($(DEBUG), 1)
CFLAGS += -g -pg
# If DEBUG is enabled on a PIE system, add -nopie to the linker flags
PIE_SYSTEM=$(shell $(CC) -v 2>&1 | grep -- "--enable-esp" | wc -l)
ifeq ($(PIE_SYSTEM), 1)
LDFLAGS += -nopie
endif
endif

.PHONY: all
all: $(SOURCES) $(LIBRARY)
	set -e; $(MAKE) -C microcode all

$(SOURCES): mdrive.h

$(LIBRARY): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -shared -o $@

.PHONY: clean
clean:
	$(RM) $(OBJECTS)
	set -e; $(MAKE) -C microcode clean
