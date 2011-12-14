#
# $Id$
#
# Copyright 2011, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libjuise juise juise-askpass extensions doc

if HAVE_LIGHTTPD
SUBDIRS += mod_juise web
endif

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

USER_INFO := $(shell if [ -f .upload ]; then cat .upload; fi )

UPLOAD = python ~/bin/googlecode_upload.py -p juise ${USER_INFO}
upload: dist
	@echo "Uploading juise-${PACKAGE_VERSION}.tar.gz ... "
	@${UPLOAD} -s 'Release ${PACKAGE_VERSION}' \
		-l Type-Source,OpSys-All,Featured \
		juise-${PACKAGE_VERSION}.tar.gz
	@echo "Uploading juise-manual-${PACKAGE_VERSION}.html ... "
	@${UPLOAD} -s 'Full Documentation' -l Type-Docs,Featured \
		doc/juise-manual-${PACKAGE_VERSION}.html
