#ifndef PAMMODUTIL_PRIVATE_H
#define PAMMODUTIL_PRIVATE_H

/*
 * $Id$
 *
 * Copyright (c) 2001 Andrew Morgan <morgan@kernel.org>
 */

#include "config.h"

#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_modutil.h>

#define PWD_INITIAL_LENGTH     0x400
#define PWD_ABSURD_PWD_LENGTH  0x40001
#define PWD_LENGTH_SHIFT 4 /* 2^4 == 16 */

extern void
pam_modutil_cleanup(pam_handle_t *pamh, void *data,
                    int error_status);

#endif /* PAMMODUTIL_PRIVATE_H */
