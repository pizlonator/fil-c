/*
 * pam_prelude.h -- prelude ids reporting
 * http://www.prelude-ids.org
 *
 * (C) Sebastien Tricaud 2005 <toady@gscore.org>
 */

#ifndef _SECURITY_PAM_PRELUDE_H
#define _SECURITY_PAM_PRELUDE_H

#include <security/_pam_types.h>

void prelude_send_alert(pam_handle_t *pamh, int authval);

#endif /* _SECURITY_PAM_PRELUDE_H */
