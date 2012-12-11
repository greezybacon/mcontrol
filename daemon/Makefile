CC=gcc
CFLAGS=--std=gnu99 -Wall -D_GNU_SOURCE -pedantic -pg
LDFLAGS=-ldl -lrt -rdynamic -lrt -nopie

SOURCES=main.c controller.c
OBJECTS=$(SOURCES:.c=.o) ../lib/*.o ../drivers/mdrive/*.o
EXECUTABLE=mcontrol

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@
