/*
 * Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/*
 * Stub pam_ecryptfs.so - A minimal PAM module for ecryptfs compatibility
 *
 * This is a stub implementation of pam_ecryptfs that does nothing except
 * allow PAM configurations that reference pam_ecryptfs.so to load successfully.
 *
 * Why this exists:
 * - Ubuntu's /etc/pam.d/common-auth includes pam_ecryptfs as "required"
 * - Real pam_ecryptfs requires Mozilla NSS (large dependency tree)
 * - ecryptfs is legacy (replaced by LUKS/fscrypt in modern systems)
 * - Modern users don't have ecryptfs-encrypted home directories
 *
 * What this does:
 * - Returns PAM_IGNORE for all operations (means "I have no opinion")
 * - Allows PAM stack to continue processing other modules
 * - Users without ecryptfs: works perfectly (does nothing)
 * - Users with ecryptfs: authentication will fail (could enhance to detect this)
 *
 * This allows Fil-C PAM to work with Ubuntu's system PAM configs without
 * porting the entire Mozilla NSS + ecryptfs stack.
 */

#include <security/pam_modules.h>
#include <security/pam_ext.h>

/*
 * Authentication phase - verify user credentials
 * Returns PAM_IGNORE to defer to other modules (like pam_unix)
 */
PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags,
                                   int argc, const char **argv)
{
    return PAM_IGNORE;
}

/*
 * Credential setting phase - establish user credentials
 * Returns PAM_IGNORE as we have no credentials to set
 */
PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags,
                              int argc, const char **argv)
{
    return PAM_IGNORE;
}

/*
 * Account management phase - verify account is valid
 * Returns PAM_IGNORE to defer to other modules
 */
PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
                                int argc, const char **argv)
{
    return PAM_IGNORE;
}

/*
 * Session opening phase - would mount ecryptfs in real implementation
 * Returns PAM_IGNORE as we don't mount anything
 */
PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags,
                                   int argc, const char **argv)
{
    return PAM_IGNORE;
}

/*
 * Session closing phase - would unmount ecryptfs in real implementation
 * Returns PAM_IGNORE as we have nothing to clean up
 */
PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags,
                                    int argc, const char **argv)
{
    return PAM_IGNORE;
}

/*
 * Password change phase - would re-wrap ecryptfs keys in real implementation
 * Returns PAM_IGNORE as we have no keys to manage
 */
PAM_EXTERN int pam_sm_chauthtok(pam_handle_t *pamh, int flags,
                                int argc, const char **argv)
{
    return PAM_IGNORE;
}
