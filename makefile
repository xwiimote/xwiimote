#
# XWiimote - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

.PHONY: menu all driver lib tools clean

menu:
	@echo "Available Targets: all driver lib tools clean"

all: driver lib tools

driver:
	@cd driver && make

lib:
	@cd lib && make

tools:
	@cd tools && make

clean:
	@cd driver && make clean
	@cd lib && make clean
	@cd tools && make clean
