#ifndef PAM_I18N_H
#define PAM_I18N_H

#ifdef ENABLE_NLS

# include <libintl.h>
# define _(msgid) dgettext(PACKAGE, msgid)
# define N_(msgid) msgid

#else

# define _(msgid) (msgid)
# define N_(msgid) msgid

#endif /* ENABLE_NLS */

#endif /* PAM_I18N_H */
