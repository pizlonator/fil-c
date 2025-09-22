#ifdef ENABLE_NLS
# include <libintl.h>
# define _(x)                   gettext(x)
#else
# define _(x)                   (x)
# define textdomain(d)          do { } while (0)
# define bindtextdomain(d,dir)  do { } while (0)
#endif
#include <locale.h>
