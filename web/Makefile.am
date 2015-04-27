#
# $Id$
#
# Copyright 2011, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

webdatadir = $(datadir)/juise/web

EXTRA_DIST = \
    Makefile.am \
    Makefile.in

BASE_FILES = \
    favicon.ico \
    index.html

EXTERNAL_FILES = \
    external/d3/d3.v3.js \
    external/d3/d3.v3.min.js \
    external/ember/ember.js \
    external/ember-restless/ember-restless+extras.js \
    external/gmaps/gmaps.js \
    external/handlebars/handlebars.js \
    external/jqgrid/grid.locale-en.js \
    external/jqgrid/jquery.jqGrid.min.js \
    external/jqgrid/jquery.jqGrid.src.js \
    external/jqgrid/ui.jqgrid.css \
    external/jquery/jquery-1.9.1.js \
    external/jquery/jquery.js \
    external/jquery/jquery.min.js \
    external/jquery-checkbox/jquery.checkbox.css \
    external/jquery-checkbox/jquery.checkbox.js \
    external/jquery-checkbox/jquery.checkbox.min.js \
    external/jquery-qtip/imagesloaded.min.js \
    external/jquery-qtip/jquery.qtip.css \
    external/jquery-qtip/jquery.qtip.js \
    external/jquery-qtip/jquery.qtip.min.css \
    external/jquery-qtip/jquery.qtip.min.js \
    external/jquery-ui/jquery-ui.js \
    external/jquery-ui/jquery-ui.min.js \
    external/bootstrap/css/bootstrap-theme.css \
    external/bootstrap/css/bootstrap-theme.css.map \
    external/bootstrap/css/bootstrap-theme.min.css \
    external/bootstrap/css/bootstrap.css \
    external/bootstrap/css/bootstrap.css.map \
    external/bootstrap/css/bootstrap.min.css \
    external/bootstrap/fonts/glyphicons-halflings-regular.eot \
    external/bootstrap/fonts/glyphicons-halflings-regular.svg \
    external/bootstrap/fonts/glyphicons-halflings-regular.ttf \
    external/bootstrap/fonts/glyphicons-halflings-regular.woff \
    external/bootstrap/fonts/glyphicons-halflings-regular.woff2 \
    external/bootstrap/js/bootstrap.js \
    external/bootstrap/js/bootstrap.min.js \
    external/bootstrap/js/npm.js \
    external/qunit/qunit.css \
    external/qunit/qunit.js

WEB_FILES = \
    ${BASE_FILES} \
    ${EXTERNAL_FILES}

nobase_dist_webdata_DATA = ${WEB_FILES}
dist_doc_DATA = \
    clira.conf

dist_bin_SCRIPTS = \
    run-clira

EXTRA_DIST += ${WEB_FILES}

svnignore:
	svn propset svn:ignore -F ${srcdir}/.svnignore ${srcdir}

install-data-hook:
	${CP} -r ${srcdir}/apps ${srcdir}/core ${srcdir}/css ${srcdir}/images ${srcdir}/themes ${DESTDIR}/${webdatadir}

dist-hook:
	${CP} -r ${srcdir}/apps ${srcdir}/core ${srcdir}/css ${srcdir}/images ${srcdir}/themes ${distdir}
