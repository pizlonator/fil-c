/* lsm.h - header file for lib directory
 *
 * Copyright 2015 Rob Landley <rob@landley.net>
 */

#if CFG_TOYBOX_SELINUX
#include <selinux/selinux.h>
#else
#define is_selinux_enabled() 0
#define setfscreatecon(...) (-1)
#define getcon(...) (-1)
#define getfilecon(...) (-1)
#define lgetfilecon(...) (-1)
#define fgetfilecon(...) (-1)
#define setfilecon(...) (-1)
#define lsetfilecon(...) (-1)
#define fsetfilecon(...) (-1)
#endif

#if CFG_TOYBOX_SMACK
#include <sys/smack.h>
#include <linux/xattr.h>
#else
#ifndef XATTR_NAME_SMACK
#define XATTR_NAME_SMACK 0
#endif
#define smack_smackfs_path(...) (-1)
#define smack_new_label_from_self(...) (-1)
#define smack_new_label_from_path(...) (-1)
#define smack_new_label_from_file(...) (-1)
#define smack_set_label_for_self(...) (-1)
#define smack_set_label_for_path(...) (-1)
#define smack_set_label_for_file(...) (-1)
#endif

// This turns into "return 0" when no LSM and lets code optimize out.
static inline int lsm_enabled(void)
{
  if (CFG_TOYBOX_SMACK) return !!smack_smackfs_path();
  else return is_selinux_enabled() == 1;
}

static inline char *lsm_name(void)
{
  if (CFG_TOYBOX_SMACK) return "Smack";
  if (CFG_TOYBOX_SELINUX) return "SELinux";

  return "LSM";
}

// Fetch this process's lsm context
static inline char *lsm_context(void)
{
  int ok = 0;
  char *result = 0;

  if (CFG_TOYBOX_SMACK) ok = smack_new_label_from_self(&result) > 0;
  else ok = getcon(&result) == 0;

  return ok ? result : strdup("?");
}

// Set default label to apply to newly created stuff (NULL to clear it)
static inline int lsm_set_create(char *context)
{
  if (CFG_TOYBOX_SMACK) return smack_set_label_for_self(context);
  else return setfscreatecon(context);
}

// Label a file, following symlinks
static inline int lsm_set_context(char *filename, char *context)
{
  if (CFG_TOYBOX_SMACK)
    return smack_set_label_for_path(filename, XATTR_NAME_SMACK, 1, context);
  else return setfilecon(filename, context);
}

// Label a file, don't follow symlinks
static inline int lsm_lset_context(char *filename, char *context)
{
  if (CFG_TOYBOX_SMACK)
    return smack_set_label_for_path(filename, XATTR_NAME_SMACK, 0, context);
  else return lsetfilecon(filename, context);
}

// Label a file by filehandle
static inline int lsm_fset_context(int file, char *context)
{
  if (CFG_TOYBOX_SMACK)
    return smack_set_label_for_file(file, XATTR_NAME_SMACK, context);
  else return fsetfilecon(file, context);
}

// returns -1 in case of error or else the length of the context */
// context can be NULL to get the length only */
static inline int lsm_get_context(char *filename, char **context)
{
  if (CFG_TOYBOX_SMACK)
    return smack_new_label_from_path(filename, XATTR_NAME_SMACK, 1, context);
  else return getfilecon(filename, context);
}

static inline int lsm_lget_context(char *filename, char **context)
{
  if (CFG_TOYBOX_SMACK)
    return smack_new_label_from_path(filename, XATTR_NAME_SMACK, 0, context);
  else return lgetfilecon(filename, context);
}

static inline int lsm_fget_context(int file, char **context)
{
  if (CFG_TOYBOX_SMACK)
    return smack_new_label_from_file(file, XATTR_NAME_SMACK, context);
  return fgetfilecon(file, context);
}
