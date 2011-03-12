/*
 * $Id: libxml.c 346460 2009-11-14 05:06:47Z ssiano $
 *
 * Copyright (c) 2004-2006, Juniper Networks, Inc.
 * All rights reserved.
 *
 * XML portability layer -- hide all the cruft from the non-cruft
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <string.h>
#include <paths.h>

#include <libxml/xpathInternals.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <libxslt/extensions.h>
#include <libslax/slax.h>

#include "config.h"
#include <libjuise/env/env_paths.h>
#include <libjuise/common/allocadup.h>
#include <libjuise/common/aux_types.h>
#include <libjuise/string/strextra.h>
#include <libjuise/xml/xmlrpc.h>
#include <libjuise/io/dbgpr.h>
#include <libjuise/io/logging.h>
#include <libjuise/xml/libxml.h>
#include <libjuise/io/jsio.h>
#include <libjuise/xml/extensions.h>

#define CONTEXT_STACK_MAX 64
static int lx_context_stack_size = 0;
static xmlParserCtxt *lx_context_stack[CONTEXT_STACK_MAX];
static xmlParserCtxt *lx_context;

static void
lx_parser_open (void)
{
    if (lx_context_stack_size >= CONTEXT_STACK_MAX - 1) {
	LX_ERR("could not store parser context");
	return;
    }

    /* Push the old value */
    lx_context_stack[lx_context_stack_size++] = lx_context;

    lx_context = xmlNewParserCtxt();
    if (lx_context == NULL) {
	LX_ERR("could not make parser context");
	return;
    }

    /*
     * Set up the default options for the context
     */
    (void) xmlCtxtUseOptions(lx_context, LX_READ_OPTIONS);

#ifdef XXX_UNUSED
    /*
     * Set up error callbacks
     */
    xerr_xml_init(lx_context);
#endif /* XXX_UNUSED */
}

static void
lx_parser_close (void)
{
    if (lx_context_stack_size == 0) {
	LX_ERR("parser stack empty");
	return;
    }

    xmlFreeParserCtxt(lx_context);

    /* Pop the old value */
    lx_context = lx_context_stack[--lx_context_stack_size];
    lx_context_stack[lx_context_stack_size] = NULL;

#ifdef XXX_UNUSED
    xerr_xml_init(lx_context);
#endif /* XXX_UNUSED */
}

/*
 * Initialize the XML parser
 */
void
lx_parser_init (void)
{
    xmlInitParser();
    lx_parser_open();
}

/*
 * Cleanup the XML parser
 */
void
lx_parser_done (void)
{

    /*
     * Clean up code for libxslt
     */
    lx_parser_close();
    lx_context = NULL;

#if 0
    xsltCleanupGlobals();
#endif

    xmlCleanupParser();
}

/*
 * Read a document and return it
 */
lx_document_t *
lx_document_read (const char *filename)
{
    /*
     * If the file is stdin, we have to use a different method
     * since xmlCtxtReadFile() does some sort of lseek() and
     * reads the input stream twice.
     */
    if (streq(filename, "-") || streq(filename, _PATH_DEV "stdin"))
	return xmlCtxtReadFd(lx_context, 0, _PATH_DEVNULL, NULL, 0);

    return xmlCtxtReadFile(lx_context, filename, NULL, 0);
}

/*
 * Read a document from the fd and return it
 */
lx_document_t *
lx_document_read_fd (int fd, const char *filename)
{
    return slaxCtxtReadFd(lx_context, fd, filename, NULL, 0);
}

/*
 * Build a new document with the given root element
 */
lx_document_t *
lx_document_create (const char *root)
{
    lx_document_t *docp;
    lx_node_t *nodep;

    docp = xmlNewDoc((const xmlChar *) XML_DEFAULT_VERSION);
    if (docp == NULL)
	return NULL;

    docp->standalone = 1;

    nodep = xmlNewNode(NULL, (const xmlChar *) root);
    if (nodep)
	xmlDocSetRootElement(docp, nodep);

    return docp;
}

/*
 * Free a document
 */
void
lx_document_free (lx_document_t *docp)
{
    xmlFreeDoc(docp);
}

/*
 * Find a node in a document
 */
lx_node_t *
lx_document_find (lx_document_t *config UNUSED, const char **path UNUSED)
{
    return NULL;
}

/*
 * Find a node beneath another node
 */
lx_node_t *
lx_node_find (lx_node_t *parent, const char **path)
{
    lx_node_t *np = parent;

    /*
     * For each element of the path, look thru the nodes and
     * see if there's a matching element.  If so, loop again.
     */
    for (; *path; path++) {
	for (np = parent->children; np; np = np->next) {
	    if (np->type == XML_ELEMENT_NODE
			&& streq(*path, (const char *) np->name))
		break;
	}

	if (np == NULL)
	    return NULL;

	parent = np;
    }

    trace(trace_file, CS_TRC_DEBUG, "find: returning %s/%p", np->name, np);
    return np;
}

/*
 * Return the root node of a document
 */
lx_node_t *
lx_document_root (lx_document_t *docp)
{
    if (docp == NULL)
	return NULL;
    return xmlDocGetRootElement(docp);
}

/*
 * Return the name of a node
 */
const char *
lx_node_name (lx_node_t *np)
{
    if (np == NULL || np->name == NULL)
	return NULL;
    return (const char *) np->name;
}

/*
 * Simple accessor for children
 */
lx_node_t *
lx_node_children (lx_node_t *np)
{
    if (np == NULL || np->name == NULL)
	return NULL;
    return np->children;
}

/*
 * Return the next node (sibling)
 */
lx_node_t *
lx_node_next (lx_node_t *np)
{
    if (np == NULL || np->name == NULL)
	return NULL;
    return np->next;
}

static boolean
string_is_whitespace (char *str)
{
    for ( ; *str; str++)
	if (!isspace((int) *str))
	    return FALSE;
    return TRUE;
}

/*
 * Return the value of this element
 */
const char *
lx_node_value (lx_node_t *np)
{
    lx_node_t *gp;
  
    if (np->type == XML_ELEMENT_NODE) {
	for (gp = np->children; gp; gp = gp->next)
	    if (gp->type == XML_TEXT_NODE
		&& !string_is_whitespace((char *) gp->content))
		return (char *) gp->content;
	/*
	 * If we found the element but not a valid text node,
	 * return an empty string to the caller can see
	 * empty elements.
	 */
	return "";
    }
     
    return NULL;
}

/*
 * Return the value of a (simple) element
 */
const char *
lx_node_child_value (lx_node_t *parent, const char *name)
{
    lx_node_t *np, *gp;

    for (np = parent->children; np; np = np->next) {
	if (np->type == XML_ELEMENT_NODE
	    	&& streq(name, (const char *) np->name)) {
	    for (gp = np->children; gp; gp = gp->next)
		if (gp->type == XML_TEXT_NODE
			&& !string_is_whitespace((char *) gp->content))
		    return (char *) gp->content;
	    /*
	     * If we found the element but not a valid text node,
	     * return an empty string to the caller can see
	     * empty elements.
	     */
	    return "";
	}
    }

    return NULL;
}

/*
 * Read a stylesheet from a file descriptor
 */
lx_stylesheet_t *
lx_style_read_fd (int fd, const char *filename)
{
    lx_document_t *docp;
    lx_stylesheet_t *stp;

    /*
     * Enable slax to allow imbedded slax documents.  We turn it
     * off before we leave to be consistent.
     */
    slaxEnable(SLAX_ENABLE);
    docp = slaxCtxtReadFd(lx_context, fd, filename, NULL, 0);

    if (docp == NULL) {
	slaxEnable(SLAX_DISABLE);
	trace(trace_file, TRACE_ALL, "reading stylesheet failed for '%s'",
	      filename);
	return NULL;
    }

    stp = xsltParseStylesheetDoc(docp);
    slaxEnable(SLAX_DISABLE);
    if (stp == NULL) {
	trace(trace_file, TRACE_ALL, "parsing stylesheet failed for '%s'",
	      filename);
	return NULL;
    }

    return stp;
}

/*
 * Run a stylesheet on a document; return the results
 */
lx_document_t *
lx_run_stylesheet (lx_stylesheet_t *slp, lx_document_t *docp,
		   const char **params)
{
    return xsltApplyStylesheet(slp, docp, params);
}

/*
 * Select a set of nodes from a node, using an XPath expression
 */
lx_nodeset_t *
lx_xpath_select (lx_document_t *docp, lx_node_t *nodep, const char *expr)
{
    xmlXPathObject *objp;
    lx_nodeset_t *results;
    xmlXPathContext *xpath_context;

    /*
     * Some XPath initialization: Create xpath evaluation context
     */
    xpath_context = xmlXPathNewContext(docp);
    INSIST(xpath_context != NULL);

    xmlXPathRegisterNs(xpath_context, (const xmlChar *) XNM_QUALIFIER,
		       (const xmlChar *) XNM_FULL_NS);

    xmlXPathRegisterNs(xpath_context, (const xmlChar *) XSL_QUALIFIER,
		       (const xmlChar *) XSL_FULL_NS);

    if (nodep == NULL)
	nodep = xmlDocGetRootElement(docp);

    xpath_context->node = nodep;

    objp = xmlXPathEvalExpression((const xmlChar *) expr, xpath_context);
    if (objp == NULL) {
	LX_ERR("could not evaluate xpath expression: %s", expr);
	return NULL;
    }

    if (objp->type != XPATH_NODESET) {
	trace(trace_file, TRACE_ALL,
	      "xpath_select: not a nodeset: %d", objp->type);
	results = NULL;
    } else {
	results = objp->nodesetval;
	objp->nodesetval = NULL;
    }

    xmlXPathFreeContext(xpath_context);
    xmlXPathFreeObject(objp);

    return results;
}

/*
 * Interate thru a nodeset.  Pass NULL for prev on the initial call.
 */
lx_node_t *
lx_nodeset_next (lx_nodeset_t *nodeset, lx_cookie_t *cookiep)
{
    int size;
    int cookie;

    if (nodeset == NULL || cookiep == NULL)
	return NULL;

    size = (nodeset) ? nodeset->nodeNr : 0;

    if (*cookiep < -1)		/* Sanity check */
	return NULL;

    cookie = (*cookiep)++;

    if (cookie < size)
	return nodeset->nodeTab[cookie];

    return NULL;
}

/*
 * Return the size of a nodeset
 */
unsigned long
lx_nodeset_size (lx_nodeset_t *nodeset)
{
    if (nodeset == NULL)
	return 0;

    return nodeset->nodeNr;
}

/*
 * Open an output file
 */
lx_output_t *
lx_output_open (const char *filename)
{
    lx_output_t *handle;

    handle = xmlSaveToFilename(filename, NULL, XML_SAVE_FORMAT);

    return handle;
}

/*
 * Open an output file descriptor
 */
lx_output_t *
lx_output_open_fd (int fd)
{
    lx_output_t *handle;

    handle = xmlSaveToFd(fd, NULL, XML_SAVE_FORMAT);

    return handle;
}

/*
 * Open an output file descriptor for a trace file
 */
static lx_output_t *
lx_output_open_trace (void)
{
    int fd;
    lx_output_t *handle;

    trace_file_flush(trace_file);

    fd = trace_fileno(trace_file);
    if (fd < 0)
	return NULL;

    handle = xmlSaveToFd(fd, NULL, XML_SAVE_FORMAT);

    return handle;
}


/*
 * Close an output file handle
 */
void
lx_output_close (lx_output_t *handle)
{
    xmlSaveFlush(handle);
    xmlSaveClose(handle);
}

/*
 * Write a document to a file
 */
void
lx_output_document (lx_output_t *handle, lx_document_t *docp)
{
    xmlSaveDoc(handle, docp);
}

/*
 * Write a node to a file
 */
void
lx_output_node (lx_output_t *handle, lx_node_t *nodep)
{
    xmlSaveTree(handle, nodep);
    xmlSaveFlush(handle);
}

/*
 * Write a node to a file to the trace file
 */
void
lx_trace_node (lx_node_t *nodep, const char *fmt, ...)
{
    va_list vap;

    va_start(vap, fmt);
    tracev(trace_file, TRACE_ALL, fmt, vap);
    va_end(vap);

    if (nodep) {
	trace(trace_file, TRACE_ALL, "begin dump");

	lx_output_t *handle = lx_output_open_trace();
	if (handle == NULL)
	    return;

	xmlSaveTree(handle, nodep);
	xmlSaveFlush(handle);

	lx_output_close(handle);
	trace(trace_file, TRACE_ALL, "end dump");

    } else {
	trace(trace_file, TRACE_ALL, "no data");
    }

}

/*
 * Register our extension functions.  We try to hide all the
 * details ofvoid) the libxslt interactions here.
 */
int
lx_extension_register (const char *ns, const char *name, xmlXPathFunction func)
{
    int rc;

    if (ns == NULL)
	ns = XNM_FULL_NS;

    rc = xsltRegisterExtModuleFunction((const xmlChar *) name,
				       (const xmlChar *) ns, func);
    if (rc != 0)
         xsltGenericError(xsltGenericErrorContext,
             "could not register extension function for %s\n", name);

    return rc;
}

/*
 * Write a document to a file to the trace file
 */
void
lx_trace_document (lx_document_t *docp, const char *fmt, ...)
{
    va_list vap;

    va_start(vap, fmt);
    tracev(trace_file, TRACE_ALL, fmt, vap);
    va_end(vap);

    if (docp) {
	trace(trace_file, TRACE_ALL, "begin dump");

	lx_output_t *handle = lx_output_open_trace();
	if (handle == NULL)
	    return;
	
	xmlSaveDoc(handle, docp);
	xmlSaveFlush(handle);

	lx_output_close(handle);
	trace(trace_file, TRACE_ALL, "end dump");

    } else {
	trace(trace_file, TRACE_ALL, "no data");
    }

}

/*
 * Write the node's children to a file
 */
void
lx_output_children (lx_output_t *handle, lx_node_t *nodep)
{
    for (nodep = nodep->children; nodep; nodep = nodep->next) {
	xmlSaveTree(handle, nodep);
    }
}
