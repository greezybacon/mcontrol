CC=gcc
CFLAGS=--std=gnu99 -Wall -D_GNU_SOURCE -pedantic -I..
LDFLAGS=-ldl -lrt -lrt -lpthread -lm

SOURCES=main.c controller.c scheduler.c trace.c worker.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=mcontrol

ifeq ($(DEBUG), 1)
CFLAGS += -g -pg
# Build static executable for profiling
OBJECTS += ../lib/*.o ../drivers/mdrive/*.o
# If DEBUG is enabled on a PIE system, add -nopie to the linker flags
PIE_SYSTEM=$(shell $(CC) -v 2>&1 | grep -- "--enable-esp" | wc -l)
ifeq ($(PIE_SYSTEM), 1)
LDFLAGS += -nopie
endif
else
LDFLAGS += -L../lib -lmcontrol
endif

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@

.PHONY: clean
clean:
	$(RM) $(OBJECTS)
	$(RM) $(EXECUTABLE)

