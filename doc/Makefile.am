#
# $Id$
#
# Copyright 2011, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

BINDIR = ~/work/yang/bin
OUTLINE2XML = ${BINDIR}/outline2xml
XML2RFC = ${BINDIR}/xml2rfc.tcl
XML2HTMLBIN = ${BINDIR}/rfc2629-yang.xslt
XML2HTMLDIR = `dirname ${XML2HTMLBIN}`
XML2HTML = xsltproc --path ${XML2HTMLDIR} ${XML2HTMLBIN}

OUTPUT = juise-manual-${PACKAGE_VERSION}
INPUT = juise.txt

EXTRA_DIST = \
    ${OUTPUT}.html \
    ${OUTPUT}.txt

doc docs: ${OUTPUT}.txt ${OUTPUT}.html

${OUTPUT}.txt: ${OUTPUT}.xml
	${XML2RFC} $< > $@

${OUTPUT}.xml: ${INPUT} ${OUTLINE2XML}
	${OUTLINE2XML} $< > $@

${OUTPUT}.html: ${OUTPUT}.fxml ${XML2HTMLBIN}
	${XML2HTML} $< > $@

${OUTPUT}.fxml: ${INPUT} ${OUTLINE2XML}
	${OUTLINE2XML} -D $< > $@

CLEANFILES = \
${OUTPUT}.xml \
${OUTPUT}.txt \
${OUTPUT}.fxml \
${OUTPUT}.html
