/*
 * $Id: extensions.h 378661 2010-05-08 04:04:48Z builder $
 *
 * Copyright (c) 2005, 2008, Juniper Networks, Inc.
 * All rights reserved.
 *
 * Extension functions for commit scripts
 */

extern int input_fd;

/*
 * Register our extension functions.  We try to hide all the
 * details of the libxslt interactions here.
 */
int
ext_register_all (void);

/*
 * Record authentication information to pass to sub-processes
 */
char *
ext_set_auth_info (char *str);

/*
 * Get authentication information 
 */
char *
ext_get_auth_info (void);

/*
 * Get the authentication information for a particular parameter
 */
void
ext_extract_authinfo(const char *, char *, size_t);

/*
 * Recursively fixes the name space
 */
js_boolean_t
ext_fix_namespaces (lx_node_t *node);

/*
 * Send the prompt to mgd which will send it to cli for reading the response 
 * and read the response back from mgd
 */
char * 
ext_input_common (char *prompt, csi_type_t input_type);

void
ext_output_callback (const char *str);
