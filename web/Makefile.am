#
# $Id$
#
# Copyright 2011, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

# CLEANFILES = lighttpd.conf

EXTRA_DIST = \
    Makefile.am \
    Makefile.in \
    lighttpd.conf.in

BASE_FILES = \
    favicon.ico

OTHER_FILES = \
    icons/loading.png \
    images/logo.png \
    jquery/jquery-ui.js \
    jquery/jquery-ui.min.js \
    jquery/jquery.dbgpr.js \
    jquery/jquery.js \
    jquery/jquery.min.js \
    jquery/jquery.mruPulldown.js \
    clira/clira.html \
    clira/clira.css \
    clira/clira.js \
    clira/parser.js \
    clira/parser-test.js \
    clira/clira.slax \
    static/drag-test.html

THEME_BLACK_TIE = \
    themes/black-tie/jquery-ui-black-tie.css \
    themes/black-tie/addons-black-tie.css \
    themes/black-tie/addons/html_bg.png \
    themes/black-tie/images/ui-bg_diagonals-thick_8_333333_40x40.png \
    themes/black-tie/images/ui-bg_flat_65_ffffff_40x100.png \
    themes/black-tie/images/ui-bg_glass_40_111111_1x400.png \
    themes/black-tie/images/ui-bg_glass_55_1c1c1c_1x400.png \
    themes/black-tie/images/ui-bg_highlight-hard_100_f9f9f9_1x100.png \
    themes/black-tie/images/ui-bg_highlight-hard_40_aaaaaa_1x100.png \
    themes/black-tie/images/ui-bg_highlight-soft_50_aaaaaa_1x100.png \
    themes/black-tie/images/ui-bg_inset-hard_45_cd0a0a_1x100.png \
    themes/black-tie/images/ui-bg_inset-hard_55_ffeb80_1x100.png \
    themes/black-tie/images/ui-icons_222222_256x240.png \
    themes/black-tie/images/ui-icons_4ca300_256x240.png \
    themes/black-tie/images/ui-icons_bbbbbb_256x240.png \
    themes/black-tie/images/ui-icons_ededed_256x240.png \
    themes/black-tie/images/ui-icons_ffcf29_256x240.png \
    themes/black-tie/images/ui-icons_ffffff_256x240.png

THEME_DOT_LUV = \
    themes/dot-luv/jquery-ui-dot-luv.css \
    themes/dot-luv/addons-dot-luv.css \
    themes/dot-luv/addons/html_bg.png \
    themes/dot-luv/html_bg.png \
    themes/dot-luv/images/ui-bg_diagonals-thick_15_0b3e6f_40x40.png \
    themes/dot-luv/images/ui-bg_dots-medium_30_0b58a2_4x4.png \
    themes/dot-luv/images/ui-bg_dots-small_20_333333_2x2.png \
    themes/dot-luv/images/ui-bg_dots-small_30_a32d00_2x2.png \
    themes/dot-luv/images/ui-bg_dots-small_40_00498f_2x2.png \
    themes/dot-luv/images/ui-bg_flat_0_aaaaaa_40x100.png \
    themes/dot-luv/images/ui-bg_flat_40_292929_40x100.png \
    themes/dot-luv/images/ui-bg_gloss-wave_20_111111_500x100.png \
    themes/dot-luv/images/ui-icons_00498f_256x240.png \
    themes/dot-luv/images/ui-icons_98d2fb_256x240.png \
    themes/dot-luv/images/ui-icons_9ccdfc_256x240.png \
    themes/dot-luv/images/ui-icons_ffffff_256x240.png

THEME_EXCITE_BIKE = \
    themes/excite-bike/jquery-ui-excite-bike.css \
    themes/excite-bike/addons-excite-bike.css \
    themes/excite-bike/addons/ui-bg_diagonals-small_25_c5ddfc_40x40.png \
    themes/excite-bike/images/ui-bg_diagonals-small_25_c5ddfc_40x40.png \
    themes/excite-bike/images/ui-bg_diagonals-thick_20_e69700_40x40.png \
    themes/excite-bike/images/ui-bg_diagonals-thick_22_1484e6_40x40.png \
    themes/excite-bike/images/ui-bg_diagonals-thick_26_2293f7_40x40.png \
    themes/excite-bike/images/ui-bg_flat_0_e69700_40x100.png \
    themes/excite-bike/images/ui-bg_flat_0_e6b900_40x100.png \
    themes/excite-bike/images/ui-bg_highlight-soft_100_f9f9f9_1x100.png \
    themes/excite-bike/images/ui-bg_inset-hard_100_eeeeee_1x100.png \
    themes/excite-bike/images/ui-icons_0a82eb_256x240.png \
    themes/excite-bike/images/ui-icons_0b54d5_256x240.png \
    themes/excite-bike/images/ui-icons_5fa5e3_256x240.png \
    themes/excite-bike/images/ui-icons_fcdd4a_256x240.png \
    themes/excite-bike/images/ui-icons_ffffff_256x240.png

WEB_FILES = \
    ${BASE_FILES} \
    ${THEME_BLACK_TIE} \
    ${THEME_DOT_LUV} \
    ${THEME_EXCITE_BIKE}

EXTRA_DIST += ${WEB_FILES}

INSTALL_WEB_FILES =  \
    lighttpd.conf \
    ${WEB_FILES}

not-yet-install-exec-hook:
	@echo "Installing Web Files ... "
	@for file in ${INSTALL_WEB_FILES} ; do \
		if [ -f $$file ]; then \
			rfile=$$file ; \
		else \
			rfile=${srcdir}/$$file ; \
		fi ; \
		mdir=${JUISE_WEB_DIR}/`dirname $$file` ; \
		mkdir -p $$mdir ; \
		cp $$rfile $$mdir/ ; \
        done

svnignore:
	svn propset svn:ignore -F ${srcdir}/.svnignore ${srcdir}
