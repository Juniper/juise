/*
 * $Id$
 *
 * Copyright (c) 2000-2006, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#ifndef JS_CLIENT_H
#define JS_CLIENT_H

#include <string.h>

#define MGD_VERSION_CURRENT	1 /* Version of CLI-to-MGD protocol */
/* security modes, used to restrict access for this session */
#define SECURITY_MODE_ALLOW                0 /* no restriction */
#define SECURITY_MODE_DENY                 1 /* deny all commands */
#define SECURITY_MODE_ALLOW_FROM_PSD       2 /* allow commands forwarded from psd */
#define SECURITY_MODE_ALLOW_FROM_RSD       3 /* allow commands forwarded from rsd */

#define NON_TTY_SESSION	    "non-tty"

typedef struct js_client_s {
    int jscl_version;		/* Version of API being used */
    char *jscl_user;		/* User name (pw_name) */
    char *jscl_logged_in_user;	/* Loggedin User name */
    char *jscl_class;		/* Login class (pw_class) */
    char *jscl_logname;		/* Login name (getlogin()) */
    char *jscl_host;		/* Host client is running on */
    char *jscl_agent;		/* Client s/w identifier */
    char *jscl_tty;		/* Terminal name if applicable */
    char *jscl_cwd;		/* Current directory */
    char *jscl_logical_router;  /* Current logical router */
    int   jscl_is_lr_master;    /* Client is a lr master */
    int jscl_pid;		/* Process ID */
    int jscl_ppid;		/* Parent Process ID */
    int jscl_aborting;		/* Currently aborting command */
    int jscl_rawxml;		/* Display raw XML */
    const char *jscl_last_cmd;	/* Last command */
    int jscl_last_len;		/* How much of last_cmd is relevant */
    short jscl_apimode;		/* Running in the xml-mode API style */
    short jscl_netconf_mode;    /* Running in the netconf API style */
    short jscl_in_reply;	/* Are we in a <rpc-reply>? */
    short jscl_junos_key;	/* Should XML config emit junos:key="key"? */
    short jscl_in_sync;		/* Are we doing a commit sync? */
    const char *jscl_log_client;/* Explicitly set log client */
    short jscl_cli;		/* Is our client CLI */ 
    short jscl_diffing;		/* Session diffing database? */ 
    short jscl_need_trailer;	/* MUST emit ]]>]]> after each conversation */
    short jscl_interactive;     /* Interactive mode */
    short jscl_test_app;     /* Interactive mode */
    int jscl_security_mode;     /* which security mode are we in? */
} js_client_t;

extern js_client_t js_client_data;

#define JS_CLIENT_ACCESSOR(_field, _type, _get_name, _set_name) 	\
static inline _type							\
_get_name (void)							\
{									\
    return js_client_data._field;					\
}									\
									\
static inline void							\
_set_name (_type value)							\
{									\
    js_client_data._field = value;      				\
}

/*
 * Same as above, but used for string types.  This will free
 * the memory of previous string if called multiple times.
 */
#define JS_CLIENT_STR_ACCESSOR(_field, _get_name, _set_name)            \
static inline const char *						\
_get_name (void)							\
{									\
    return js_client_data._field;					\
}									\
									\
static inline void							\
_set_name (const char *value)   					\
{				                                        \
    if (js_client_data._field) free(js_client_data._field);             \
    js_client_data._field = value ? strdup (value) : NULL;              \
}

JS_CLIENT_STR_ACCESSOR(jscl_user, js_client_user, js_client_set_user);
JS_CLIENT_STR_ACCESSOR(jscl_logged_in_user, js_client_logged_in_user, js_client_set_logged_in_user);
JS_CLIENT_STR_ACCESSOR(jscl_class, js_client_class, js_client_set_class);
JS_CLIENT_STR_ACCESSOR(jscl_logname, js_client_logname, js_client_set_logname);
JS_CLIENT_STR_ACCESSOR(jscl_host, js_client_host, js_client_set_host);
JS_CLIENT_STR_ACCESSOR(jscl_agent, js_client_agent, js_client_set_agent);
JS_CLIENT_STR_ACCESSOR(jscl_tty, js_client_tty, js_client_set_tty);
JS_CLIENT_STR_ACCESSOR(jscl_cwd, js_client_cwd, js_client_set_cwd);
JS_CLIENT_STR_ACCESSOR(jscl_logical_router, js_client_logical_router,
		       js_client_set_logical_router);

JS_CLIENT_ACCESSOR(jscl_version, int,
		    js_client_version, js_client_set_version);
JS_CLIENT_ACCESSOR(jscl_pid, int, js_client_pid, js_client_set_pid);
JS_CLIENT_ACCESSOR(jscl_ppid, int, js_client_ppid, js_client_set_ppid);
JS_CLIENT_ACCESSOR(jscl_aborting, int, js_client_aborting,
		    js_client_set_aborting);
JS_CLIENT_ACCESSOR(jscl_rawxml, int, js_client_rawxml,
		    js_client_set_rawxml);
JS_CLIENT_ACCESSOR(jscl_apimode, short, js_client_apimode,
		    js_client_set_apimode);
JS_CLIENT_ACCESSOR(jscl_netconf_mode, short, js_client_netconf_mode,
                    js_client_set_netconf_mode);
JS_CLIENT_ACCESSOR(jscl_in_reply, short, js_client_in_reply,
		    js_client_set_in_reply);
JS_CLIENT_ACCESSOR(jscl_junos_key, short, js_client_junos_key,
		    js_client_set_junos_key);
JS_CLIENT_ACCESSOR(jscl_in_sync, short, js_client_in_sync,
		    js_client_set_in_sync);
JS_CLIENT_ACCESSOR(jscl_is_lr_master, int, js_client_is_lr_master,
		    js_client_set_is_lr_master);
JS_CLIENT_ACCESSOR(jscl_cli, short, js_client_is_cli,
		    js_client_set_cli);
JS_CLIENT_ACCESSOR(jscl_diffing, short, js_client_is_diffing,
		    js_client_set_diffing);
JS_CLIENT_ACCESSOR(jscl_need_trailer, short, js_client_need_trailer,
		    js_client_set_need_trailer);
JS_CLIENT_ACCESSOR(jscl_interactive, short, js_client_is_interactive,
		   js_client_set_interactive);
JS_CLIENT_ACCESSOR(jscl_test_app, short, js_client_is_test_app,
		   js_client_set_test_app);
JS_CLIENT_ACCESSOR(jscl_security_mode, int, js_client_security_mode,
		   js_client_set_security_mode);


static inline void
js_client_set_last_cmd (const char *last)
{
    js_client_data.jscl_last_len = last ? strcspn(last, " ") : 0;
    js_client_data.jscl_last_cmd = last;
}

static inline int
js_client_match_last_cmd (const char *cmd)
{
    return (cmd == NULL || js_client_data.jscl_last_cmd == NULL
	    || strncmp(cmd, js_client_data.jscl_last_cmd,
		       js_client_data.jscl_last_len) == 0);
}

static inline void
js_client_set_log_client (const char *log_client)
{
    js_client_data.jscl_log_client = log_client;
}

const char *
js_client_commit_log_client (void);

#endif /* JS_CLIENT_H */
