/*
 * $Id$
 *
 * Copyright (c) 2013, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#include <pwd.h>
#include <uuid/uuid.h>

#include "local.h"
#include <libpsu/psubase64.h>
#include "debug.h"
#include "db.h"
#include "sqlite3.h"
#include "util.h"


/* Current Database Schema Version */
#define MX_DB_CURRENT_VERSION		1

/* For hostkey 'type' in 'hostkeys' table */
#define MX_DB_HOSTKEY_RSA		0
#define MX_DB_HOSTKEY_DSA		1

/*
 * Mixer database schema version 1:
 *
 * general => (
 *  [0] version => INTEGER,            version of the db
 *  [1] privatekey => BLOB,            private key for all connections
 *  [2] publickey => BLOB,             public key for all connections
 *  [2] passphrase => VARCHAR,         saved passphrase for above key
 *  [3] save_passphrase => BOOL,       should we save passphrase?
 * )
 *
 * devices => (
 *  [0] id => INTEGER PRIMARY KEY NOT NULL,
 *  [1] name => VARCHAR,               'target' name/alias
 *  [2] hostname => VARCHAR,           host name of this device
 *  [3] port => INTEGER,               ssh port of this device
 *  [4] username => VARCHAR,           username to log in as
 *  [5] password => VARCHAR,           password to log in with
 *  [6] save_password => BOOL,         should we save password?
 * )
 *
 * hostkeys => (
 *  [0] id => INTEGER PRIMARY KEY NOT NULL,
 *  [1] name => VARCHAR,               host name identifier of key
 *  [2] type => INTEGER,               type of key (RSA/DSA)
 *  [3] hostkey => VARCHAR,            base64 hostkey of host
 * )
 *
 */

static sqlite3 *mx_db_handle;
static char *mx_db_passphrase;

/*
 * Check the hostkey database for this session's hostkey.
 *
 * return DB_CHECK_HOSTKEY_MATCH if hostkey is in database and matches
 * return DB_CHECK_HOSTKEY_NOMATCH if hostkey is not in database
 * return DB_CHECK_HOSTKEY_MISMATCH if hostkey is in database but does not
 *     match
 */
int
mx_db_check_hostkey (mx_sock_session_t *mssp, mx_request_t *mrp)
{
    sqlite3_stmt *stmt;
    const char *hostkey;
    int keytype, type, retval = DB_CHECK_HOSTKEY_NOMATCH;
    char buf[BUFSIZ];
    size_t len, olen;
    const char *fingerprint;
    char *enckey;

    if (opt_no_db)
	return DB_CHECK_HOSTKEY_NOMATCH;

    /*
     * Our hostkey name looks like:
     *
     * hostname:port
     */
    if (sqlite3_prepare_v2(mx_db_handle,
		"SELECT * FROM hostkeys WHERE name = ?", -1, &stmt,
		NULL) != SQLITE_OK) {
	mx_log("db error checking hostkey (%s:%d): %s", mrp->mr_hostname,
		mrp->mr_port, sqlite3_errmsg(mx_db_handle));
	return retval;
    }
    snprintf(buf, sizeof(buf), "%s:%d", mrp->mr_hostname, mrp->mr_port);
    if (sqlite3_bind_text(stmt, 1, buf, -1, SQLITE_STATIC) != SQLITE_OK) {
	mx_log("db error checking hostkey (%s): %s", buf,
		sqlite3_errmsg(mx_db_handle));
	goto cleanup;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
	hostkey = (const char*) sqlite3_column_text(stmt, 3);
	type = sqlite3_column_int(stmt, 2);
	switch (type) {
	    case MX_DB_HOSTKEY_RSA:
		keytype = LIBSSH2_HOSTKEY_TYPE_RSA;
		break;
	    case MX_DB_HOSTKEY_DSA:
		keytype = LIBSSH2_HOSTKEY_TYPE_DSS;
		break;
	    default:
		mx_log("Unkown key type in key '%s', deleting.", hostkey);
		snprintf(buf, sizeof(buf), "DELETE FROM hostkeys WHERE id = '%d'",
			sqlite3_column_int(stmt, 0));
		sqlite3_exec(mx_db_handle, buf, NULL, NULL, NULL);
		goto cleanup;
	}
	fingerprint = libssh2_session_hostkey(mssp->mss_session, &len, &type);

	/*
	 * Skip "ssh-rsa ".  Note that libssh2 makes these same assumptions.
	 */
	fingerprint += 8;
	len -= 8;

	enckey = psu_base64_encode(fingerprint, len, &olen);
	if (!enckey) {
	    mx_log("Could not allocate memory for base64-encoded key");
	    goto cleanup;
	}

	if (!strncmp(enckey, hostkey, strlen(hostkey)) && (keytype == type)) {
	    retval = DB_CHECK_HOSTKEY_MATCH;
	} else {
	    retval = DB_CHECK_HOSTKEY_MISMATCH;
	}

	free(enckey);
    }

cleanup:

    sqlite3_finalize(stmt);

    return retval;
}

/*
 * Save hostkey to db
 */
void
mx_db_save_hostkey (mx_sock_session_t *mssp, mx_request_t *mrp)
{
    int type, rc;
    char keyname[BUFSIZ];
    size_t len, olen;
    const char *fingerprint;
    char *enckey = NULL;
    sqlite3_stmt *stmt;

     if (opt_no_db)
	return;

   /*
     * Delete old hostkeys with this name
     */
    if (sqlite3_prepare_v2(mx_db_handle,
		"DELETE FROM hostkeys WHERE name = ?", -1, &stmt,
		NULL) != SQLITE_OK) {
	mx_log("db error deleting from hostkeys: %s",
		sqlite3_errmsg(mx_db_handle));
	return;
    }
    snprintf(keyname, sizeof(keyname), "%s:%d", mrp->mr_hostname, mrp->mr_port);
    if (sqlite3_bind_text(stmt, 1, keyname, -1, SQLITE_STATIC) != SQLITE_OK) {
	mx_log("db error deleting from hostkeys: %s",
		sqlite3_errmsg(mx_db_handle));
	goto cleanup;
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_OK && rc != SQLITE_DONE) {
	mx_log("db error deleting from hostkeys: %s",
		sqlite3_errmsg(mx_db_handle));
	goto cleanup;
    }

    sqlite3_finalize(stmt);

    /*
     * Add new hostkey entry.  First base64 encode our key.
     */
    fingerprint = libssh2_session_hostkey(mssp->mss_session, &len, &type);

    /*
     * Skip "ssh-rsa ".  Note that libssh2 makes these same assumptions.
     */
    fingerprint += 8;
    len -= 8;

    enckey = psu_base64_encode(fingerprint, len, &olen);
    if (!enckey) {
	mx_log("Could not allocate memory for base64-encoded key");
	goto cleanup;
    }

    sqlite3_prepare_v2(mx_db_handle,
	    "INSERT INTO hostkeys (name, type, hostkey) VALUES (?, ?, ?)", -1,
	    &stmt, NULL);
    if (sqlite3_bind_text(stmt, 1, keyname, -1,
		SQLITE_STATIC) != SQLITE_OK) {
	mx_log("db error inserting new hostkey (%s): %s", mrp->mr_hostname,
		sqlite3_errmsg(mx_db_handle));
	goto cleanup;
    }
    if (sqlite3_bind_int(stmt, 2, (type == LIBSSH2_HOSTKEY_TYPE_RSA) ?
		MX_DB_HOSTKEY_RSA : MX_DB_HOSTKEY_DSA) != SQLITE_OK) {
	mx_log("db error inserting new hostkey (%s): %s", mrp->mr_hostname,
		sqlite3_errmsg(mx_db_handle));
	goto cleanup;
    }
    if (sqlite3_bind_text(stmt, 3, enckey, -1,
		SQLITE_STATIC) != SQLITE_OK) {
	mx_log("db error inserting new hostkey (%s): %s", mrp->mr_hostname,
		sqlite3_errmsg(mx_db_handle));
	goto cleanup;
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_OK && rc != SQLITE_DONE) {
	mx_log("db error inserting new hostkey (%s): %s", mrp->mr_hostname,
		sqlite3_errmsg(mx_db_handle));
	goto cleanup;
    }

cleanup:

    if (enckey) {
	free(enckey);
    }

    sqlite3_finalize(stmt);
}

/*
 * Get the stored passphrase from the db.  If none stored, return NULL
 */
const char *
mx_db_get_passphrase (void)
{
    int rc;
    sqlite3_stmt *stmt;
    static char *db_passphrase = NULL;

     if (opt_no_db)
	return mx_db_passphrase;

    rc = sqlite3_prepare_v2(mx_db_handle, "SELECT passphrase FROM general",
	    -1, &stmt, NULL);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
	if (db_passphrase) {
	    free(db_passphrase);
	}
	db_passphrase = nstrdup((const char *) sqlite3_column_text(stmt, 0));
    }

    sqlite3_finalize(stmt);

    return db_passphrase;
}

/*
 * Save the passphrase to the db if save_passphrase is set to 1
 */
void
mx_db_save_passphrase (const char *passphrase)
{
    int rc;
    sqlite3_stmt *stmt;
    int save_passphrase = 0;

    if (opt_no_db) {
	if (mx_db_passphrase)
	    free(mx_db_passphrase);
	mx_db_passphrase = strdup(passphrase);
	return;
    }

    rc = sqlite3_prepare_v2(mx_db_handle, "SELECT save_passphrase FROM "
	    " general", -1, &stmt, NULL);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
	save_passphrase = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    if (save_passphrase) {
	if (sqlite3_prepare_v2(mx_db_handle,
		    "UPDATE general SET passphrase = ?", -1, &stmt,
		    NULL) != SQLITE_OK) {
	    mx_log("db error saving passphrase: %s",
		    sqlite3_errmsg(mx_db_handle));
	    return;
	}
	if (sqlite3_bind_text(stmt, 1, passphrase, -1,
		    SQLITE_STATIC) != SQLITE_OK) {
	    mx_log("db error saving passphrase: %s",
		sqlite3_errmsg(mx_db_handle));
	    goto cleanup;
	}
	sqlite3_step(stmt);
    } else {
	return;
    }

cleanup:

    sqlite3_finalize(stmt);
}

/*
 * Save the password to the record if save_password is set to 1
 */
void
mx_db_save_password (mx_request_t *mrp, const char *password)
{
    sqlite3_stmt *stmt;
    int save_password = 0, row_id = -1;

    if (opt_no_db)
	return;

    if (!mrp || !mrp->mr_target) {
	return;
    }

    if (sqlite3_prepare_v2(mx_db_handle, "SELECT id, save_password FROM "
		" devices WHERE name = ?", -1, &stmt, NULL) != SQLITE_OK) {
	mx_log("db error getting save_password value: %s",
		sqlite3_errmsg(mx_db_handle));
	return;
    }
    if (sqlite3_bind_text(stmt, 1, mrp->mr_target, -1,
		SQLITE_STATIC) != SQLITE_OK) {
	mx_log("db error getting save_password value: %s",
		sqlite3_errmsg(mx_db_handle));
	goto cleanup;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
	row_id = sqlite3_column_int(stmt, 0);
	save_password = sqlite3_column_int(stmt, 1);
    }

    sqlite3_finalize(stmt);

    if (save_password && row_id > -1) {
	if (sqlite3_prepare_v2(mx_db_handle,
		    "UPDATE devices SET password = ? WHERE id = ?", -1, &stmt,
		    NULL) != SQLITE_OK) {
	    mx_log("db error saving password: %s",
		    sqlite3_errmsg(mx_db_handle));
	    return;
	}
	if (sqlite3_bind_text(stmt, 1, password, -1,
		    SQLITE_STATIC) != SQLITE_OK) {
	    mx_log("db error saving password: %s",
		sqlite3_errmsg(mx_db_handle));
	    goto cleanup;
	}
	if (sqlite3_bind_int(stmt, 2, row_id) != SQLITE_OK) {
	    mx_log("db error saving password: %s",
		sqlite3_errmsg(mx_db_handle));
	    goto cleanup;
	}
	sqlite3_step(stmt);
    } else {
	return;
    }

cleanup:

    sqlite3_finalize(stmt);
}

/*
 * Look up target information from the db and populate the request with the
 * information.
 *
 * Return FALSE if could not lookup target or target does not exist
 * Return TRUE if target looked up successfully
 */
mx_boolean_t
mx_db_target_lookup (const char *target, mx_request_t *mrp)
{
    int rc, retval = FALSE, port = -1;
    sqlite3_stmt *stmt;
    char *cp;

    if (!target) {
	return FALSE;
    }

    mrp->mr_target = nstrdup(target);

    /*
     * Clean up target - strip off [user@] and [:port]
     *
     * Override user with [user@]
     */
    cp = index(mrp->mr_target, '@');
    if (cp) {
	mrp->mr_user = mrp->mr_target;
	*cp++ = '\0';
	mrp->mr_target = strdup(cp);
    }

    /*
     * Override port with [:port]
     */
    cp = index(mrp->mr_target, ':');
    if (cp) {
	port = strtol(cp + 1, NULL, 10);
	*cp = '\0';
    }

    if (port == -1) {
	mrp->mr_port = opt_destport;
    } else {
	mrp->mr_port = port;
    }

    if (opt_no_db)
	return FALSE;

    /*
     * See if we have an entry
     */
    rc = sqlite3_prepare_v2(mx_db_handle,
	    "SELECT * FROM devices WHERE name = ?", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
	mx_log("Could not look up '%s' from 'devices' table: %s",
		mrp->mr_target, sqlite3_errmsg(mx_db_handle));
	goto cleanup;
    }
    if (sqlite3_bind_text(stmt, 1, mrp->mr_target, -1, SQLITE_STATIC) !=
	    SQLITE_OK) {
	mx_log("Could not look up '%s' from 'devices' table: %s",
		mrp->mr_target, sqlite3_errmsg(mx_db_handle));
	goto cleanup;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
	mrp->mr_hostname = nstrdup((const char *) sqlite3_column_text(stmt,
		    2));
	/*
	 * If we are not overriding the user, use the stored credentials.
	 * If we are overriding the user [user@], then do not use stored
	 * credentials.
	 */
	if (!mrp->mr_user) {
	    mrp->mr_user = nstrdup((const char *) sqlite3_column_text(stmt,
			4));
	    mrp->mr_password = nstrdup((const char *) sqlite3_column_text(stmt,
			5));
	}
	if (port == -1) {
	    mrp->mr_port = sqlite3_column_int(stmt, 3);
	}

	retval = TRUE;
    }

cleanup:

    if (mrp->mr_port == 0) {
	mrp->mr_port = opt_destport;
    }

    sqlite3_finalize(stmt);

    return retval;
}

/*
 * Upgrade the database schema (if necessary)
 *
 * Return FALSE if could not upgrade (too new a db for this ver of mixer, etc)
 * Return TRUE if upgrade suceeded/not necessary
 */
static mx_boolean_t
mx_db_upgrade (int version)
{
    if (version > MX_DB_CURRENT_VERSION) {
	mx_log("Database (%s) is at version %d.  This mixer supports "
		"up to version %d.  Please upgrade mixer.",
		opt_db, version, MX_DB_CURRENT_VERSION);
	return FALSE;
    }

    if (version == MX_DB_CURRENT_VERSION) {
	return TRUE;
    }

    /*
     * Perform future schema upgrades here...
     */

    return TRUE;
}

/*
 * Open the database for read/write.
 */
static int
mx_db_open (void)
{
    if (sqlite3_open(opt_db, &mx_db_handle) != SQLITE_OK) {
	mx_log("Could not open database '%s' for read/write.  "
		"Critical error!", opt_db);
	return FALSE;
    }

    return TRUE;
}

/*
 * Initialize the database that stores our devices/hostkeys.  If it doesn't
 * exist, create it.
 *
 * Return FALSE if failed, TRUE if successful
 */
int
mx_db_init (void)
{
    int rc, version = 0;
    char buf[BUFSIZ];
    sqlite3_stmt *stmt;
    struct passwd *pw = NULL;

    /* 
     * Use default ~user/.juise/mixer.db as the database
     */
    if (!opt_db) {
	pw = getpwuid(geteuid());
	if (!pw) {
	    mx_log("Could not determine home directory of user");
	    return FALSE;
	}
	snprintf(buf, sizeof(buf), "%s/.juise/mixer.db", pw->pw_dir);
	opt_db = strdup(buf);
    }

    if (!mx_db_open()) {
	return FALSE;
    }

    /*
     * See if this db has the proper tables
     */
    rc = sqlite3_prepare_v2(mx_db_handle, "SELECT * FROM general", -1, &stmt,
	    NULL);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
	/*
	 * We have the version table, which mean's we're good to go.  Save
	 * the version and continue to see if we need to upgrade the DB.
	 */
	version = sqlite3_column_int(stmt, 0);

	sqlite3_finalize(stmt);

	if (!mx_db_upgrade(version)) {
	    return FALSE;
	}

	mx_log("Opened database (%s; version: %d)", opt_db, version);

	return TRUE;
    } else {
	sqlite3_finalize(stmt);
    }

    /* Need to create the database below */
    mx_log("Creating initial database (%s)", opt_db);

    /*
     * Create 'devices' table
     */
    sqlite3_exec(mx_db_handle,
	    "CREATE TABLE \"devices\" ("
	    "    \"id\" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
	    "    \"name\" VARCHAR,"
	    "    \"hostname\" VARCHAR,"
	    "    \"port\" INTEGER,"
	    "    \"username\" VARCHAR,"
	    "    \"password\" VARCHAR,"
	    "    \"save_password\" BOOL DEFAULT 1,"
	    "    UNIQUE(name)"
	    ")", NULL, NULL, NULL);

    /*
     * Create 'hostkeys' table
     */
    sqlite3_exec(mx_db_handle,
	    "CREATE TABLE \"hostkeys\" ("
	    "    \"id\" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
	    "    \"name\" VARCHAR,"
	    "    \"type\" INTEGER,"
	    "    \"hostkey\" VARCHAR"
	    ")", NULL, NULL, NULL);
    
    /*
     * Create 'groups' table
     */
    sqlite3_exec(mx_db_handle,
	    "CREATE TABLE \"groups\" ("
	    "    \"id\" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
	    "    \"name\" VARCHAR"
	    ")", NULL, NULL, NULL);

    /*
     * Create 'groups_members' table
     */
    sqlite3_exec(mx_db_handle,
	    "CREATE TABLE \"groups_members\" ("
	    "    \"id\" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
	    "    \"group_id\" INTEGER NOT NULL,"
	    "    \"device_id\" INTEGER NOT NULL"
	    ")", NULL, NULL, NULL);

    /*
     * Create 'general' table and insert DB version
     */
    sqlite3_exec(mx_db_handle,
	    "CREATE TABLE \"general\" ("
	    "    \"version\" INTEGER,"
	    "    \"privatekey\" BLOB,"
	    "    \"publickey\" BLOB,"
	    "    \"passphrase\" VARCHAR,"
	    "    \"save_passphrase\" BOOL DEFAULT 1"
	    ")", NULL, NULL, NULL);
    snprintf(buf, sizeof(buf),
	    "INSERT INTO \"general\" (version, save_passphrase) "
	    "VALUES (\"%d\", 1)",
	    MX_DB_CURRENT_VERSION);
    sqlite3_exec(mx_db_handle, buf, NULL, NULL, NULL);

    return TRUE;
}

void
mx_db_close (void)
{
    if (mx_db_handle) {
	sqlite3_close(mx_db_handle);
	mx_db_handle = NULL;
    }
}
