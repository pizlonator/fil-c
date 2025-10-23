/*
 * $Id$
 *
 * This function provides a thread safer version of getspnam() for use
 * with PAM modules that care about this sort of thing.
 *
 * XXX - or at least it should provide a thread-safe alternative.
 */

#include "pam_modutil_private.h"
#include "pam_inline.h"

#include <errno.h>
#include <limits.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>

struct spwd *
pam_modutil_getspnam(pam_handle_t *pamh, const char *user)
{
#ifdef HAVE_GETSPNAM_R

    void *buffer=NULL;
    size_t length = PWD_INITIAL_LENGTH;

    do {
	int status;
	void *new_buffer;
	struct spwd *result = NULL;

	new_buffer = realloc(buffer, sizeof(struct spwd) + length);
	if (new_buffer == NULL) {

	    D(("out of memory"));

	    /* no memory for the user - so delete the memory */
	    free(buffer);
	    return NULL;
	}
	buffer = new_buffer;

	/* make the re-entrant call to get the spwd structure */
        errno = 0;
	status = getspnam_r(user, buffer,
			    sizeof(struct spwd) + (char *) buffer,
			    length, &result);
	if (!status && (result == buffer)) {
	    const void *ignore;
	    int i;

	    if (pamh != NULL) {
	        for (i = 0; i < INT_MAX; i++) {
		    char *data_name = pam_asprintf("_pammodutil_getspnam_%s_%d",
						   user, i);
		    if (data_name == NULL) {
			D(("was unable to register the data item [%s]",
			   pam_strerror(pamh, status)));
			free(buffer);
			return NULL;
		    }
		    status = PAM_NO_MODULE_DATA;
	            if (pam_get_data(pamh, data_name, &ignore) != PAM_SUCCESS) {
		        status = pam_set_data(pamh, data_name,
					      result, pam_modutil_cleanup);
		    }
		    free(data_name);
		    if (status == PAM_SUCCESS) {
		        break;
		    }
		}
	    } else {
	        status = PAM_SUCCESS;
	    }

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

    D(("spwd structure took %zu bytes or so of memory",
       length+sizeof(struct spwd)));

    free(buffer);
    return NULL;

#else /* ie. ifndef HAVE_GETSPNAM_R */

    /*
     * Sorry, there does not appear to be a reentrant version of
     * getspnam(). So, we use the standard libc function.
     */

    return getspnam(user);

#endif /* def HAVE_GETSPNAM_R */
}
