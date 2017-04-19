/*
 * $Id: xmlinput.c,v 1.3 2006/11/18 03:59:53 phil Exp $
 *
 * Copyright (c) 2000-2006, Juniper Networks, Inc.
 * All rights reserved.
 *
 * Functions for handling xml rpc
 *
 */

#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <sys/queue.h>

#include "juiseconfig.h"
#include <libpsu/psucommon.h>
#include <libpsu/psustring.h>
#include <libjuise/io/fbuf.h>
#include <libjuise/common/aux_types.h>
#include <libjuise/string/strextra.h>
#include <libjuise/io/dbgpr.h>
#include <libjuise/xml/xmlutil.h>
#include <libjuise/xml/xmlrpc.h>
#if 0
#include <libjuise/time/timestr.h>
#include <libjuise/common/bits.h>
#include <libjuise/xml/client.h>
#include <libjuise/common/allocadup.h>
#endif

static xml_get_method_t  xml_get_method;

xml_get_method_t
xml_set_get_method (xml_get_method_t func)
{
    xml_get_method_t old_func = xml_get_method;
    xml_get_method = func;
    return old_func;
}

/*
 * xml_input_match2: match input against known-to-be-next values.
 *
 * This was split up from xml_input_match so that we can decouple the
 * abort function from js_client_set_aborting() -- which is specific to
 * cli/mgd.
 *
 * Returns TRUE for success, FALSE for failure.
 */
boolean
xml_input_match2 (void *peer, int type_to_match, int *typep,
                  const char **tagp, char **restp, unsigned *flagsp,
                  void (*abort_cb)(void))
{
    char *cp, *rest;
    int type;
    unsigned flags = flagsp ? *flagsp : 0;
    unsigned xml_flags = 0;

    if (!(flags & XIMF_LINEBUFD)) xml_flags |= FXF_COMPLETE;

    INSIST(xml_get_method != NULL);
    
    for (;;) {
	type = 0;
	rest = NULL;
	cp = (*xml_get_method)(peer, &type, &rest, xml_flags);

	DEBUGIF(flags & XIMF_TRACE,
		dbgpr("xml: input: [%s] [%s] %d:%s; after [%s] %d:%s%s%s",
		      cp ?: "(NULL)", rest ?: "(NULL)", type,
		      fbuf_xml_type(type),
		      (tagp && *tagp) ? *tagp : "(NULL)", type_to_match,
		      fbuf_xml_type(type_to_match),
		      (!type_to_match || type == type_to_match)
				? "" : " type-mismatch",
		      (tagp && cp && (*tagp == NULL || streq(cp, *tagp)))
				? "" : " tag-mismatch"));

	if (cp == NULL) break;

	if (type == XML_TYPE_DATA)
	    xml_unescape(cp, INT_MAX, cp, FALSE);

	if (type == XML_TYPE_COMMENT && (flags & XIMF_SKIP_COMMENTS))
	    continue;
	
	if (type == XML_TYPE_NOOP && !(flags & XIMF_ALLOW_NOOP))
	    continue;

	if (type == XML_TYPE_ABORT) {
            if (abort_cb) abort_cb();

	    if (flags & XIMF_SKIP_ABORTS) {
		if (flagsp) *flagsp |= XIMF_ABORT_SEEN;
		continue;
	    }
	}

	if (type_to_match && type != type_to_match) break;
	if (tagp && *tagp && !streq(cp, *tagp)) break;

	if (typep) *typep = type;
	if (tagp) *tagp = cp;
	if (restp) *restp = rest;

	return TRUE;		/* The sweat smell of victory */
    }

    if (typep) *typep = type;
    if (tagp) *tagp = cp;
    if (restp) *restp = rest;

    return FALSE;		/* The agony of defeat */
}

