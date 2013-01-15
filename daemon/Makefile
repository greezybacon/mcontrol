CC=gcc
CFLAGS=--std=gnu99 -Wall -D_GNU_SOURCE -pedantic -g -pg
LDFLAGS=-ldl -lrt -rdynamic -lrt -lpthread -lm -nopie

SOURCES=main.c controller.c scheduler.c worker.c
OBJECTS=$(SOURCES:.c=.o) ../lib/*.o ../drivers/mdrive/*.o
EXECUTABLE=mcontrol

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@

.PHONY: clean
clean:
	$(RM) $(OBJECTS)
	$(RM) $(EXECUTABLE)
