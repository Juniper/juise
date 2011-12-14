/*
 * $Id$
 *
 * Copyright (c) 1998-2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#ifndef SHARED_ENV_PATHS_H
#define SHARED_ENV_PATHS_H

/**
*  @file end_paths.h 
*  @brief Shared path definitions.
* 
*/

#define PATH_TMP_DIR            "/tmp/"
#define PATH_ETC_DIR            "/etc/"
#define PATH_VAR_DIR            "/var"
#define PATH_VAR_ETC_DIR        "/var/etc/"
#define PATH_VAR_TMP_DIR	"/var/tmp/"
#define PATH_VAR_CRASH_DIR	"/var/crash/"
#define PATH_VAR_RUN_DIR	"/var/run/"
#define PATH_VAR_RUN_EXT_DIR	"/var/run/ext/" 
#define PATH_VAR_DB_EXT_DIR	"/var/db/ext/"
#define PATH_USR_SBIN		"/usr/sbin/"
#define PATH_OPT_SBIN		"/opt/sbin/"
#define PATH_SBIN		"/sbin/"
#define PATH_PACKAGES_MNT_DIR	"/packages/mnt/"
#define PATH_OPT_SDK_DIR	"/opt/sdk/"

#define PATH_PACKAGE_LOCK	PATH_TMP_DIR ".pkg.LCK"
#define PATH_PACKAGE_REBOOT	PATH_TMP_DIR ".pkg.REBOOT"

#define PATH_CONF_DIR		PATH_VAR_ETC_DIR
#define PATH_PIDFILE_DIR	PATH_VAR_RUN_DIR
#define PATH_PIDFILE_EXT_DIR	PATH_VAR_RUN_EXT_DIR
#define PATH_DAEMON_CWD		PATH_VAR_TMP_DIR
#define PATH_MGMT_DIR		PATH_VAR_RUN_DIR
#define PATH_MGMT_EXT_DIR	PATH_VAR_RUN_EXT_DIR
#define PATH_SDK_BASE_DIR	PATH_OPT_SDK_DIR

#define PATH_VAR_LOG_EXT	"/var/log/ext/"

#endif /* SHARED_ENV_PATHS_H */
