#
# XWiimote - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

.PHONY: all driver lib clean

all:
	@echo "Available Targets: driver lib"

driver:
	@cd driver; make

lib:
	@cd lib; make

clean:
	@cd driver; make clean
	@cd lib; make clean
