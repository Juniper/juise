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

SUBDIRS = libjuise juise juise-askpass extensions doc import tests

if NEED_MIXER
SUBDIRS += mixer
endif

if HAVE_LIGHTTPD
SUBDIRS += mod_juise web
endif

bin_SCRIPTS=juise-config
dist_doc_DATA = Copyright

EXTRA_DIST = \
    juise-config.in \
    warnings.mk \
    README.md \
    INSTALL \
    bin/setup.sh \
    packaging/juise.spec

.PHONY: test tests

test tests:
	@(cd tests ; ${MAKE} test)

errors:
	@(cd tests/errors ; ${MAKE} test)

docs:
	@(cd doc ; ${MAKE} docs)

DIST_FILES_DIR = ~/Dropbox/dist-files/
GH_PAGES_DIR = gh-pages/
PACKAGE_FILE = ${PACKAGE_TARNAME}-${PACKAGE_VERSION}.tar.gz

upload: dist
	@echo "Remember to run:"
	@echo "        gt tag ${PACKAGE_VERSION}"

upload-docs: docs
	@-[ -d ${GH_PAGES_DIR} ] \
		&& echo "Updating manual on gh-pages ..." \
		&& cp doc/juise-manual.html ${GH_PAGES_DIR} \
		&& (cd ${GH_PAGES_DIR} \
			&& git commit -m 'new docs' \
                               juise-manual.html \
			&& git push origin gh-pages ) ; true

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = packaging/${PACKAGE_NAME}.pc

get-wiki:
	git clone https://github.com/Juniper/${PACKAGE_NAME}.wiki.git wiki

get-gh-pages:
	git clone https://github.com/Juniper/${PACKAGE_NAME}.git \
		gh-pages -b gh-pages


UPDATE_PACKAGE_FILE = \
    -e "s;__SHA1__;$$SHA1;" \
    -e "s;__SHA256__;SHA256 (textproc/${PACKAGE_FILE}) = $$SHA256;" \
    -e "s;__SIZE__;SIZE (textproc/${PACKAGE_FILE}) = $$SIZE;"

GH_PACKAGING_DIR = packaging/${PACKAGE_VERSION}
GH_PAGES_PACKAGE_DIR = ${GH_PAGES_DIR}/${GH_PACKAGING_DIR}

packages:
	@-[ -d ${GH_PAGES_DIR} ] \
		&& echo "Updating packages on gh-pages ..." \
		&& SHA1=`openssl sha1 ${PACKAGE_FILE} | awk '{print $$2}'` \
		&& SHA256="`openssl sha256 ${PACKAGE_FILE} | awk '{print $$2}'`" \
		&& SIZE="`ls -l ${PACKAGE_FILE} | awk '{print $$5}'`" \
		&& mkdir -p ${GH_PAGES_PACKAGE_DIR}/freebsd \
		&& echo "... ${GH_PAGES_PACKAGE_DIR}/${PACKAGE_NAME}.rb ..." \
		&& sed ${UPDATE_PACKAGE_FILE} \
			packaging/${PACKAGE_NAME}.rb.base \
			> ${GH_PAGES_PACKAGE_DIR}/${PACKAGE_NAME}.rb \
		&& cp packaging/${PACKAGE_NAME}.spec \
			${GH_PAGES_PACKAGE_DIR}/${PACKAGE_NAME}.spec \
		&& cp packaging/${PACKAGE_NAME}.spec \
			${GH_PAGES_PACKAGE_DIR}/${PACKAGE_NAME}.spec \
		&& echo "... ${GH_PAGES_PACKAGE_DIR}/freebsd ..." \
		&& sed ${UPDATE_PACKAGE_FILE} \
			${srcdir}/packaging/freebsd/distinfo.base \
			> ${GH_PAGES_PACKAGE_DIR}/freebsd/distinfo \
		&& cp ${srcdir}/packaging/freebsd/pkg-descr \
			${GH_PAGES_PACKAGE_DIR}/freebsd/pkg-descr \
		&& cp ${srcdir}/packaging/freebsd/pkg-plist \
			${GH_PAGES_PACKAGE_DIR}/freebsd/pkg-plist \
		&& cp ${srcdir}/packaging/freebsd/pkg-plist \
			${GH_PAGES_PACKAGE_DIR}/freebsd/pkg-plist \
		&& cp packaging/freebsd/port-Makefile \
			${GH_PAGES_PACKAGE_DIR}/freebsd/Makefile \
		&& (cd ${GH_PAGES_DIR} \
			&& git add ${GH_PACKAGING_DIR} \
			&& git commit -m 'new packaging data' \
                               ${GH_PACKAGING_DIR} \
			&& git push origin gh-pages ) ; true
