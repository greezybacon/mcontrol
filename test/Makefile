CC=gcc
CFLAGS=--std=c99 -Wall -g
LDFLAGS= -lrt -L../lib -lmcontrol

SOURCES=test-connect.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=test-connect

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@
