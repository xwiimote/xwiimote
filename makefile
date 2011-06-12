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
	@echo Making library
	@cd lib && make --no-print-directory
	@echo Library done

tools:
	@echo Making tools
	@cd tools && make --no-print-directory
	@echo Tools done

clean:
	@cd lib && make clean --no-print-directory
	@cd tools && make clean --no-print-directory
