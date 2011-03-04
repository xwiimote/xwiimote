#
# XWiimote - driver - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

CC=gcc
CFLAGS=-c -fPIC -Wall -pedantic -Iinclude -I../lib/include --std=c99 -D_POSIX_C_SOURCE=200809L
LD=gcc
LFLAGS=-Wall -pedantic -lbluetooth -lrt

BINARY=xwiimote
OBJECTS=src/proto.o src/log.o src/main.o src/bluetooth.o src/driver.o

.PHONY: all info clean

all: info $(BINARY)

info:
	@echo 'Building XWiimote Driver...'

src/%.o: src/%.c include/*.h
	$(CC) $(CFLAGS) -o $@ $<

$(BINARY): $(OBJECTS)
	$(CC) $(LFLAGS) -o $@ $(OBJECTS)

clean:
	@rm -rfv src/*.o $(BINARY)
