
#include <glib-object.h>

#include "gitestmacros.h"

#define BAR_TYPE_BAZ              (bar_baz_get_type ())
#define BAR_BAZ(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), BAR_TYPE_BAZ, BarBaz))
#define BAR_IS_BAZ(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), BAR_TYPE_BAZ))

typedef struct BarBaz          BarBaz;
typedef struct BarBazClass     BarBazClass;

struct BarBaz
{
  GObject parent_instance;
};

struct BarBazClass
{
  GObjectClass parent_class;
};

_GI_TEST_EXTERN
GType bar_baz_get_type          (void) G_GNUC_CONST;


_GI_TEST_EXTERN
void bar_app_func (void);

_GI_TEST_EXTERN
void bar_app_func2 (int x, double y);


/* The intent of this case is to match mutter, which has unprefixed names like "MutterWindow".
 * In this case we don't want to add "Mutter" as a namespace prefix, since that gives us a
 * clash on a class name.  (MetaWindow and MutterWindow)
 */
#define MUTTER_TYPE_WINDOW              (mutter_window_get_type ())
#define MUTTER_WINDOW(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), MUTTER_TYPE_WINDOW, MutterWindow))
#define MUTTER_IS_WINDOW(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), MUTTER_TYPE_WINDOW))

typedef struct MutterWindow          MutterWindow;
typedef struct MutterWindowClass     MutterWindowClass;

struct MutterWindow
{
  GObject parent_instance;
};

struct MutterWindowClass
{
  GObjectClass parent_class;
};

_GI_TEST_EXTERN
GType mutter_window_get_type          (void) G_GNUC_CONST;

_GI_TEST_EXTERN
void mutter_window_func (MutterWindow *window, guint v);
