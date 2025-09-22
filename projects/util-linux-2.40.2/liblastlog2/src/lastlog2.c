/* SPDX-License-Identifier: BSD-2-Clause

  Copyright (c) 2023, Thorsten Kukuk <kukuk@suse.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

#include <pwd.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <lastlog.h>

#include "lastlog2P.h"
#include "strutils.h"

/* Set the ll2 context/environment */
/* Returns the context or NULL if an error has happened. */
extern struct ll2_context * ll2_new_context(const char *db_path)
{
	struct ll2_context *context = (struct ll2_context *)malloc(sizeof(struct ll2_context));

	if (context) {
		if (db_path) {
			if ((context->lastlog2_path = strdup(db_path)) == NULL) {
				free(context);
				context = NULL;
			}
		} else {
			if ((context->lastlog2_path = strdup(LL2_DEFAULT_DATABASE)) == NULL) {
				free(context);
				context = NULL;
			}
		}
	}
	return context;
}

/* Release ll2 context/environment */
extern void ll2_unref_context(struct ll2_context *context)
{
	if (context)
		free(context->lastlog2_path);
	free(context);
}

/* Returns 0 on success, -ENOMEM or -1 on other failure. */
static int
open_database_ro(struct ll2_context *context, sqlite3 **db, char **error)
{
	int ret = 0;
	char *path = LL2_DEFAULT_DATABASE;

	if (context && context->lastlog2_path)
		path = context->lastlog2_path;

	if (sqlite3_open_v2(path, db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
		ret = -1;
		if (error)
			if (asprintf(error, "Cannot open database (%s): %s",
				     path, sqlite3_errmsg(*db)) < 0)
				ret = -ENOMEM;

		sqlite3_close(*db);
	}

	return ret;
}

/* Returns 0 on success, -ENOMEM or -1 on other failure. */
static int
open_database_rw(struct ll2_context *context,  sqlite3 **db, char **error)
{
	int ret = 0;
	char *path = LL2_DEFAULT_DATABASE;

	if (context && context->lastlog2_path)
		path = context->lastlog2_path;

	if (sqlite3_open(path, db) != SQLITE_OK) {
		ret = -1;
		if (error)
			if (asprintf(error, "Cannot create/open database (%s): %s",
				     path, sqlite3_errmsg(*db)) < 0)
				ret = -ENOMEM;

		sqlite3_close(*db);
	}

	return ret;
}

/* Reads one entry from database and returns that.
   Returns 0 on success, -ENOMEM or -1 on other failure. */
static int
read_entry(sqlite3 *db, const char *user,
	   int64_t *ll_time, char **tty, char **rhost,
	   char **pam_service, char **error)
{
	int retval = 0;
	sqlite3_stmt *res = NULL;
	static const char *sql = "SELECT Name,Time,TTY,RemoteHost,Service FROM Lastlog2 WHERE Name = ?";

	if (sqlite3_prepare_v2(db, sql, -1, &res, 0) != SQLITE_OK) {
		retval = -1;
		if (error)
			if (asprintf(error, "Failed to execute statement: %s",
				     sqlite3_errmsg(db)) < 0)
				retval = -ENOMEM;
		goto out_read_entry;
	}

	if (sqlite3_bind_text(res, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		retval = -1;
		if (error)
			if (asprintf(error, "Failed to create search query: %s",
				     sqlite3_errmsg(db)) < 0)
				retval = -ENOMEM;
		goto out_read_entry;
	}

	int step = sqlite3_step(res);

	if (step == SQLITE_ROW) {
		const unsigned char *luser = sqlite3_column_text(res, 0);
		const unsigned char *uc;

		if (strcmp((const char *)luser, user) != 0) {
			retval = -1;
			if (error)
				if (asprintf(error, "Returned data is for %s, not %s", luser, user) < 0)
				retval = -ENOMEM;
			goto out_read_entry;
		}

		if (ll_time)
			*ll_time = sqlite3_column_int64(res, 1);

		if (tty) {
			uc = sqlite3_column_text(res, 2);
			if (uc != NULL && strlen((const char *)uc) > 0)
				if ((*tty = strdup((const char *)uc)) == NULL) {
					retval = -ENOMEM;
					goto out_read_entry;
				}
		}
		if (rhost) {
			uc = sqlite3_column_text(res, 3);
			if (uc != NULL && strlen((const char *)uc) > 0)
				if ((*rhost = strdup((const char *)uc)) == NULL) {
					retval = -ENOMEM;
					goto out_read_entry;
				}
		}
		if (pam_service) {
			uc = sqlite3_column_text(res, 4);
			if (uc != NULL && strlen((const char *)uc) > 0)
				if ((*pam_service = strdup((const char *)uc)) == NULL) {
					retval = -ENOMEM;
					goto out_read_entry;
				}
		}
	} else {
		retval = -1;
		if (error)
			if (asprintf(error, "User '%s' not found (%d)", user, step) < 0)
				retval = -ENOMEM;
	}

out_read_entry:
	if (res)
		sqlite3_finalize(res);

	return retval;
}

/* reads 1 entry from database and returns that. Returns 0 on success, -ENOMEM or -1 on other failure. */
int
ll2_read_entry(struct ll2_context *context, const char *user,
	       int64_t *ll_time, char **tty, char **rhost,
	       char **pam_service, char **error)
{
	sqlite3 *db;
	int retval;

	if ((retval = open_database_ro(context, &db, error)) != 0)
		return retval;

	retval = read_entry(db, user, ll_time, tty, rhost, pam_service, error);

	sqlite3_close(db);

	return retval;
}

/* Write a new entry. Returns 0 on success, -ENOMEM or -1 on other failure. */
static int
write_entry(sqlite3 *db, const char *user,
	    int64_t ll_time, const char *tty, const char *rhost,
	    const char *pam_service, char **error)
{
	int retval = 0;
	char *err_msg = NULL;
	sqlite3_stmt *res = NULL;
	static const char *sql_table = "CREATE TABLE IF NOT EXISTS Lastlog2(Name TEXT PRIMARY KEY, Time INTEGER, TTY TEXT, RemoteHost TEXT, Service TEXT);";
	static const char *sql_replace = "REPLACE INTO Lastlog2 VALUES(?,?,?,?,?);";

	if (sqlite3_exec(db, sql_table, 0, 0, &err_msg) != SQLITE_OK) {
		retval = -1;
		if (error)
			if (asprintf(error, "SQL error: %s", err_msg) < 0)
				retval = -ENOMEM;

		sqlite3_free(err_msg);
		goto out_ll2_read_entry;
	}

	if (sqlite3_prepare_v2(db, sql_replace, -1, &res, 0) != SQLITE_OK) {
		retval = -1;
		if (error)
			if (asprintf(error, "Failed to execute statement: %s",
				     sqlite3_errmsg(db)) < 0)
				retval = -ENOMEM;
		goto out_ll2_read_entry;
	}

	if (sqlite3_bind_text(res, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		retval = -1;
		if (error)
			if (asprintf(error, "Failed to create replace statement for user: %s",
				     sqlite3_errmsg(db)) < 0)
				retval = -ENOMEM;
		goto out_ll2_read_entry;
	}

	if (sqlite3_bind_int64(res, 2, ll_time) != SQLITE_OK) {
		retval = -1;
		if (error)
			if (asprintf(error, "Failed to create replace statement for ll_time: %s",
				     sqlite3_errmsg(db)) < 0)
				retval = -ENOMEM;
		goto out_ll2_read_entry;
	}

	if (sqlite3_bind_text(res, 3, tty, -1, SQLITE_STATIC) != SQLITE_OK) {
		retval = -1;
		if (error)
			if (asprintf(error, "Failed to create replace statement for tty: %s",
				     sqlite3_errmsg(db)) < 0)
				retval = -ENOMEM;
		goto out_ll2_read_entry;
	}

	if (sqlite3_bind_text(res, 4, rhost, -1, SQLITE_STATIC) != SQLITE_OK) {
		retval = -1;
		if (error)
			if (asprintf(error, "Failed to create replace statement for rhost: %s",
				     sqlite3_errmsg(db)) < 0)
				retval = -ENOMEM;
		goto out_ll2_read_entry;
	}

	if (sqlite3_bind_text(res, 5, pam_service, -1, SQLITE_STATIC) != SQLITE_OK) {
		retval = -1;
		if (error)
			if (asprintf(error, "Failed to create replace statement for PAM service: %s",
				     sqlite3_errmsg(db)) < 0)
				retval = -ENOMEM;
		goto out_ll2_read_entry;
	}

	int step = sqlite3_step(res);

	if (step != SQLITE_DONE) {
		retval = -1;
		if (error)
			if (asprintf(error, "Delete statement did not return SQLITE_DONE: %d",
				     step) < 0)
				retval = -ENOMEM;
		goto out_ll2_read_entry;
	}
out_ll2_read_entry:
	if (res)
		sqlite3_finalize(res);

	return retval;
}

/* Write a new entry. Returns 0 on success, -ENOMEM or -1 on other failure. */
int
ll2_write_entry(struct ll2_context *context, const char *user,
		int64_t ll_time, const char *tty, const char *rhost,
		const char *pam_service, char **error)
{
	sqlite3 *db;
	int retval;

	if ((retval = open_database_rw(context, &db, error)) != 0)
		return retval;

	retval = write_entry(db, user, ll_time, tty, rhost, pam_service, error);

	sqlite3_close(db);

	return retval;
}

/* Write a new entry with updated login time.
   Returns 0 on success, -ENOMEM or -1 on other failure. */
int
ll2_update_login_time(struct ll2_context *context, const char *user,
		      int64_t ll_time, char **error)
{
	sqlite3 *db;
	int retval;
	char *tty;
	char *rhost;
	char *pam_service;

	if ((retval = open_database_rw(context , &db, error)) != 0)
		return retval;

	if ((retval = read_entry(db, user, 0, &tty, &rhost, &pam_service, error)) != 0) {
		sqlite3_close(db);
		return retval;
	}

	retval = write_entry(db, user, ll_time, tty, rhost, pam_service, error);

	sqlite3_close(db);

	free(tty);
	free(rhost);
	free(pam_service);

	return retval;
}


typedef int (*callback_f)(const char *user, int64_t ll_time,
			  const char *tty, const char *rhost,
			  const char *pam_service, const char *cb_error);

static int
callback(void *cb_func, __attribute__((unused)) int argc, char **argv, __attribute__((unused)) char **azColName)
{
	char *endptr;
	callback_f print_entry = cb_func;

	errno = 0;
	char *cb_error = NULL;
	int64_t ll_time = strtoll(argv[1], &endptr, 10);
	if ((errno == ERANGE && (ll_time == INT64_MAX || ll_time == INT64_MIN))
	    || (endptr == argv[1]) || (*endptr != '\0'))
		if (asprintf(&cb_error, "Invalid numeric time entry for '%s': '%s'\n", argv[0], argv[1]) < 0)
			return -1;

	print_entry(argv[0], ll_time, argv[2], argv[3], argv[4], cb_error);
	free(cb_error);

	return 0;
}

/* Reads all entries from database and calls the callback function for each entry.
   Returns 0 on success, -ENOMEM or -1 on other failure. */
int
ll2_read_all(struct ll2_context *context,
	     int (*cb_func)(const char *user, int64_t ll_time,
			    const char *tty, const char *rhost,
			    const char *pam_service, const char *cb_error),
	     char **error)
{
	sqlite3 *db;
	char *err_msg = 0;
	int retval = 0;

	if ((retval = open_database_ro(context, &db, error)) != 0)
		return retval;

	static const char *sql = "SELECT Name,Time,TTY,RemoteHost,Service FROM Lastlog2 ORDER BY Name ASC";

	if (sqlite3_exec(db, sql, callback, cb_func, &err_msg) != SQLITE_OK) {
		retval = -1;
		if (error)
			if (asprintf(error, "SQL error: %s", err_msg) < 0)
				retval = -ENOMEM;

		sqlite3_free(err_msg);
	}

	sqlite3_close(db);

	return retval;
}

/* Remove an user entry. Returns 0 on success, -ENOMEM or -1 on other failure. */
static int
remove_entry(sqlite3 *db, const char *user, char **error)
{
	int retval = 0;
	sqlite3_stmt *res = NULL;
	static const char *sql = "DELETE FROM Lastlog2 WHERE Name = ?";

	if (sqlite3_prepare_v2(db, sql, -1, &res, 0) != SQLITE_OK) {
		if (error)
			if (asprintf(error, "Failed to execute statement: %s",
				     sqlite3_errmsg(db)) < 0)
				return -ENOMEM;

		return -1;
	}

	if (sqlite3_bind_text(res, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		retval = -1;
		if (error)
			if (asprintf(error, "Failed to create delete statement: %s",
				     sqlite3_errmsg(db)) < 0)
				retval = -ENOMEM;
		goto out_remove_entry;
	}

	int step = sqlite3_step(res);

	if (step != SQLITE_DONE) {
		retval = -1;
		if (error)
			if (asprintf(error, "Delete statement did not return SQLITE_DONE: %d",
				     step) < 0)
				retval = -ENOMEM;
	}
out_remove_entry:
	if (res)
		sqlite3_finalize(res);

	return retval;
}

/* Remove an user entry. Returns 0 on success, -ENOMEM or -1 on other failure. */
int
ll2_remove_entry(struct ll2_context *context, const char *user,
		 char **error)
{
	sqlite3 *db;
	int retval;

	if ((retval = open_database_rw(context, &db, error)) != 0)
		return retval;

	retval = remove_entry(db, user, error);

	sqlite3_close(db);

	return retval;
}

/* Renames an user entry. Returns 0 on success, -ENOMEM or -1 on other failure. */
int
ll2_rename_user(struct ll2_context *context, const char *user,
		const char *newname, char **error)
{
	sqlite3 *db;
	int64_t ll_time;
	char *tty;
	char *rhost;
	char *pam_service;
	int retval;

	if ((retval = open_database_rw(context, &db, error)) != 0)
		return retval;

	if ((retval = read_entry(db, user, &ll_time, &tty, &rhost, &pam_service, error) != 0)) {
		sqlite3_close(db);
		return retval;
	}

	if ((retval = write_entry(db, newname, ll_time, tty, rhost, pam_service, error) != 0)) {
		sqlite3_close(db);
		free(tty);
		free(rhost);
		return retval;
	}

	retval = remove_entry(db, user, error);

	sqlite3_close(db);

	free(tty);
	free(rhost);
	free(pam_service);

	return retval;
}

/* Import old lastlog file.
   Returns 0 on success, -ENOMEM or -1 on other failure. */
int
ll2_import_lastlog(struct ll2_context *context, const char *lastlog_file,
		   char **error)
{
	const struct passwd *pw;
	struct stat statll;
	sqlite3 *db;
	FILE *ll_fp;
	int retval = 0;

	if ((retval = open_database_rw(context, &db, error)) != 0)
		return retval;

	ll_fp = fopen(lastlog_file, "r");
	if (ll_fp == NULL) {
		if (error && asprintf(error, "Failed to open '%s': %s",
				     lastlog_file, strerror(errno)) < 0)
			return -ENOMEM;

		return -1;
	}


	if (fstat(fileno(ll_fp), &statll) != 0) {
		retval = -1;
		if (error && asprintf(error, "Cannot get size of '%s': %s",
					lastlog_file, strerror(errno)) < 0)
			retval = -ENOMEM;

		goto done;
	}

	setpwent();
	while ((pw = getpwent()) != NULL ) {
		off_t offset;
		struct lastlog ll;

		offset = (off_t) pw->pw_uid * sizeof (ll);

		if ((offset + (off_t)sizeof(ll)) <= statll.st_size) {
			if (fseeko(ll_fp, offset, SEEK_SET) == -1)
				continue; /* Ignore seek error */

			if (fread(&ll, sizeof(ll), 1, ll_fp) != 1) {
				retval = -1;
				if (error)
					if (asprintf(error, "Failed to get the entry for UID '%lu'",
						     (unsigned long int)pw->pw_uid) < 0)
						retval = -ENOMEM;
				goto out_import_lastlog;
			}

			if (ll.ll_time != 0) {
				int64_t ll_time;
				char tty[sizeof(ll.ll_line) + 1];
				char rhost[sizeof(ll.ll_host) + 1];

				ll_time = ll.ll_time;
				mem2strcpy(tty, ll.ll_line, sizeof(ll.ll_line), sizeof(tty));
				mem2strcpy(rhost, ll.ll_host, sizeof(ll.ll_host), sizeof(rhost));

				if ((retval = write_entry(db, pw->pw_name, ll_time, tty,
							  rhost, NULL, error)) != 0)
					goto out_import_lastlog;
			}
		}
	}
out_import_lastlog:
	endpwent();
	sqlite3_close(db);
done:
	fclose(ll_fp);

	return retval;
}
