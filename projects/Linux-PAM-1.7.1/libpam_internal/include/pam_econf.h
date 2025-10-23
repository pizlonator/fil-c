/* pam_econf.h -- routines to parse configuration files with libeconf */

#ifndef PAM_ECONF_H
#define PAM_ECONF_H

#ifdef USE_ECONF

#include <libeconf.h>

econf_err pam_econf_readconfig(econf_file **key_file,
			       const char *usr_conf_dir,
			       const char *etc_conf_dir,
			       const char *config_name,
			       const char *config_suffix,
			       const char *delim,
			       const char *comment,
			       bool (*callback)(const char *filename, const void *data),
			       const void *callback_data);

#endif /* USE_ECONF */

#endif /* PAM_ECONF_H */
