#
# XWiimote - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

.PHONY: menu all driver lib clean

menu:
	@echo "Available Targets: all driver lib"

all: driver lib

driver:
	@cd driver; make

lib:
	@cd lib; make

clean:
	@cd driver; make clean
	@cd lib; make clean
