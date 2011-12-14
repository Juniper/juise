/*
 * $Id$
 *
 * Copyright (c) 2005, 2008, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * Extension functions for commit scripts
 */

/*
 * Register our extension functions.  We try to hide all the
 * details of the libxslt interactions here.
 */
int
ext_jcs_register_all (void);

/*
 * Record authentication information to pass to sub-processes
 */
char *
ext_jcs_set_auth_info (char *str);

/*
 * Get authentication information 
 */
char *
ext_jcs_get_auth_info (void);

/*
 * Get the authentication information for a particular parameter
 */
void
ext_jcs_extract_authinfo(const char *, char *, size_t);

/*
 * Recursively fixes the name space
 */
js_boolean_t
ext_jcs_fix_namespaces (lx_node_t *node);

/*
 * Send the prompt to mgd which will send it to cli for reading the response 
 * and read the response back from mgd
 */
char * 
ext_jcs_input_common (char *prompt, csi_type_t input_type);

void
ext_jcs_output_callback (const char *str);
