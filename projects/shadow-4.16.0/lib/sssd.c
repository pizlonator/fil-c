/* Author: Peter Vrabec <pvrabec@redhat.com> */

#include <config.h>
#ifdef USE_SSSD

#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "alloc.h"
#include "exitcodes.h"
#include "defines.h"
#include "prototypes.h"
#include "sssd.h"

#include "shadowlog_internal.h"

#define MSG_SSSD_FLUSH_CACHE_FAILED "%s: Failed to flush the sssd cache."

int sssd_flush_cache (int dbflags)
{
	int status, code, rv;
	const char *cmd = "/usr/sbin/sss_cache";
	struct stat sb;
	char *sss_cache_args = NULL;
	const char *spawnedArgs[] = {"sss_cache", NULL, NULL};
	const char *spawnedEnv[] = {NULL};
	int i = 0;

	rv = stat(cmd, &sb);
	if (rv == -1 && errno == ENOENT)
		return 0;

	sss_cache_args = MALLOC(4, char);
	if (sss_cache_args == NULL) {
	    return -1;
	}

	sss_cache_args[i++] = '-';
	if (dbflags & SSSD_DB_PASSWD) {
		sss_cache_args[i++] = 'U';
	}
	if (dbflags & SSSD_DB_GROUP) {
		sss_cache_args[i++] = 'G';
	}
	sss_cache_args[i++] = '\0';
	if (i == 2) {
		/* Neither passwd nor group, nothing to do */
		free(sss_cache_args);
		return 0;
	}
	spawnedArgs[1] = sss_cache_args;

	rv = run_command (cmd, spawnedArgs, spawnedEnv, &status);
	free(sss_cache_args);
	if (rv != 0) {
		/* run_command writes its own more detailed message. */
		SYSLOG ((LOG_WARN, MSG_SSSD_FLUSH_CACHE_FAILED, shadow_progname));
		return -1;
	}

	code = WEXITSTATUS (status);
	if (!WIFEXITED (status)) {
		SYSLOG ((LOG_WARN, "%s: sss_cache did not terminate normally (signal %d)",
			shadow_progname, WTERMSIG (status)));
		return -1;
	} else if (code == E_CMD_NOTFOUND) {
		/* sss_cache is not installed, or it is installed but uses an
		   interpreter that is missing.  Probably the former. */
		return 0;
	} else if (code != 0) {
		SYSLOG ((LOG_WARN, "%s: sss_cache exited with status %d", shadow_progname, code));
		SYSLOG ((LOG_WARN, MSG_SSSD_FLUSH_CACHE_FAILED, shadow_progname));
		return -1;
	}

	return 0;
}
#else				/* USE_SSSD */
extern int ISO_C_forbids_an_empty_translation_unit;
#endif				/* USE_SSSD */

