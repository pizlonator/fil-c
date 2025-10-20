/*
 * $Id$
 *
 * This function provides a thread safer version of getpwnam() for use
 * with PAM modules that care about this sort of thing.
 *
 * XXX - or at least it should provide a thread-safe alternative.
 */

#include "pam_modutil_private.h"

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>

static int intlen(int number)
{
    int len = 2;
    while (number != 0) {
        number /= 10;
	len++;
    }
    return len;
}

struct passwd *
pam_modutil_getpwnam(pam_handle_t *pamh, const char *user)
{
#ifdef HAVE_GETPWNAM_R

    void *buffer=NULL;
    size_t length = PWD_INITIAL_LENGTH;

    do {
	int status;
	void *new_buffer;
	struct passwd *result = NULL;

	new_buffer = realloc(buffer, sizeof(struct passwd) + length);
	if (new_buffer == NULL) {

	    D(("out of memory"));

	    /* no memory for the user - so delete the memory */
	    free(buffer);
	    return NULL;
	}
	buffer = new_buffer;

	/* make the re-entrant call to get the pwd structure */
	errno = 0;
	status = getpwnam_r(user, buffer,
			    sizeof(struct passwd) + (char *) buffer,
			    length, &result);
	if (!status && (result == buffer)) {
	    char *data_name;
	    const void *ignore;
	    int i;

	    data_name = malloc(strlen("_pammodutil_getpwnam") + 1 +
			       strlen(user) + 1 + intlen(INT_MAX) + 1);
	    if ((pamh != NULL) && (data_name == NULL)) {
	        D(("was unable to register the data item [%s]",
	           pam_strerror(pamh, status)));
		free(buffer);
		return NULL;
	    }

	    if (pamh != NULL) {
	        for (i = 0; i < INT_MAX; i++) {
	            sprintf(data_name, "_pammodutil_getpwnam_%s_%d", user, i);
		    status = PAM_NO_MODULE_DATA;
	            if (pam_get_data(pamh, data_name, &ignore) != PAM_SUCCESS) {
		        status = pam_set_data(pamh, data_name,
					      result, pam_modutil_cleanup);
		    }
		    if (status == PAM_SUCCESS) {
		        break;
		    }
		}
	    } else {
	        status = PAM_SUCCESS;
	    }

	    free(data_name);

	    if (status == PAM_SUCCESS) {
		D(("success"));
		return result;
	    }

	    D(("was unable to register the data item [%s]",
	       pam_strerror(pamh, status)));

	    free(buffer);
	    return NULL;

	} else if (errno != ERANGE && errno != EINTR) {
                /* no sense in repeating the call */
                break;
        }

	length <<= PWD_LENGTH_SHIFT;

    } while (length < PWD_ABSURD_PWD_LENGTH);

    D(("pwd structure took %zu bytes or so of memory",
       length+sizeof(struct passwd)));

    free(buffer);
    return NULL;

#else /* ie. ifndef HAVE_GETPWNAM_R */

    /*
     * Sorry, there does not appear to be a reentrant version of
     * getpwnam(). So, we use the standard libc function.
     */

    return getpwnam(user);

#endif /* def HAVE_GETPWNAM_R */
}
