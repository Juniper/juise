/*
 * $Id$
 *
 * Copyright (c) 2004-2005, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * libxml -- XML portability layer
 */

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

/*
 * Use these options for reading XML data
 */
#define LX_READ_OPTIONS (XML_PARSE_NONET | XML_PARSE_NODICT \
			| XML_PARSE_NSCLEAN | XML_PARSE_NOCDATA)

/* Use our own typedefs */
#define XML_TYPEDEF(_struct, _typedef ) \
    struct _struct;			\
    typedef struct _struct _typedef;

XML_TYPEDEF(_xmlDoc, lx_document_t);
XML_TYPEDEF(_xmlNode, lx_node_t);
XML_TYPEDEF(_xsltStylesheet, lx_stylesheet_t);
XML_TYPEDEF(_xmlNodeSet, lx_nodeset_t);
XML_TYPEDEF(_xmlSaveCtxt, lx_output_t);
XML_TYPEDEF(_xmlXPathObject, lx_xpath_t);

/* Helper types */
typedef int lx_cookie_t;
#define LX_COOKIE_CLEAR(_x) (*_x) = -1;

/*
 * Simple error call
 */
#define LX_ERR(_msg...) xsltGenericError(xsltGenericErrorContext, _msg)

/*
 * Perform any initialization required by the xml parser
 */
void lx_parser_init (void);

/*
 * Perform any cleanup required by the xml parser
 */
void lx_parser_done (void);

/*
 * Read a file into an in-memory XML document tree.
 */
lx_document_t *lx_document_read (const char *filename);

/*
 *   Read a document from the fd and return it
 */
lx_document_t *lx_document_read_fd (int fd, const char *filename);

/*
 * Build a new document with the given root element
 */
lx_document_t *lx_document_create (const char *root);

/*
 * Free a document
 */
void lx_document_free (lx_document_t *docp);

/*
 * Find a node in a document
 */
lx_node_t *lx_document_find (lx_document_t *config, const char **path);
/*
 * Find a node beneath another node
 */
lx_node_t *lx_node_find (lx_node_t *top, const char **path);

/*
 * Return the root node of a document
 */
lx_node_t *lx_document_root (lx_document_t *docp);

/*
 * Simple accessor for children
 */
lx_node_t *lx_node_children (lx_node_t *np);

/*
 * Return the next node (sibling)
 */
lx_node_t *lx_node_next (lx_node_t *np);

/*
 * Return the value of a (simple) element
 */
const char *lx_node_child_value (lx_node_t *parent, const char *name);

/*
 * Read a stylesheet from a file descriptor
 */
lx_stylesheet_t *lx_style_read_fd (int fd, const char *filename);

/*
 * Run a stylesheet on a document; return the results
 */
lx_document_t *lx_run_stylesheet (lx_stylesheet_t *slp, lx_document_t *docp,
				  const char **params);

/*
 * Dump a copy of the results to the debug log.
 */
void lx_dump_results (lx_document_t *docp, lx_stylesheet_t *sp);

/*
 * Dump a node to the debug log
 */
void
lx_dump_node (lx_document_t *docp, lx_node_t *nodep);

/*
 * Interate thru a nodeset.  Pass NULL for prev on the initial call.
 */
lx_node_t *
lx_nodeset_next (lx_nodeset_t *nodeset, lx_cookie_t *cookie);

/*
 * Return the size of a nodeset
 */
unsigned long
lx_nodeset_size (lx_nodeset_t *nodeset);

/*
 * Return a nodeset matching the xpath expression
 */
lx_nodeset_t *
lx_xpath_select (lx_document_t *docp, lx_node_t *nodep, const char *expr);

/*
 * Open an output file
 */
lx_output_t *
lx_output_open (const char *filename);

/*
 * Open an output file descriptor
 */
lx_output_t *
lx_output_open_fd (int fd);

/*
 * Close an output file handle
 */
void
lx_output_close (lx_output_t *handle);

/*
 * Write a document to a file
 */
void
lx_output_document (lx_output_t *handle, lx_document_t *docp);

/*
 * Write a node to a file
 */
void
lx_output_node (lx_output_t *handle, lx_node_t *nodep);

/*
 * Write a node to the trace file
 */
void
lx_trace_node (lx_node_t *nodep, const char *fmt, ...);

/*
 * Write a document to a file to the trace file
 */
void
lx_trace_document (lx_document_t *docp, const char *fmt, ...);

/*
 * Write the node's children to a file
 */
void
lx_output_children (lx_output_t *handle, lx_node_t *nodep);
