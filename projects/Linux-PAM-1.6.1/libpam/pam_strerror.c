/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pam_private.h"

const char *pam_strerror(pam_handle_t *pamh UNUSED, int errnum)
{
    switch (errnum) {
    case PAM_SUCCESS:
      return _("Success");
    case PAM_ABORT:
      return _("Critical error - immediate abort");
    case PAM_OPEN_ERR:
      return _("Failed to load module");
    case PAM_SYMBOL_ERR:
      return _("Symbol not found");
    case PAM_SERVICE_ERR:
      return _("Error in service module");
    case PAM_SYSTEM_ERR:
      return _("System error");
    case PAM_BUF_ERR:
      return _("Memory buffer error");
    case PAM_PERM_DENIED:
      return _("Permission denied");
    case PAM_AUTH_ERR:
      return _("Authentication failure");
    case PAM_CRED_INSUFFICIENT:
      return _("Insufficient credentials to access authentication data");
    case PAM_AUTHINFO_UNAVAIL:
      return _("Authentication service cannot retrieve authentication info");
    case PAM_USER_UNKNOWN:
      return _("User not known to the underlying authentication module");
    case PAM_MAXTRIES:
      return _("Have exhausted maximum number of retries for service");
    case PAM_NEW_AUTHTOK_REQD:
      return _("Authentication token is no longer valid; new one required");
    case PAM_ACCT_EXPIRED:
      return _("User account has expired");
    case PAM_SESSION_ERR:
      return _("Cannot make/remove an entry for the specified session");
    case PAM_CRED_UNAVAIL:
      return _("Authentication service cannot retrieve user credentials");
    case PAM_CRED_EXPIRED:
      return _("User credentials expired");
    case PAM_CRED_ERR:
      return _("Failure setting user credentials");
    case PAM_NO_MODULE_DATA:
      return _("No module specific data is present");
    case PAM_BAD_ITEM:
      return _("Bad item passed to pam_*_item()");
    case PAM_CONV_ERR:
      return _("Conversation error");
    case PAM_AUTHTOK_ERR:
      return _("Authentication token manipulation error");
    case PAM_AUTHTOK_RECOVERY_ERR:
      return _("Authentication information cannot be recovered");
    case PAM_AUTHTOK_LOCK_BUSY:
      return _("Authentication token lock busy");
    case PAM_AUTHTOK_DISABLE_AGING:
      return _("Authentication token aging disabled");
    case PAM_TRY_AGAIN:
      return _("Failed preliminary check by password service");
    case PAM_IGNORE:
      return _("The return value should be ignored by PAM dispatch");
    case PAM_MODULE_UNKNOWN:
      return _("Module is unknown");
    case PAM_AUTHTOK_EXPIRED:
      return _("Authentication token expired");
    case PAM_CONV_AGAIN:
      return _("Conversation is waiting for event");
    case PAM_INCOMPLETE:
      return _("Application needs to call libpam again");
    }

    return _("Unknown PAM error");
}
