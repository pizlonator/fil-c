/*
 * $Id$
 *
 * This function provides a common pam_set_data() friendly version of free().
 */

#include "pam_modutil_private.h"

#include <stdlib.h>

void
pam_modutil_cleanup (pam_handle_t *pamh UNUSED, void *data,
		     int error_status UNUSED)
{
	/* junk it */
	free(data);
}
