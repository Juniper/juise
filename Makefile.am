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

SUBDIRS = libjuise juise juise-askpass extensions doc import

if NEED_MIXER
SUBDIRS += mixer
endif

if HAVE_LIGHTTPD
SUBDIRS += mod_juise web
endif

bin_SCRIPTS=juise-config
dist_doc_DATA = Copyright

EXTRA_DIST = juise-config.in warnings.mk README.md INSTALL

.PHONY: test tests

test tests:
	@(cd tests ; ${MAKE} tests)

errors:
	@(cd tests/errors ; ${MAKE} test)

docs:
	@(cd doc ; ${MAKE} docs)

USER_INFO := $(shell if [ -f .upload ]; then cat .upload; fi )

UPLOAD = python ~/bin/googlecode_upload.py -p juise ${USER_INFO}
upload-google: dist
	@echo "Uploading juise-${PACKAGE_VERSION}.tar.gz ... "
	@${UPLOAD} -s 'Release ${PACKAGE_VERSION}' \
		-l Type-Source,OpSys-All,Featured \
		juise-${PACKAGE_VERSION}.tar.gz
	@echo "Uploading juise-manual-${PACKAGE_VERSION}.html ... "
	@${UPLOAD} -s 'Full Documentation' -l Type-Docs,Featured \
		doc/juise-manual-${PACKAGE_VERSION}.html

DIST_FILES_DIR = ~/Dropbox/dist-files/
GH_PAGES_DIR = gh-pages/
upload-dropbox: dist
	@echo "Uploading juise-${PACKAGE_VERSION}.tar.gz ... "
	@cp juise-${PACKAGE_VERSION}.tar.gz ${DIST_FILES_DIR}
	@echo "Uploading juise-manual.html ... "
	@cp doc/juise-manual.html ${DIST_FILES_DIR}
	@-[ -d ${GH_PAGES_DIR} ] \
		&& cp doc/juise-manual.html ${GH_PAGES_DIR} \
		&& (cd ${GH_PAGES_DIR} \
			&& git commit -m 'new docs' juise-manual.html \
			&& git push origin gh-pages )

upload: dist
	@-[ -d ${GH_PAGES_DIR} ] \
		&& echo "Updating manual on gh-pages ..." \
		&& cp doc/juise-manual.html ${GH_PAGES_DIR} \
		&& (cd ${GH_PAGES_DIR} \
			&& git commit -m 'new docs' juise-manual.html \
			&& git push origin gh-pages )
	@echo "Remember to run: gt tag ${PACKAGE_VERSION}"

get-wiki:
	git clone https://github.com/Juniper/juise.wiki.git wiki

get-gh-pages:
	git clone https://github.com/Juniper/juise.git gh-pages -b gh-pages
