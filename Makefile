# Makefile

# Normal build will link against the shared library for simavr
# in the current build tree, so you don't have to 'install' to
# run simavr or the examples.
#
# For package building, you will need to pass RELEASE=1 to make
RELEASE	?= 0

PREFIX = ${DESTDIR}

.PHONY: doc

all:	build-simavr

build-simavr:
	$(MAKE) -C simavr RELEASE=$(RELEASE)

clean:
	$(MAKE) -C simavr clean


