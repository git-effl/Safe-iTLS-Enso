#
# Safe-iTLS-Enso Root Project Makefile
# Builds both:
#  1. plugin (itls.skprx - Kernel Redirection Plugin)
#  2. companion-app (Safe-iTLS-Companion.vpk - Hardware Extractor)
#

.PHONY: all plugin companion clean

all: plugin companion

plugin:
	$(MAKE) -C plugin

companion:
	$(MAKE) -C companion-app

clean:
	$(MAKE) -C plugin clean
	$(MAKE) -C companion-app clean
