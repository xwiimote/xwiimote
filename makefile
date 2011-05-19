#
# XWiimote - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

.PHONY: menu all lib tools clean

menu:
	@echo "Available Targets: all lib tools clean"

all: lib tools

lib:
	@cd lib && make

tools:
	@cd tools && make

clean:
	@cd lib && make clean
	@cd tools && make clean
