CC=gcc
CFLAGS=-I../../ --std=gnu99 -O2 -march=native -Wall -fPIC -D_GNU_SOURCE -pedantic -g -pg
LDFLAGS=-lrt -lm -lpthread

SOURCES=driver.c serial.c queue.c config.c query.c motion.c search.c \
	events.c profile.c firmware.c microcode.c
OBJECTS=$(SOURCES:.c=.o)
LIBRARY=mdrive.so

all: $(SOURCES) $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -shared -o $@
