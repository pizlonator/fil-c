/* PAM setquota module

   This PAM module sets disk quota when a session begins.

   Copyright © 2006 Ruslan Savchenko <savrus@mexmat.net>
   Copyright © 2010 Shane Tzen <shane@ict.usc.edu>
   Copyright © 2012-2020 Sven Hartge <sven@svenhartge.de>
   Copyright © 2016 Keller Fuchs <kellerfuchs@hashbang.sh>
*/

#include "pam_inline.h"

#include <sys/types.h>
#include <sys/quota.h>
#include <linux/quota.h>
#include <pwd.h>
#include <syslog.h>
#include <errno.h>
#include <mntent.h>
#include <stdio.h>
#include <stdbool.h>

#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/pam_ext.h>
#include <security/pam_modutil.h>

#ifndef PATH_LOGIN_DEFS
# define PATH_LOGIN_DEFS "/etc/login.defs"
#endif

#define MAX_UID_VALUE 0xFFFFFFFFUL

struct pam_params {
  uid_t start_uid;
  uid_t end_uid;
  const char *fs;
  size_t fs_len;
  int overwrite;
  int debug;
};

static inline void
debug(pam_handle_t *pamh, const struct if_dqblk *p,
      const char *device, const char *dbgprefix) {
  pam_syslog(pamh, LOG_DEBUG, "%s device=%s bsoftlimit=%llu bhardlimit=%llu "
                              "isoftlimit=%llu ihardlimit=%llu btime=%llu itime=%llu",
	     dbgprefix, device,
	     (unsigned long long) p->dqb_bsoftlimit,
	     (unsigned long long) p->dqb_bhardlimit,
	     (unsigned long long) p->dqb_isoftlimit,
	     (unsigned long long) p->dqb_ihardlimit,
	     (unsigned long long) p->dqb_btime,
	     (unsigned long long) p->dqb_itime);
}

static unsigned long long
str_to_dqb_num(pam_handle_t *pamh, const char *str, const char *param) {
  char *ep = NULL;

  errno = 0;
  long long temp = strtoll(str, &ep, 10);
  if (temp < 0 || str == ep || *ep != '\0' || errno !=0) {
    pam_syslog(pamh, LOG_ERR, "Parameter \"%s=%s\" invalid, setting to 0", param, str);
    return 0;
  }
  else {
    return temp;
  }
}

static bool
parse_dqblk(pam_handle_t *pamh, int argc, const char **argv, struct if_dqblk *p) {
  bool bhard = false, bsoft = false, ihard = false, isoft = false;

  /* step through arguments */
  for (; argc-- > 0; ++argv) {
    const char *str;
    if ((str = pam_str_skip_prefix(*argv, "bhardlimit=")) != NULL) {
      p->dqb_bhardlimit = str_to_dqb_num(pamh, str, "bhardlimit");
      p->dqb_valid |= QIF_BLIMITS;
      bhard = true;
    } else if ((str = pam_str_skip_prefix(*argv, "bsoftlimit=")) != NULL) {
      p->dqb_bsoftlimit = str_to_dqb_num(pamh, str, "bsoftlimit");
      p->dqb_valid |= QIF_BLIMITS;
      bsoft = true;
    } else if ((str = pam_str_skip_prefix(*argv, "ihardlimit=")) != NULL) {
      p->dqb_ihardlimit = str_to_dqb_num(pamh, str, "ihardlimit");
      p->dqb_valid |= QIF_ILIMITS;
      ihard = true;
    } else if ((str = pam_str_skip_prefix(*argv, "isoftlimit=")) != NULL) {
      p->dqb_isoftlimit = str_to_dqb_num(pamh, str, "isoftlimit");
      p->dqb_valid |= QIF_ILIMITS;
      isoft = true;
    } else if ((str = pam_str_skip_prefix(*argv, "btime=")) != NULL) {
      p->dqb_btime = str_to_dqb_num(pamh, str, "btime");
      p->dqb_valid |= QIF_BTIME;
    } else if ((str = pam_str_skip_prefix(*argv, "itime=")) != NULL) {
      p->dqb_itime = str_to_dqb_num(pamh, str, "itime");
      p->dqb_valid |= QIF_ITIME;
    }
  }

  /* return FALSE if a softlimit or hardlimit has been set
   * independently of its counterpart.
   */
  return !(bhard ^ bsoft) && !(ihard ^ isoft);
}

/* inspired by pam_usertype_get_id */
static uid_t
str_to_uid(pam_handle_t *pamh, const char *value, uid_t default_value, const char *param) {
    unsigned long ul;
    char *ep;
    uid_t uid;

    errno = 0;
    ul = strtoul(value, &ep, 10);
    if (!(ul >= MAX_UID_VALUE
        || (uid_t)ul >= MAX_UID_VALUE
        || (errno != 0 && ul == 0)
        || value == ep
        || *ep != '\0')) {
        uid = (uid_t)ul;
    } else {
        pam_syslog(pamh, LOG_ERR, "Parameter \"%s=%s\" invalid, "
                   "setting to %u", param, value, default_value);
        uid = default_value;
    }

    return uid;
}

static void
parse_params(pam_handle_t *pamh, int argc, const char **argv, struct pam_params *p) {
  /* step through arguments */
  for (; argc-- > 0; ++argv) {
    const char *str;
    char *ep = NULL;
    if ((str = pam_str_skip_prefix(*argv, "startuid=")) != NULL) {
      p->start_uid = str_to_uid(pamh, str, p->start_uid, "startuid");
    } else if ((str = pam_str_skip_prefix(*argv, "enduid=")) != NULL) {
      p->end_uid = str_to_uid(pamh, str, p->end_uid, "enduid");
    } else if ((str = pam_str_skip_prefix(*argv, "fs=")) != NULL) {
      p->fs = str;
      p->fs_len = strlen(str);
      /* Mask the unnecessary '/' from the end of fs parameter */
      if (p->fs_len > 1 && p->fs[p->fs_len - 1] == '/')
        --p->fs_len;
    } else if ((str = pam_str_skip_prefix(*argv, "overwrite=")) != NULL) {
      errno = 0;
      p->overwrite = strtol(str, &ep, 10);
      if (*ep != '\0' || str == ep || errno !=0 || (p->overwrite < 0)) {
        pam_syslog(pamh, LOG_ERR, "Parameter \"overwrite=%s\" invalid, "
                        "setting to 0", str);
        p->overwrite = 0;
      }
    } else if ((str = pam_str_skip_prefix(*argv, "debug=")) != NULL) {
      errno = 0;
      p->debug = strtol(str, &ep, 10);
      if (*ep != '\0' || str == ep || errno != 0 || (p->debug < 0)) {
        pam_syslog(pamh, LOG_ERR, "Parameter \"debug=%s\" invalid, "
                        "setting to 0", str);
        p->debug = 0;
      }
    }
  }
}

int
pam_sm_open_session(pam_handle_t *pamh, int flags UNUSED,
		    int argc, const char **argv)
{
  int retval;
  char *val, *mntdevice = NULL;
  const void *user;
  const struct passwd *pwd;
  struct pam_params param = {
          .start_uid = PAM_USERTYPE_UIDMIN,
          .end_uid = 0,
          .fs = NULL };
  struct if_dqblk ndqblk;
  FILE *fp;
  size_t mnt_len = 0, match_size = 0;
#ifdef HAVE_GETMNTENT_R
  char buf[BUFSIZ];
  struct mntent ent;
#endif
  const struct mntent *mnt;
  const char *service;

  if (pam_get_item(pamh, PAM_SERVICE, (const void **)&service) != PAM_SUCCESS)
    service = "";

  /* Get UID_MIN for default start_uid from login.defs */
  val = pam_modutil_search_key(pamh, PATH_LOGIN_DEFS, "UID_MIN");

  /* Should UID_MIN be undefined, use current value of param.start_uid
   * pre-defined as PAM_USERTYPE_UIDMIN set by configure as safe
   * starting UID to avoid setting a quota for root and system
   * users if startuid= parameter is absent.
   */
  if (val) {
    param.start_uid = str_to_uid(pamh, val, param.start_uid, PATH_LOGIN_DEFS":UID_MIN");
    _pam_drop(val);
  }

  /* Parse parameter values
   * Must come after pam_modutil_search_key so that the current system
   * default for UID_MIN is already in p.start_uid to serve as default
   * for str_to_uid in case of a parse error.
   * */
  parse_params(pamh, argc, argv, &param);

  if (param.debug >= 1)
    pam_syslog(pamh, LOG_DEBUG, "Config: startuid=%u enduid=%u fs=%s "
                    "debug=%d overwrite=%d",
                    param.start_uid, param.end_uid,
                    param.fs ? param.fs : "(none)",
                    param.debug, param.overwrite);

  /* Determine the user name so we can get the home directory */
  retval = pam_get_item(pamh, PAM_USER, &user);
  if (retval != PAM_SUCCESS || user == NULL || *(const char *)user == '\0') {
    pam_syslog(pamh, LOG_NOTICE, "user unknown");
    return PAM_USER_UNKNOWN;
  }

  /* Get the password entry */
  pwd = pam_modutil_getpwnam(pamh, user);
  if (pwd == NULL) {
    pam_syslog(pamh, LOG_NOTICE, "user unknown");
    return PAM_USER_UNKNOWN;
  }

  /* Check if we should not set quotas for user */
  if ((pwd->pw_uid < param.start_uid) ||
      ((param.end_uid >= param.start_uid) && (param.start_uid != 0) &&
       (pwd->pw_uid > param.end_uid)))
    return PAM_SUCCESS;

  /* Find out what device the filesystem is hosted on */
  if ((fp = setmntent("/proc/mounts", "r")) == NULL) {
    pam_syslog(pamh, LOG_ERR, "Unable to open /proc/mounts");
    return PAM_PERM_DENIED;
  }

  while (
#ifdef HAVE_GETMNTENT_R
           (mnt = getmntent_r(fp, &ent, buf, sizeof(buf))) != NULL
#else
           (mnt = getmntent(fp)) != NULL
#endif
        ) {
    /* If param.fs is not specified use filesystem with users homedir
     * as default.
     */
    if (param.fs == NULL) {
      /* Mask trailing / from mnt->mnt_dir, to get a leading / on the
       * remaining suffix returned by pam_str_skip_prefix_len()
       */
      for (mnt_len = strlen(mnt->mnt_dir); mnt_len > 0; --mnt_len)
        if (mnt->mnt_dir[mnt_len - 1] != '/')
          break;
      const char *s;
      if (param.debug >= 2)
        pam_syslog(pamh, LOG_DEBUG, "Trying to match pw_dir=\"%s\" "
                        "with mnt_dir=\"%s\"", pwd->pw_dir, mnt->mnt_dir);
      /*
       * (mnt_len > match_size) Only try matching the mnt_dir if its length
       * is longer than the last matched length, trying to find the longest
       * mnt_dir for a given pwd_dir.
       *
       * (mnt_len == 0 && mnt->mnt_dir[0] == '/') special-cases the
       * root-dir /, which is the only mnt_dir with a trailing '/', which
       * got masked earlier.
       */
      if ((mnt_len > match_size || (mnt_len == 0 && mnt->mnt_dir[0] == '/')) &&
         (s = pam_str_skip_prefix_len(pwd->pw_dir, mnt->mnt_dir, mnt_len)) != NULL &&
         s[0] == '/') {
        free(mntdevice);
        if ((mntdevice = strdup(mnt->mnt_fsname)) == NULL) {
          pam_syslog(pamh, LOG_CRIT, "Memory allocation error");
          endmntent(fp);
          return PAM_PERM_DENIED;
        }
        match_size = mnt_len;
        if (param.debug >= 2)
          pam_syslog(pamh, LOG_DEBUG, "Found pw_dir=\"%s\" in mnt_dir=\"%s\" "
                     "with suffix=\"%s\" on device=\"%s\"", pwd->pw_dir,
                     mnt->mnt_dir, s, mntdevice);
      }
    /* param.fs has been specified, find exactly matching filesystem */
    } else if ((strncmp(param.fs, mnt->mnt_dir, param.fs_len) == 0
                && mnt->mnt_dir[param.fs_len] == '\0') ||
               (strncmp(param.fs, mnt->mnt_fsname, param.fs_len) == 0
                && mnt->mnt_fsname[param.fs_len] == '\0' )) {
        free(mntdevice);
        if ((mntdevice = strdup(mnt->mnt_fsname)) == NULL) {
          pam_syslog(pamh, LOG_CRIT, "Memory allocation error");
          endmntent(fp);
          return PAM_PERM_DENIED;
        }
        if (param.debug >= 2)
          pam_syslog(pamh, LOG_DEBUG, "Found fs=\"%s\" in mnt_dir=\"%s\" "
                     "on device=\"%s\"", param.fs, mnt->mnt_dir, mntdevice);
    }
  }

  endmntent(fp);

  if (mntdevice == NULL) {
    pam_syslog(pamh, LOG_ERR, "Filesystem or device not found: %s", param.fs ? param.fs : pwd->pw_dir);
    return PAM_PERM_DENIED;
  }

  /* Get limits */
  if (quotactl(QCMD(Q_GETQUOTA, USRQUOTA), mntdevice, pwd->pw_uid,
               (void *)&ndqblk) == -1) {
    pam_syslog(pamh, LOG_ERR, "fail to get limits for user %s : %m",
               pwd->pw_name);
    free(mntdevice);
    return PAM_PERM_DENIED;
  }

  if (param.debug >= 1)
    debug(pamh, &ndqblk, mntdevice, "Quota read:");

  /* Only overwrite if quotas aren't already set or if overwrite is set */
  if ((ndqblk.dqb_bsoftlimit == 0 && ndqblk.dqb_bhardlimit == 0 &&
       ndqblk.dqb_isoftlimit == 0 && ndqblk.dqb_ihardlimit == 0) ||
      param.overwrite == 1) {

    /* Parse new limits
     * Exit with an error should only the hard- or softlimit be
     * configured but not both.
     * This avoids errors, inconsistencies and possible race conditions
     * during setquota.
     */
    ndqblk.dqb_valid = 0;
    if (!parse_dqblk(pamh, argc, argv, &ndqblk)) {
      pam_syslog(pamh, LOG_ERR,
                 "Both soft- and hardlimits for %s need to be configured "
                 "at the same time!", mntdevice);
      free(mntdevice);
      return PAM_PERM_DENIED;
    }

    /* Nothing changed? Are no limits defined at all in configuration? */
    if (ndqblk.dqb_valid == 0) {
      pam_syslog(pamh, LOG_AUTH | LOG_WARNING, "no limits defined in "
                 "configuration for user %s on %s", pwd->pw_name, mntdevice);
      free(mntdevice);
      return PAM_IGNORE;
    }

    /* Set limits */
    if (quotactl(QCMD(Q_SETQUOTA, USRQUOTA), mntdevice, pwd->pw_uid,
                 (void *)&ndqblk) == -1) {
      pam_syslog(pamh, LOG_ERR, "failed to set limits for user %s on %s: %m",
                 pwd->pw_name, mntdevice);
      free(mntdevice);
      return PAM_PERM_DENIED;
    }
    if (param.debug >= 1)
      debug(pamh, &ndqblk, mntdevice, "Quota set:");

    /* End module */
    free(mntdevice);
    return PAM_SUCCESS;

  } else {
    /* Quota exists and overwrite!=1 */
    if (param.debug >= 1) {
      pam_syslog(pamh, LOG_DEBUG, "Quota already exists for user %s "
                 "on %s, not overwriting it without \"overwrite=1\"",
                 pwd->pw_name, mntdevice);
    }
    /* End module */
    free(mntdevice);
    return PAM_IGNORE;
  }

}

int
pam_sm_close_session(pam_handle_t *pamh UNUSED, int flags UNUSED,
		     int argc UNUSED, const char **argv UNUSED)
{
  return PAM_SUCCESS;
}
