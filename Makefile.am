#
# $Id$
#

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libjuise juise
bin_SCRIPTS=juise-config
dist_doc_DATA = Copyright README INSTALL

EXTRA_DIST = juise-config.in warnings.mk

.PHONY: test tests

test tests:
	@(cd tests ; ${MAKE} tests)

errors:
	@(cd tests/errors ; ${MAKE} test)

docs:
	@(cd doc ; ${MAKE} docs)
