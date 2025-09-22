/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1998, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2002 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008 - 2010, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "defines.h"
#include "faillog.h"
#include "failure.h"
#include "memzero.h"
#include "prototypes.h"
#include "string/strftime.h"
#include "string/strtcpy.h"


#define	YEAR	(365L*DAY)
/*
 * failure - make failure entry
 *
 *	failure() creates a new (struct faillog) entry or updates an
 *	existing one with the current failed login information.
 */
void failure (uid_t uid, const char *tty, struct faillog *fl)
{
	int fd;
	off_t offset_uid = (off_t) (sizeof *fl) * uid;

	/*
	 * Don't do anything if failure logging isn't set up.
	 */

	if (access (FAILLOG_FILE, F_OK) != 0) {
		return;
	}

	fd = open (FAILLOG_FILE, O_RDWR);
	if (fd < 0) {
		SYSLOG ((LOG_WARN,
		         "Can't write faillog entry for UID %lu in %s: %m",
		         (unsigned long) uid, FAILLOG_FILE));
		return;
	}

	/*
	 * The file is indexed by UID value meaning that shared UID's
	 * share failure log records.  That's OK since they really
	 * share just about everything else ...
	 */

	if (   (lseek (fd, offset_uid, SEEK_SET) != offset_uid)
	    || (read (fd, fl, sizeof *fl) != (ssize_t) sizeof *fl)) {
		/* This is not necessarily a failure. The file is
		 * initially zero length.
		 *
		 * If lseek() or read() failed for any other reason, this
		 * might reset the counter. But the new failure will be
		 * logged.
		 */
		memzero (fl, sizeof *fl);
	}

	/*
	 * Update the record.  We increment the failure count to log the
	 * latest failure.  The only concern here is overflow, and we'll
	 * check for that.  The line name and time of day are both
	 * updated as well.
	 */

	if (fl->fail_cnt + 1 > 0) {
		fl->fail_cnt++;
	}

	STRTCPY(fl->fail_line, tty);
	(void) time (&fl->fail_time);

	/*
	 * Seek back to the correct position in the file and write the
	 * record out.  Ideally we should lock the file in case the same
	 * account is being logged simultaneously.  But the risk doesn't
	 * seem that great.
	 */

	if (   (lseek (fd, offset_uid, SEEK_SET) != offset_uid)
	    || (write_full(fd, fl, sizeof *fl) == -1)) {
		goto err_write;
	}

	if (close (fd) != 0 && errno != EINTR) {
		goto err_close;
	}

	return;

err_write:
	{
		int saved_errno = errno;
		(void) close (fd);
		errno = saved_errno;
	}
err_close:
	SYSLOG ((LOG_WARN,
	         "Can't write faillog entry for UID %lu to %s: %m",
	         (unsigned long) uid, FAILLOG_FILE));
}

static bool too_many_failures (const struct faillog *fl)
{
	time_t now;

	if ((0 == fl->fail_max) || (fl->fail_cnt < fl->fail_max)) {
		return false;
	}

	if (0 == fl->fail_locktime) {
		return true;	/* locked until reset manually */
	}

	(void) time (&now);
	if ((fl->fail_time + fl->fail_locktime) < now) {
		return false;	/* enough time since last failure */
	}

	return true;
}

/*
 * failcheck - check for failures > allowable
 *
 *	failcheck() is called AFTER the password has been validated.  If the
 *	account has been "attacked" with too many login failures, failcheck()
 *	returns 0 to indicate that the login should be denied even though
 *	the password is valid.
 *
 *	failed indicates if the login failed AFTER the password has been
 *	       validated.
 */

int failcheck (uid_t uid, struct faillog *fl, bool failed)
{
	int fd;
	struct faillog fail;
	off_t offset_uid = (off_t) (sizeof *fl) * uid;

	/*
	 * Suppress the check if the log file isn't there.
	 */

	if (access (FAILLOG_FILE, F_OK) != 0) {
		return 1;
	}

	fd = open (FAILLOG_FILE, failed?O_RDONLY:O_RDWR);
	if (fd < 0) {
		SYSLOG ((LOG_WARN,
		         "Can't open the faillog file (%s) to check UID %lu: %m; "
		         "User access authorized.",
		         FAILLOG_FILE, (unsigned long) uid));
		return 1;
	}

	/*
	 * Get the record from the file and determine if the user has
	 * exceeded the failure limit.  If "max" is zero, any number
	 * of failures are permitted.  Only when "max" is non-zero and
	 * "cnt" is greater than or equal to "max" is the account
	 * considered to be locked.
	 *
	 * If read fails, there is no record for this user yet (the
	 * file is initially zero length and extended by writes), so
	 * no need to reset the count.
	 */

	if (   (lseek (fd, offset_uid, SEEK_SET) != offset_uid)
	    || (read (fd, fl, sizeof *fl) != (ssize_t) sizeof *fl)) {
		(void) close (fd);
		return 1;
	}

	if (too_many_failures (fl)) {
		(void) close (fd);
		return 0;
	}

	/*
	 * The record is updated if this is not a failure.  The count will
	 * be reset to zero, but the rest of the information will be left
	 * in the record in case someone wants to see where the failed
	 * login originated.
	 */

	if (!failed) {
		fail = *fl;
		fail.fail_cnt = 0;

		if (   (lseek (fd, offset_uid, SEEK_SET) != offset_uid)
		    || (write_full(fd, &fail, sizeof fail) == -1)) {
			    goto err_write;
		}

		if (close (fd) != 0 && errno != EINTR) {
			goto err_close;
		}
	} else {
		(void) close (fd);
	}

	return 1;

err_write:
	{
		int saved_errno = errno;
		(void) close (fd);
		errno = saved_errno;
	}
err_close:
	SYSLOG ((LOG_WARN,
	         "Can't reset faillog entry for UID %lu in %s: %m",
	         (unsigned long) uid, FAILLOG_FILE));
	return 1;
}

/*
 * failprint - print line of failure information
 *
 *	failprint takes a (struct faillog) entry and formats it into a
 *	message which is displayed at login time.
 */

void failprint (const struct faillog *fail)
{
	struct tm *tp;
	char lasttimeb[256];
	char *lasttime = lasttimeb;
	time_t NOW;

	if (0 == fail->fail_cnt) {
		return;
	}

	tp = localtime (&(fail->fail_time));
	(void) time (&NOW);

	/*
	 * Print all information we have.
	 */
	STRFTIME(lasttimeb, "%c", tp);

	/*@-formatconst@*/
	(void) printf (ngettext ("%d failure since last login.\n"
	                         "Last was %s on %s.\n",
	                         "%d failures since last login.\n"
	                         "Last was %s on %s.\n",
	                         (unsigned long) fail->fail_cnt),
	               fail->fail_cnt, lasttime, fail->fail_line);
	/*@=formatconst@*/
}
