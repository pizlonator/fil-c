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
#include "pam_inline.h"
#include "pam_i18n.h"

#include <security/pam_ext.h>

#define PROMPT _("Password: ")
/* For Translators: "%s" is replaced with "<service>". */
#define PROMPT_CURRENT_ARG _("Current %s password: ")
#define PROMPT_CURRENT_NOARG _("Current password: ")
/* For Translators: "%s" is replaced with "<service>". */
#define PROMPT_NEW_ARG _("New %s password: ")
#define PROMPT_NEW_NOARG _("New password: ")
/* For Translators: "%s" is replaced with "<service>". */
#define PROMPT_RETYPE_ARG _("Retype new %s password: ")
#define PROMPT_RETYPE_NOARG _("Retype new password: ")
#define MISTYPED_PASS _("Sorry, passwords do not match.")

#define PAM_GETAUTHTOK_NOVERIFY  1

static const char *
get_option (pam_handle_t *pamh, const char *option)
{
  int i;
  size_t len;


  if (option == NULL || pamh == NULL ||
      pamh->mod_argc == 0 || pamh->mod_argv == NULL)
    return NULL;

  len = strlen (option);

  for (i = 0; i < pamh->mod_argc; i++)
    {
      if (strncmp (option, pamh->mod_argv[i], len) == 0)
        {
          if (pamh->mod_argv[i][len] == '=')
            return &(pamh->mod_argv[i][len+1]);
          else if (pamh->mod_argv[i][len] == '\0')
            return "";
        }
    }
  return NULL;
}


static int
pam_get_authtok_internal (pam_handle_t *pamh, int item,
			  const char **authtok, const char *prompt,
			  unsigned int flags)

{
  IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

  char *resp[2] = {NULL, NULL};
  const void *prevauthtok;
  const char *authtok_type = "";
  int chpass = 0; /* Password change, ask twice for it */
  int retval;

  if (authtok == NULL)
    return PAM_SYSTEM_ERR;

  /* PAM_AUTHTOK in password stack returns new password,
     which needs to be verified. */
  if (pamh->choice == PAM_CHAUTHTOK)
    {
      if (item == PAM_AUTHTOK)
	{
	  chpass = 1;
	  if (!(flags & PAM_GETAUTHTOK_NOVERIFY))
	    ++chpass;
	}
      authtok_type = get_option (pamh, "authtok_type");
      if (authtok_type == NULL)
	{
	  retval = pam_get_item (pamh, PAM_AUTHTOK_TYPE, (const void **)&authtok_type);
	  if (retval != PAM_SUCCESS || authtok_type == NULL)
	    authtok_type = "";
	}
      else
        pam_set_item(pamh, PAM_AUTHTOK_TYPE, authtok_type);
    }

  retval = pam_get_item (pamh, item, &prevauthtok);
  if (retval == PAM_SUCCESS && prevauthtok != NULL)
    {
      *authtok = prevauthtok;
      return PAM_SUCCESS;
    }
  else if (get_option (pamh, "use_first_pass") ||
	   (chpass && get_option (pamh, "use_authtok")))
    {
      if (prevauthtok == NULL)
	{
	  if (chpass)
	    return PAM_AUTHTOK_ERR;
	  else
	    return PAM_AUTH_ERR;
	}
      else
	return retval;
    }

  if (prompt != NULL)
    {
      retval = pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[0],
			   "%s", prompt);
      if (retval == PAM_SUCCESS && chpass > 1 && resp[0] != NULL)
	retval = pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[1],
			     _("Retype %s"), prompt);
    }
  else if (chpass)
    {
      pamh->authtok_verified = 0;

      retval = *authtok_type ?
	pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[0],
		    PROMPT_NEW_ARG, authtok_type) :
	pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[0],
		    "%s", PROMPT_NEW_NOARG);
      if (retval == PAM_SUCCESS && chpass > 1 && resp[0] != NULL)
	{
	  retval = *authtok_type ?
	    pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[1],
			PROMPT_RETYPE_ARG, authtok_type) :
	    pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[1],
			"%s", PROMPT_RETYPE_NOARG);
	}
    }
  else if (item == PAM_OLDAUTHTOK)
    {
      retval = *authtok_type ?
	pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[0],
		    PROMPT_CURRENT_ARG, authtok_type) :
	pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[0],
		    "%s", PROMPT_CURRENT_NOARG);
    }
  else
    retval = pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[0], "%s", PROMPT);

  if (retval != PAM_SUCCESS || resp[0] == NULL ||
      (chpass > 1 && resp[1] == NULL))
    {
      /* We want to abort */
      pam_overwrite_string (resp[0]);
      _pam_drop (resp[0]);
      pam_overwrite_string (resp[1]);
      _pam_drop (resp[1]);
      if (chpass)
        pam_error (pamh, _("Password change has been aborted."));
      return PAM_AUTHTOK_ERR;
    }

  if (chpass > 1 && strcmp (resp[0], resp[1]) != 0)
    {
      pam_error (pamh, MISTYPED_PASS);
      pam_overwrite_string (resp[0]);
      _pam_drop (resp[0]);
      pam_overwrite_string (resp[1]);
      _pam_drop (resp[1]);
      return PAM_TRY_AGAIN;
    }

  pam_overwrite_string (resp[1]);
  _pam_drop (resp[1]);

  retval = pam_set_item (pamh, item, resp[0]);
  pam_overwrite_string (resp[0]);
  _pam_drop (resp[0]);
  if (retval != PAM_SUCCESS)
    return retval;

  if (chpass > 1)
    pamh->authtok_verified = 1;

  return pam_get_item(pamh, item, (const void **)authtok);
}

int
pam_get_authtok (pam_handle_t *pamh, int item, const char **authtok,
		 const char *prompt)
{
  return pam_get_authtok_internal (pamh, item, authtok, prompt, 0);
}


int
pam_get_authtok_noverify (pam_handle_t *pamh, const char **authtok,
			  const char *prompt)
{
  return pam_get_authtok_internal (pamh, PAM_AUTHTOK, authtok, prompt,
				   PAM_GETAUTHTOK_NOVERIFY);
}

int
pam_get_authtok_verify (pam_handle_t *pamh, const char **authtok,
			const char *prompt)
{
  IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

  char *resp = NULL;
  const char *authtok_type = "";
  int retval;

  if (authtok == NULL || pamh->choice != PAM_CHAUTHTOK)
    return PAM_SYSTEM_ERR;

  if (pamh->authtok_verified)
    return pam_get_item (pamh, PAM_AUTHTOK, (const void **)authtok);

  if (prompt != NULL)
    {
      retval = pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp,
			   _("Retype %s"), prompt);
    }
  else
    {
      retval = pam_get_item (pamh, PAM_AUTHTOK_TYPE, (const void **)&authtok_type);
      if (retval != PAM_SUCCESS || authtok_type == NULL)
        authtok_type = "";
      retval = *authtok_type ?
	pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp,
		    PROMPT_RETYPE_ARG, authtok_type) :
	pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp,
		    "%s", PROMPT_RETYPE_NOARG);
    }

  if (retval != PAM_SUCCESS || resp == NULL)
    {
      /* We want to abort the password change */
      pam_set_item (pamh, PAM_AUTHTOK, NULL);
      pam_error (pamh, _("Password change has been aborted."));
      return PAM_AUTHTOK_ERR;
    }

  if (strcmp (*authtok, resp) != 0)
    {
      pam_set_item (pamh, PAM_AUTHTOK, NULL);
      pam_error (pamh, MISTYPED_PASS);
      pam_overwrite_string (resp);
      _pam_drop (resp);
      return PAM_TRY_AGAIN;
    }

  retval = pam_set_item (pamh, PAM_AUTHTOK, resp);
  pam_overwrite_string (resp);
  _pam_drop (resp);
  if (retval != PAM_SUCCESS)
    return retval;

  pamh->authtok_verified = 1;

  return pam_get_item(pamh, PAM_AUTHTOK, (const void **)authtok);
}
