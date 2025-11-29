/* GtkIconTheme - a loader for icon themes
 * gtk-icon-theme.c Copyright (C) 2002, 2003 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <glib.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode)&_S_IFDIR)
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include "win32/gdkwin32.h"
#include "win32/gdkprivate-win32.h"
#endif /* G_OS_WIN32 */

#include "gtkiconthemeprivate.h"
#include "gtkcsspalettevalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkdebug.h"
#include "gtkiconcacheprivate.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtksettingsprivate.h"
#include "gtksnapshot.h"
#include "gtkstyleproviderprivate.h"
#include "gtksymbolicpaintable.h"
#include "gtkwidgetprivate.h"
#include "gdktextureutilsprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdkprofilerprivate.h"

#define GDK_ARRAY_ELEMENT_TYPE char *
#define GDK_ARRAY_NULL_TERMINATED 1
#define GDK_ARRAY_FREE_FUNC g_free
#define GDK_ARRAY_TYPE_NAME GtkStrvBuilder
#define GDK_ARRAY_NAME gtk_strv_builder
#define GDK_ARRAY_PREALLOC 16
#include "gdk/gdkarrayimpl.c"

/**
 * GtkIconTheme:
 *
 * `GtkIconTheme` provides a facility for loading themed icons.
 *
 * The main reason for using a name rather than simply providing a filename
 * is to allow different icons to be used depending on what “icon theme” is
 * selected by the user. The operation of icon themes on Linux and Unix
 * follows the [Icon Theme Specification](http://www.freedesktop.org/Standards/icon-theme-spec)
 * There is a fallback icon theme, named `hicolor`, where applications
 * should install their icons, but additional icon themes can be installed
 * as operating system vendors and users choose.
 *
 * In many cases, named themes are used indirectly, via [class@Gtk.Image]
 * rather than directly, but looking up icons directly is also simple. The
 * `GtkIconTheme` object acts as a database of all the icons in the current
 * theme. You can create new `GtkIconTheme` objects, but it’s much more
 * efficient to use the standard icon theme of the `GtkWidget` so that the
 * icon information is shared with other people looking up icons.
 *
 * ```c
 * GtkIconTheme *icon_theme;
 * GtkIconPaintable *icon;
 * GdkPaintable *paintable;
 *
 * icon_theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (my_widget));
 * icon = gtk_icon_theme_lookup_icon (icon_theme,
 *                                    "my-icon-name", // icon name
 *                                    48, // icon size
 *                                    1,  // scale
 *                                    0,  // flags);
 * paintable = GDK_PAINTABLE (icon);
 * // Use the paintable
 * g_object_unref (icon);
 * ```
 */


/* There are a lot of icon names (around 1000 in adwaita and 1000 in
 * hicolor), and the are short, which makes individual allocations
 * wasteful (i.e. glibc malloc min chunk size is 32 byte), so we and
 * fragmenting. Instead we store the string in larger chunks of memory
 * for the string data, avoiding duplicates via a hash.
 *
 * Additionally, doing a up-front hash lookup to intern an
 * icon name allows further hash lookups (for each directory in
 * the theme) to use g_direct_hash/equal() which makes those
 * lookups faster.
 */

typedef struct _GtkStringSetChunk GtkStringSetChunk;

/* This size allows a malloc to be one page, including the next_chunk
   pointer and the malloc overhead, which is a good size (one page) */
#define STRING_SET_CHUNK_SIZE (4096 - 2 *sizeof (gpointer))

struct _GtkStringSetChunk {
  GtkStringSetChunk *next;
  char data[STRING_SET_CHUNK_SIZE];
};

struct _GtkStringSet {
  GHashTable *hash;
  GtkStringSetChunk *chunks;
  int used_in_chunk;
};

static void
dump_string_set (GtkStringSet *set)
{
  GtkStringSetChunk *chunk = set->chunks;
  unsigned int n_chunks = 0;
  GHashTableIter iter;
  gpointer key;

  while (chunk)
    {
      n_chunks++;
      chunk = chunk->next;
    }
  g_print ("%u strings, %u chunks\n", g_hash_table_size (set->hash), n_chunks);

  g_hash_table_iter_init (&iter, set->hash);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      char *string = key;
      g_print ("%s\n", string);
    }
}

static void
gtk_string_set_init (GtkStringSet *set)
{
  set->hash = g_hash_table_new (g_str_hash, g_str_equal);
  set->chunks = NULL;
  set->used_in_chunk = STRING_SET_CHUNK_SIZE; /* To trigger a grow directly */
}

static void
gtk_string_set_destroy (GtkStringSet *set)
{
  GtkStringSetChunk *next, *chunk;
  g_hash_table_destroy (set->hash);
  for (chunk = set->chunks; chunk != NULL; chunk = next)
    {
      next = chunk->next;
      g_free (chunk);
    }
}

static const char *
gtk_string_set_lookup (GtkStringSet *set,
                       const char   *string)
{
  return g_hash_table_lookup (set->hash, string);
}

static void
gtk_string_set_list (GtkStringSet *set,
                     GHashTable   *result)
{
  GHashTableIter iter;
  gpointer key;

  g_hash_table_iter_init (&iter, set->hash);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      char *string = key;
      g_hash_table_insert (result, string, string);
    }
}

const char *
gtk_string_set_add (GtkStringSet *set,
                    const char   *string)
{
  const char *intern = g_hash_table_lookup (set->hash, string);

  if (intern == NULL)
    {
      GtkStringSetChunk *chunk;
      int string_len = strlen (string) + 1;

      if (string_len > STRING_SET_CHUNK_SIZE - set->used_in_chunk)
        {
          gsize chunk_size = sizeof (GtkStringSetChunk);
          /* Doesn't fit in existing chunk, need a new one.  Normally
           * we allocate one of regular size, but in the case the
           * string is longer than CHUNK_SIZE that we allocate enough
           * for it to fit this one string. */
          if (string_len > STRING_SET_CHUNK_SIZE)
            chunk_size += string_len - STRING_SET_CHUNK_SIZE;
          chunk = g_malloc (chunk_size);
          chunk->next = set->chunks;
          set->chunks = chunk;
          set->used_in_chunk = 0;
        }
      else
        {
          /* We fit in existing chunk */
          chunk = set->chunks;
        }

      intern = &chunk->data[set->used_in_chunk];
      memcpy ((char *)intern, string, string_len);
      set->used_in_chunk += string_len;
      g_hash_table_insert (set->hash, (char *)intern, (char *)intern);
    }

  return intern;
}

/* Threading:
 *
 * GtkIconTheme is partially threadsafe, construction and setup can
 * only be done on the main thread (and this is not really fixable
 * since it uses other things like GdkDisplay and GSettings and
 * signals on those. However, once the icon theme is set up on the
 * main thread we can pass it to a thread and do basic lookups on
 * it. This will cause any parallel calls on the main thread (or any
 * other thread) to block until its done, but most of the time
 * lookups are fast. The only time its not fast is when we need
 * to rescan the theme, but then it would be slow if we didn't block
 * and did the rescan ourselves anyway.
 *
 * All private functions that take a GtkIconTheme (or one of its
 * private data types (like IconThemeDir, UnthemedIcon, etc) arg are
 * expected to be called with the icon theme lock held, unless the
 * function has a _unlocked suffix. Any similar function that must be
 * called on the main thread, will have a _mainthread suffix.
 *
 * So the rules for such functions are:
 *
 * *  non-_unlocked function cannot call _unlocked functions.
 * * _unlocked must lock before calling a non-_unlocked.
 * * non-_mainthread cannot call _mainthread.
 * * Public APIs must lock before calling a non-_unlocked private function
 * * Public APIs that never call _mainthread are threadsafe.
 *
 * There is a global "icon_cache" G_LOCK, which protects icon_cache
 * and lru_cache in GtkIconTheme as well as its reverse pointer
 * GtkIcon->in_cache. This is sometimes taken with the theme lock held
 * (from the theme side) and sometimes not (from the icon side),
 * but we never take another lock after taking it, so this is safe.
 * Since this is a global (not per icon/theme) lock we should never
 * block while holding it.
 *
 * Sometimes there are "weak" references to the icon theme that can
 * call into the icon theme. For example, from the "theme-changed"
 * signal. Since these don't own the theme they can run in parallel
 * with some other thread which could be finalizing the theme. To
 * avoid this all such references are done via the GtkIconThemeRef
 * object which contains an NULL:able pointer to the theme and the
 * main lock for that theme. Using this we can safely generate a ref
 * for the theme if it still lives (or get NULL if it doesn't).
 *
 * The icon theme needs to call into the icons sometimes, for example
 * when checking if it should be cached (icon_should_cache__unlocked)
 * this takes the icon->texture_lock while the icon theme is locked.
 * In order to avoid ABBA style deadlocks this means the icon code
 * should never try to lock an icon theme.
 */

#define FALLBACK_ICON_THEME "hicolor"

typedef enum
{
  ICON_THEME_DIR_FIXED,
  ICON_THEME_DIR_SCALABLE,
  ICON_THEME_DIR_THRESHOLD,
  ICON_THEME_DIR_UNTHEMED
} IconThemeDirType;

#if 0
#define DEBUG_CACHE(args) g_print args
#else
#define DEBUG_CACHE(args)
#endif

#define LRU_CACHE_SIZE 100
#define MAX_LRU_TEXTURE_SIZE 128

typedef struct _GtkIconPaintableClass GtkIconPaintableClass;
typedef struct _GtkIconThemeClass     GtkIconThemeClass;


typedef struct _GtkIconThemeRef GtkIconThemeRef;

/* Acts as a database of information about an icon theme.
 * Normally, you retrieve the icon theme for a particular
 * display using gtk_icon_theme_get_for_display() and it
 * will contain information about current icon theme for
 * that display, but you can also create a new `GtkIconTheme`
 * object and set the icon theme name explicitly using
 * gtk_icon_theme_set_theme_name().
 */
struct _GtkIconTheme
{
  GObject parent_instance;
  GtkIconThemeRef *ref;

  GHashTable *icon_cache;                       /* Protected by icon_cache lock */

  GtkIconPaintable *lru_cache[LRU_CACHE_SIZE];  /* Protected by icon_cache lock */
  int lru_cache_current;                        /* Protected by icon_cache lock */

  GtkStringSet icons;

  char *current_theme;
  char **search_path;
  char **resource_path;

  guint custom_theme         : 1;
  guint is_display_singleton : 1;
  guint pixbuf_supports_svg  : 1;
  guint themes_valid         : 1;

  /* A list of all the themes needed to look up icons.
   * In search order, without duplicates
   */
  GList *themes;
  GHashTable *unthemed_icons;

  /* GdkDisplay for the icon theme (may be NULL) */
  GdkDisplay *display;
  GtkSettings *display_settings;

  /* time when we last stat:ed for theme changes */
  gint64 last_stat_time;
  GArray *dir_mtimes;

  gulong theme_changed_idle;

  int serial;
};

struct _GtkIconThemeClass
{
  GObjectClass parent_class;

  void (* changed)  (GtkIconTheme *self);
};

typedef struct {
  char **icon_names;
  int size;
  int scale;
  GtkIconLookupFlags flags;
} IconKey;

struct _GtkIconPaintableClass
{
  GObjectClass parent_class;
};

/* This lock protects both IconTheme.icon_cache and the dependent Icon.in_cache.
 * Its a global lock, so hold it only for short times. */
G_LOCK_DEFINE_STATIC(icon_cache);

/**
 * GtkIconPaintable:
 *
 * Contains information found when looking up an icon in `GtkIconTheme`.
 *
 * `GtkIconPaintable` implements `GdkPaintable`.
 */
struct _GtkIconPaintable
{
  GObject parent_instance;

  /* Information about the source
   */
  IconKey key;
  GtkIconTheme *in_cache; /* Protected by icon_cache lock */

  char *icon_name;
  char *filename;
  GLoadableIcon *loadable;

#ifdef G_OS_WIN32
  /* win32 icon (if there is any) */
  GdkPixbuf *win32_icon;
#endif

  /* Parameters influencing the scaled icon
   */
  int desired_size;
  int desired_scale;
  guint is_svg          : 1;
  guint is_resource     : 1;
  guint is_symbolic     : 1;
  guint only_fg         : 1;

  /* Cached information if we go ahead and try to load the icon.
   *
   * All access to these are protected by the texture_lock. Everything
   * above is immutable after construction and can be used without
   * locks.
   */
  GMutex texture_lock;

  GdkTexture *texture;
};

typedef struct
{
  char *name;
  char *display_name;
  char *comment;

  GArray *dir_sizes;     /* IconThemeDirSize */
  GArray *dirs;          /* IconThemeDir */
} IconTheme;

typedef struct
{
  guint16 dir_index;    /* index in dirs */
  guint8 best_suffix;
  guint8 best_suffix_no_svg;
} IconThemeFile;

typedef struct
{
  IconThemeDirType type;
  int size;
  int min_size;
  int max_size;
  int threshold;
  int scale;

  GArray *icon_files;
  GHashTable *icon_hash; /* name (interned) -> file index */
} IconThemeDirSize;

typedef struct
{
  gboolean is_resource;
  char *path;  /* e.g. "/usr/share/icons/hicolor/32x32/apps" */
} IconThemeDir;

typedef struct
{
  char *svg_filename;
  char *no_svg_filename;
  gboolean is_resource;
} UnthemedIcon;

typedef struct
{
  char *dir;
  time_t mtime;
  GtkIconCache *cache;
  gboolean exists;
} IconThemeDirMtime;

static void              gtk_icon_theme_finalize          (GObject          *object);
static void              gtk_icon_theme_dispose           (GObject          *object);
static IconTheme *       theme_new                        (const char       *theme_name,
                                                           GKeyFile         *theme_file);
static void              theme_dir_size_destroy           (IconThemeDirSize *dir_size);
static void              theme_dir_destroy                (IconThemeDir     *dir);
static void              theme_destroy                    (IconTheme        *theme);
static GtkIconPaintable *theme_lookup_icon                (IconTheme        *theme,
                                                           const char       *icon_name,
                                                           int               size,
                                                           int               scale,
                                                           gboolean          allow_svg);
static void              theme_subdir_load                (GtkIconTheme     *self,
                                                           IconTheme        *theme,
                                                           GKeyFile         *theme_file,
                                                           char             *subdir);
static void              do_theme_change                  (GtkIconTheme     *self);
static void              blow_themes                      (GtkIconTheme     *self);
static gboolean          rescan_themes                    (GtkIconTheme     *self);
static GtkIconPaintable *icon_paintable_new               (const char       *icon_name,
                                                           int               desired_size,
                                                           int               desired_scale);
static inline IconCacheFlag
                         suffix_from_name                 (const char       *name);
static void              icon_ensure_texture__locked      (GtkIconPaintable *icon,
                                                           gboolean          in_thread);
static void              gtk_icon_theme_unset_display     (GtkIconTheme     *self);
static void              gtk_icon_theme_set_display       (GtkIconTheme     *self,
                                                           GdkDisplay       *display);
static void              update_current_theme__mainthread (GtkIconTheme     *self);
static gboolean          ensure_valid_themes              (GtkIconTheme     *self,
                                                           gboolean          non_blocking);


static guint signal_changed = 0;

/* This is like a weak ref with a lock, anyone doing
 * operations on the theme must take the lock in this,
 * but you can also take the lock even if the theme
 * has been finalized (but theme will then be NULL).
 *
 * This is used to avoid race conditions where signals
 * like theme-changed happen on the main thread while
 * the last active owning ref of the icon theme is
 * on some thread.
 */
struct _GtkIconThemeRef
{
  gatomicrefcount count;
  GMutex lock;
  GtkIconTheme *theme;
};

static GtkIconThemeRef *
gtk_icon_theme_ref_new (GtkIconTheme *theme)
{
  GtkIconThemeRef *ref = g_new0 (GtkIconThemeRef, 1);

  g_atomic_ref_count_init (&ref->count);
  g_mutex_init (&ref->lock);
  ref->theme = theme;

  return ref;
}

static GtkIconThemeRef *
gtk_icon_theme_ref_ref (GtkIconThemeRef *ref)
{
  g_atomic_ref_count_inc (&ref->count);
  return ref;
}

static void
gtk_icon_theme_ref_unref (GtkIconThemeRef *ref)
{
  if (g_atomic_ref_count_dec (&ref->count))
    {
      g_assert (ref->theme == NULL);
      g_mutex_clear (&ref->lock);
      g_free (ref);
    }
}

/* Take the lock and if available ensure the theme lives until (at
 * least) ref_release is called. */
static GtkIconTheme *
gtk_icon_theme_ref_aquire (GtkIconThemeRef *ref)
{
  g_mutex_lock (&ref->lock);
  if (ref->theme)
    g_object_ref (ref->theme);
  return ref->theme;
}

static void
gtk_icon_theme_ref_release (GtkIconThemeRef *ref)
{
  GtkIconTheme *theme;

  /* Get a pointer to the theme, because when we unlock it could become NULLed by dispose, this pointer still owns a ref */
  theme = ref->theme;
  g_mutex_unlock (&ref->lock);

  /* Then unref outside the lock, because otherwise if this is the last ref the dispose handler would deadlock trying to NULL ref->theme */
  if (theme)
    g_object_unref (theme);

}

static void
gtk_icon_theme_ref_dispose (GtkIconThemeRef *ref)
{
  gtk_icon_theme_ref_aquire (ref);
  ref->theme = NULL;
  gtk_icon_theme_ref_release (ref);
}

static void
gtk_icon_theme_lock (GtkIconTheme *self)
{
  g_mutex_lock (&self->ref->lock);
}

static void
gtk_icon_theme_unlock (GtkIconTheme *self)
{
  g_mutex_unlock (&self->ref->lock);
}


static guint
icon_key_hash (gconstpointer _key)
{
  const IconKey *key = _key;
  guint h = 0;
  int i;
  for (i = 0; key->icon_names[i] != NULL; i++)
    h ^= g_str_hash (key->icon_names[i]);

  h ^= key->size * 0x10001;
  h ^= key->scale * 0x1000010;
  h ^= key->flags * 0x100000100;

  return h;
}

static gboolean
icon_key_equal (gconstpointer _a,
                gconstpointer _b)
{
  const IconKey *a = _a;
  const IconKey *b = _b;
  int i;

  if (a->size != b->size)
    return FALSE;

  if (a->scale != b->scale)
    return FALSE;

  if (a->flags != b->flags)
    return FALSE;

  for (i = 0;
       a->icon_names[i] != NULL &&
       b->icon_names[i] != NULL; i++)
    {
      if (strcmp (a->icon_names[i], b->icon_names[i]) != 0)
        return FALSE;
    }

  return a->icon_names[i] == NULL && b->icon_names[i] == NULL;
}

/****************** Icon cache ***********************
 *
 * The icon cache, this spans both GtkIconTheme and GtkIcon, so the locking is
 * a bit tricky. Never do block with the lock held, and never do any
 * callouts to other code. In particular, don't call theme or finalizers
 * because that will take the lock when removing from the icon cache.
 */

/* This is called with icon_cache lock held so must not take any locks */
static gboolean
_icon_cache_should_lru_cache (GtkIconPaintable *icon)
{
  return icon->desired_size <= MAX_LRU_TEXTURE_SIZE;
}

/* This returns the old lru element because we can't unref it with
 * the lock held */
static GtkIconPaintable *
_icon_cache_add_to_lru_cache (GtkIconTheme     *theme,
                              GtkIconPaintable *icon)
{
  GtkIconPaintable *old_icon = NULL;

  /* Avoid storing the same info multiple times in a row */
  if (theme->lru_cache[theme->lru_cache_current] != icon)
    {
      theme->lru_cache_current = (theme->lru_cache_current + 1) % LRU_CACHE_SIZE;
      old_icon = theme->lru_cache[theme->lru_cache_current];
      theme->lru_cache[theme->lru_cache_current] = g_object_ref (icon);
    }

  return old_icon;
}

static GtkIconPaintable *
icon_cache_lookup (GtkIconTheme *theme,
                   IconKey      *key)
{
  GtkIconPaintable *old_icon = NULL;
  GtkIconPaintable *icon;

  G_LOCK (icon_cache);

  icon = g_hash_table_lookup (theme->icon_cache, key);
  if (icon != NULL)
    {
      DEBUG_CACHE (("cache hit %p (%s %d 0x%x) (cache size %d)\n",
                    icon,
                    g_strjoinv (",", icon->key.icon_names),
                    icon->key.size, icon->key.flags,
                    g_hash_table_size (theme->icon_cache)));

      icon = g_object_ref (icon);

      /* Move item to front in LRU cache */
      if (_icon_cache_should_lru_cache (icon))
        old_icon = _icon_cache_add_to_lru_cache (theme, icon);
    }

  G_UNLOCK (icon_cache);

  /* Call potential finalizer outside the lock */
  if (old_icon)
    g_object_unref (old_icon);

  return icon;
}

/* The icon was removed from the icon_hash hash table.
 * This is a callback from the icon_cache hashtable, so the icon_cache lock is already held.
 */
static void
icon_uncached_cb (GtkIconPaintable *icon)
{
  DEBUG_CACHE (("removing %p (%s %d 0x%x) from cache (icon_them: %p)  (cache size %d)\n",
                icon,
                g_strjoinv (",", icon->key.icon_names),
                icon->key.size, icon->key.flags,
                self,
                icon_theme != NULL ? g_hash_table_size (self->icon_cache) : 0));
  g_assert (icon->in_cache != NULL);
  icon->in_cache = NULL;
}

static void
icon_cache_mark_used_if_cached (GtkIconPaintable *icon)
{
  GtkIconPaintable *old_icon = NULL;

  if (!_icon_cache_should_lru_cache (icon))
    return;

  G_LOCK (icon_cache);
  if (icon->in_cache)
    old_icon = _icon_cache_add_to_lru_cache (icon->in_cache, icon);
  G_UNLOCK (icon_cache);

  /* Call potential finalizers outside the lock */
  if (old_icon)
    g_object_unref (old_icon);
}

static void
icon_cache_add (GtkIconTheme     *theme,
                GtkIconPaintable *icon)
{
  GtkIconPaintable *old_icon = NULL;

  G_LOCK (icon_cache);
  icon->in_cache = theme;
  g_hash_table_insert (theme->icon_cache, &icon->key, icon);

  if (_icon_cache_should_lru_cache (icon))
    old_icon = _icon_cache_add_to_lru_cache (theme, icon);
  DEBUG_CACHE (("adding %p (%s %d 0x%x) to cache (cache size %d)\n",
                icon,
                g_strjoinv (",", icon->key.icon_names),
                icon->key.size, icon->key.flags,
                g_hash_table_size (theme->icon_cache)));
  G_UNLOCK (icon_cache);

  /* Call potential finalizer outside the lock */
  if (old_icon)
    g_object_unref (old_icon);
}

static void
icon_cache_remove (GtkIconPaintable *icon)
{
  G_LOCK (icon_cache);
  if (icon->in_cache)
    g_hash_table_remove (icon->in_cache->icon_cache, &icon->key);
  G_UNLOCK (icon_cache);
}

static void
icon_cache_clear (GtkIconTheme *theme)
{
  int i;
  GtkIconPaintable *old_icons[LRU_CACHE_SIZE];

  G_LOCK (icon_cache);
  g_hash_table_remove_all (theme->icon_cache);
  for (i = 0; i < LRU_CACHE_SIZE; i ++)
    {
      old_icons[i] = theme->lru_cache[i];
      theme->lru_cache[i] = NULL;
    }
  G_UNLOCK (icon_cache);

  /* Call potential finalizers outside the lock */
  for (i = 0; i < LRU_CACHE_SIZE; i ++)
    {
      if (old_icons[i] != NULL)
        g_object_unref (old_icons[i]);
    }
}

/****************** End of icon cache ***********************/

G_DEFINE_TYPE (GtkIconTheme, gtk_icon_theme, G_TYPE_OBJECT)

/**
 * gtk_icon_theme_new:
 *
 * Creates a new icon theme object.
 *
 * Icon theme objects are used to lookup up an icon by name
 * in a particular icon theme. Usually, you’ll want to use
 * [func@Gtk.IconTheme.get_for_display] rather than creating
 * a new icon theme object for scratch.
 *
 * Returns: the newly created `GtkIconTheme` object.
 */
GtkIconTheme *
gtk_icon_theme_new (void)
{
  return g_object_new (GTK_TYPE_ICON_THEME, NULL);
}

static void
load_theme_thread  (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  GtkIconTheme *self = GTK_ICON_THEME (source_object);

  gtk_icon_theme_lock (self);
  ensure_valid_themes (self, FALSE);
  gtk_icon_theme_unlock (self);
  g_task_return_pointer (task, NULL, NULL);
}

static void
gtk_icon_theme_load_in_thread (GtkIconTheme *self)
{
  GTask *task;

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_task_data (task, g_object_ref (self), g_object_unref);
  g_task_run_in_thread (task, load_theme_thread);
  g_object_unref (task);
}

/**
 * gtk_icon_theme_get_for_display:
 * @display: a `GdkDisplay`
 *
 * Gets the icon theme object associated with @display.
 *
 * If this function has not previously been called for the given
 * display, a new icon theme object will be created and associated
 * with the display. Icon theme objects are fairly expensive to create,
 * so using this function is usually a better choice than calling
 * [ctor@Gtk.IconTheme.new] and setting the display yourself; by using
 * this function a single icon theme object will be shared between users.
 *
 * Returns: (transfer none): A unique `GtkIconTheme` associated with
 *   the given display. This icon theme is associated with the display
 *   and can be used as long as the display is open. Do not ref or unref it.
 */
GtkIconTheme *
gtk_icon_theme_get_for_display (GdkDisplay *display)
{
  GtkIconTheme *self;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  self = g_object_get_data (G_OBJECT (display), "gtk-icon-theme");
  if (!self)
    {
      self = gtk_icon_theme_new ();
      self->is_display_singleton = TRUE;
      g_object_set_data (G_OBJECT (display), I_("gtk-icon-theme"), self);

      /* Call this after setting the user-data, because it recurses into gtk_icon_theme_get_for_display via the thememing machinery */
      gtk_icon_theme_set_display (self, display);

      /* Queue early read of the default themes, we read the icon theme name in set_display(). */
      gtk_icon_theme_load_in_thread (self);
    }

  return self;
}

enum {
  PROP_DISPLAY = 1,
  PROP_ICON_NAMES,
  PROP_SEARCH_PATH,
  PROP_RESOURCE_PATH,
  PROP_THEME_NAME,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP];

static void
gtk_icon_theme_get_property (GObject      *object,
                             guint         prop_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  GtkIconTheme *self = GTK_ICON_THEME (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, self->display);
      break;

    case PROP_ICON_NAMES:
      g_value_take_boxed (value, gtk_icon_theme_get_icon_names (self));
      break;

    case PROP_SEARCH_PATH:
      g_value_set_boxed (value, self->search_path);
      break;

    case PROP_RESOURCE_PATH:
      g_value_set_boxed (value, self->resource_path);
      break;

    case PROP_THEME_NAME:
      g_value_take_string (value, gtk_icon_theme_get_theme_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_icon_theme_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkIconTheme *self = GTK_ICON_THEME (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      gtk_icon_theme_set_display (self, g_value_get_object (value));
      break;

    case PROP_SEARCH_PATH:
      gtk_icon_theme_set_search_path (self, g_value_get_boxed (value));
      break;

    case PROP_RESOURCE_PATH:
      gtk_icon_theme_set_resource_path (self, g_value_get_boxed (value));
      break;

    case PROP_THEME_NAME:
      gtk_icon_theme_set_theme_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_icon_theme_class_init (GtkIconThemeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_icon_theme_finalize;
  gobject_class->dispose = gtk_icon_theme_dispose;
  gobject_class->get_property = gtk_icon_theme_get_property;
  gobject_class->set_property = gtk_icon_theme_set_property;

  /**
   * GtkIconTheme::changed:
   * @self: the icon theme
   *
   * Emitted when the icon theme changes.
   *
   * This can happen because current icon theme is switched or
   * because GTK detects that a change has occurred in the
   * contents of the current icon theme.
   */
  signal_changed = g_signal_new (I_("changed"),
                                 G_TYPE_FROM_CLASS (klass),
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET (GtkIconThemeClass, changed),
                                 NULL, NULL,
                                 NULL,
                                 G_TYPE_NONE, 0);

  /**
   * GtkIconTheme:display:
   *
   * The display that this icon theme object is attached to.
   */
  props[PROP_DISPLAY] =
      g_param_spec_object ("display", NULL, NULL,
                           GDK_TYPE_DISPLAY,
                           G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkIconTheme:icon-names:
   *
   * The icon names that are supported by the icon theme.
   */
  props[PROP_ICON_NAMES] =
      g_param_spec_boxed ("icon-names", NULL, NULL,
                          G_TYPE_STRV,
                          GTK_PARAM_READABLE);

  /**
   * GtkIconTheme:search-path:
   *
   * The search path for this icon theme.
   *
   * When looking for icons, GTK will search for a subdirectory of
   * one or more of the directories in the search path with the same
   * name as the icon theme containing an index.theme file. (Themes
   * from multiple of the path elements are combined to allow themes
   * to be extended by adding icons in the user’s home directory.)
   */
  props[PROP_SEARCH_PATH] =
      g_param_spec_boxed ("search-path", NULL, NULL,
                          G_TYPE_STRV,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkIconTheme:resource-path:
   *
   * Resource paths that will be looked at when looking for icons,
   * similar to search paths.
   *
   * The resources are considered as part of the hicolor icon theme
   * and must be located in subdirectories that are defined in the
   * hicolor icon theme, such as `@path/16x16/actions/run.png`.
   * Icons that are directly placed in the resource path instead
   * of a subdirectory are also considered as ultimate fallback.
   */
  props[PROP_RESOURCE_PATH] =
      g_param_spec_boxed ("resource-path", NULL, NULL,
                          G_TYPE_STRV,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkIconTheme:theme-name:
   *
   * The name of the icon theme that is being used.
   *
   * Unless set to a different value, this will be the value of
   * the `GtkSettings:gtk-icon-theme-name` property of the `GtkSettings`
   * object associated to the display of the icontheme object. 
   */
  props[PROP_THEME_NAME] =
      g_param_spec_string ("theme-name", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_PROP, props);
}


/* Callback when the display that the icon theme is attached
 * to is closed; unset the display, and if it’s the unique theme
 * for the display, drop the reference
 */
static void
display_closed__mainthread_unlocked (GdkDisplay      *display,
                                     gboolean         is_error,
                                     GtkIconThemeRef *ref)
{
  GtkIconTheme *self = gtk_icon_theme_ref_aquire (ref);
  gboolean was_display_singleton;

  if (self)
    {
      /* This is only set at construction and accessed here in the main thread, so no locking necessary */
      was_display_singleton = self->is_display_singleton;
      if (was_display_singleton)
        {
          g_object_set_data (G_OBJECT (display), I_("gtk-icon-theme"), NULL);
          self->is_display_singleton = FALSE;
        }

      gtk_icon_theme_unset_display (self);
      update_current_theme__mainthread (self);

      if (was_display_singleton)
        g_object_unref (self);
    }

  gtk_icon_theme_ref_release (ref);
}

static void
update_current_theme__mainthread (GtkIconTheme *self)
{
#define theme_changed(_old, _new) \
  ((_old && !_new) || (!_old && _new) || \
   (_old && _new && strcmp (_old, _new) != 0))

  if (!self->custom_theme)
    {
      char *theme = NULL;
      gboolean changed = FALSE;

      if (self->display)
        {
          GtkSettings *settings = gtk_settings_get_for_display (self->display);
          g_object_get (settings, "gtk-icon-theme-name", &theme, NULL);
        }

      if (theme_changed (self->current_theme, theme))
        {
          g_free (self->current_theme);
          self->current_theme = theme;
          changed = TRUE;
        }
      else
        g_free (theme);

      if (changed)
        do_theme_change (self);
    }
#undef theme_changed
}

/* Callback when the icon theme GtkSetting changes
 */
static void
theme_changed__mainthread_unlocked (GtkSettings     *settings,
                                    GParamSpec      *pspec,
                                    GtkIconThemeRef *ref)
{
  GtkIconTheme *self = gtk_icon_theme_ref_aquire (ref);

  if (self)
    {
      update_current_theme__mainthread (self);

      /* Queue early read of the new theme */
      gtk_icon_theme_load_in_thread (self);
    }

  gtk_icon_theme_ref_release (ref);
}

static void
gtk_icon_theme_unset_display (GtkIconTheme *self)
{
  if (self->display)
    {
      g_signal_handlers_disconnect_by_func (self->display,
                                            (gpointer) display_closed__mainthread_unlocked,
                                            self->ref);
      g_signal_handlers_disconnect_by_func (self->display_settings,
                                            (gpointer) theme_changed__mainthread_unlocked,
                                            self->ref);

      self->display = NULL;
      self->display_settings = NULL;

      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DISPLAY]);
    }
}

void
gtk_icon_theme_set_display (GtkIconTheme *self,
                            GdkDisplay   *display)
{
  g_return_if_fail (GTK_ICON_THEME (self));
  g_return_if_fail (display == NULL || GDK_IS_DISPLAY (display));

  gtk_icon_theme_lock (self);

  g_object_freeze_notify (G_OBJECT (self));

  gtk_icon_theme_unset_display (self);

  if (display)
    {
      self->display = display;
      self->display_settings = gtk_settings_get_for_display (display);

      g_signal_connect_data (display, "closed",
                             G_CALLBACK (display_closed__mainthread_unlocked),
                             gtk_icon_theme_ref_ref (self->ref),
                             (GClosureNotify)gtk_icon_theme_ref_unref,
                             0);
      g_signal_connect_data (self->display_settings, "notify::gtk-icon-theme-name",
                             G_CALLBACK (theme_changed__mainthread_unlocked),
                             gtk_icon_theme_ref_ref (self->ref),
                             (GClosureNotify)gtk_icon_theme_ref_unref,
                             0);

      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DISPLAY]);
    }

  update_current_theme__mainthread (self);

  gtk_icon_theme_unlock (self);

  g_object_thaw_notify (G_OBJECT (self));
}

/* Checks whether a loader for SVG files has been registered
 * with GdkPixbuf.
 */
static gboolean
pixbuf_supports_svg (void)
{
  GSList *formats;
  GSList *tmp_list;
  static int found_svg = -1;

  if (found_svg != -1)
    return found_svg;

  formats = gdk_pixbuf_get_formats ();

  found_svg = FALSE;
  for (tmp_list = formats; tmp_list && !found_svg; tmp_list = tmp_list->next)
    {
      char **mime_types = gdk_pixbuf_format_get_mime_types (tmp_list->data);
      char **mime_type;

      for (mime_type = mime_types; *mime_type && !found_svg; mime_type++)
        {
          if (strcmp (*mime_type, "image/svg") == 0)
            found_svg = TRUE;
        }

      g_strfreev (mime_types);
    }

  g_slist_free (formats);

  return found_svg;
}

static void
free_dir_mtime (IconThemeDirMtime *dir_mtime)
{
  if (dir_mtime->cache)
    gtk_icon_cache_unref (dir_mtime->cache);

  g_free (dir_mtime->dir);
}

static void
gtk_icon_theme_init (GtkIconTheme *self)
{
  const char * const *xdg_data_dirs;
  int i, j;
  int len;

  self->ref = gtk_icon_theme_ref_new (self);

  self->icon_cache = g_hash_table_new_full (icon_key_hash, icon_key_equal, NULL,
                                            (GDestroyNotify)icon_uncached_cb);

  self->custom_theme = FALSE;
  self->dir_mtimes = g_array_new (FALSE, TRUE, sizeof (IconThemeDirMtime));
  g_array_set_clear_func (self->dir_mtimes, (GDestroyNotify)free_dir_mtime);

  xdg_data_dirs = g_get_system_data_dirs ();
  for (i = 0; xdg_data_dirs[i]; i++) ;

  len = 2 * i + 3;

  self->search_path = g_new (char *, len);

  i = 0;
  self->search_path[i++] = g_build_filename (g_get_user_data_dir (), "icons", NULL);
  self->search_path[i++] = g_build_filename (g_get_home_dir (), ".icons", NULL);

  for (j = 0; xdg_data_dirs[j]; j++)
    self->search_path[i++] = g_build_filename (xdg_data_dirs[j], "icons", NULL);

  for (j = 0; xdg_data_dirs[j]; j++)
    self->search_path[i++] = g_build_filename (xdg_data_dirs[j], "pixmaps", NULL);

  self->search_path[i] = NULL;

  self->resource_path = g_new (char *, 2);
  self->resource_path[0] = g_strdup ("/org/gtk/libgtk/icons/");
  self->resource_path[1] = NULL;

  self->themes_valid = FALSE;
  self->themes = NULL;
  self->unthemed_icons = NULL;

  self->pixbuf_supports_svg = pixbuf_supports_svg ();
}

static gboolean
theme_changed_idle__mainthread_unlocked (gpointer user_data)
{
  GtkIconThemeRef *ref = (GtkIconThemeRef *)user_data;
  GtkIconTheme *self;
  GdkDisplay *display = NULL;

  self = gtk_icon_theme_ref_aquire (ref);
  if (self)
    {
      g_object_ref (self); /* Ensure theme lives during the changed signal emissions */

      self->theme_changed_idle = 0;

      if (self->display && self->is_display_singleton)
        display = g_object_ref (self->display);
    }
  gtk_icon_theme_ref_release (ref);

  if (self)
    {
      /* Emit signals outside locks. */
      g_signal_emit (self, signal_changed, 0);

      if (display)
        {
          GtkSettings *settings = gtk_settings_get_for_display (self->display);
          gtk_style_provider_changed (GTK_STYLE_PROVIDER (settings));
          gtk_system_setting_changed (display, GTK_SYSTEM_SETTING_ICON_THEME);
          g_object_unref (display);
        }

      g_object_unref (self);
    }

  return FALSE;
}

static void
queue_theme_changed (GtkIconTheme *self)
{
  if (!self->theme_changed_idle)
    {
      self->theme_changed_idle = g_idle_add_full (GTK_PRIORITY_RESIZE - 2,
                                                  theme_changed_idle__mainthread_unlocked,
                                                  gtk_icon_theme_ref_ref (self->ref),
                                                  (GDestroyNotify)gtk_icon_theme_ref_unref);
      gdk_source_set_static_name_by_id (self->theme_changed_idle, "[gtk] theme_changed_idle");
    }
}

static void
do_theme_change (GtkIconTheme *self)
{
  icon_cache_clear (self);

  if (!self->themes_valid)
    return;

  GTK_DISPLAY_DEBUG (self->display, ICONTHEME, "change to icon theme \"%s\"", self->current_theme);
  blow_themes (self);

  queue_theme_changed (self);
}

static void
blow_themes (GtkIconTheme *self)
{
  if (self->themes_valid)
    {
      g_list_free_full (self->themes, (GDestroyNotify) theme_destroy);
      g_array_set_size (self->dir_mtimes, 0);
      g_hash_table_destroy (self->unthemed_icons);
      gtk_string_set_destroy (&self->icons);
    }
  self->themes = NULL;
  self->unthemed_icons = NULL;
  self->themes_valid = FALSE;
  self->serial++;
}

int
gtk_icon_theme_get_serial (GtkIconTheme *self)
{
  return self->serial;
}

static void
gtk_icon_theme_dispose (GObject *object)
{
  GtkIconTheme *self = GTK_ICON_THEME (object);

  /* We make sure all outstanding GtkIconThemeRefs to us are NULLed
   * out so that no other threads than the one running finalize will
   * refer to the icon theme after this. This could happen if
   * we finalize on a thread and on the main thread some display or
   * setting signal is emitted.
   *
   * It is possible that before we acquire the lock this happens
   * and the other thread refs the icon theme for some reason, but
   * this is ok as it is allowed to resurrect objects in dispose
   * (but not in finalize).
   */
  gtk_icon_theme_ref_dispose (self->ref);

  G_OBJECT_CLASS (gtk_icon_theme_parent_class)->dispose (object);
}

static void
gtk_icon_theme_finalize (GObject *object)
{
  GtkIconTheme *self = GTK_ICON_THEME (object);

  /* We don't actually need to take the lock here, because by now
     there can be no other threads that own a ref to this object, but
     technically this is considered "locked" */

  icon_cache_clear (self);

  if (self->theme_changed_idle)
    g_source_remove (self->theme_changed_idle);

  gtk_icon_theme_unset_display (self);

  g_free (self->current_theme);

  g_strfreev (self->search_path);
  g_strfreev (self->resource_path);

  blow_themes (self);
  g_array_free (self->dir_mtimes, TRUE);

  gtk_icon_theme_ref_unref (self->ref);

  G_OBJECT_CLASS (gtk_icon_theme_parent_class)->finalize (object);
}

/**
 * gtk_icon_theme_set_search_path:
 * @self: a `GtkIconTheme`
 * @path: (array zero-terminated=1) (element-type filename) (nullable): NULL-terminated
 *   array of directories that are searched for icon themes
 *
 * Sets the search path for the icon theme object.
 *
 * When looking for an icon theme, GTK will search for a subdirectory
 * of one or more of the directories in @path with the same name
 * as the icon theme containing an index.theme file. (Themes from
 * multiple of the path elements are combined to allow themes to be
 * extended by adding icons in the user’s home directory.)
 *
 * In addition if an icon found isn’t found either in the current
 * icon theme or the default icon theme, and an image file with
 * the right name is found directly in one of the elements of
 * @path, then that image will be used for the icon name.
 * (This is legacy feature, and new icons should be put
 * into the fallback icon theme, which is called hicolor,
 * rather than directly on the icon path.)
 */
void
gtk_icon_theme_set_search_path (GtkIconTheme       *self,
                                const char * const *path)
{
  char **search_path;

  g_return_if_fail (GTK_IS_ICON_THEME (self));

  gtk_icon_theme_lock (self);

  search_path = g_strdupv ((char **)path);
  g_strfreev (self->search_path);
  self->search_path = search_path;

  do_theme_change (self);

  gtk_icon_theme_unlock (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SEARCH_PATH]);
}

/**
 * gtk_icon_theme_get_search_path:
 * @self: a `GtkIconTheme`
 *
 * Gets the current search path.
 *
 * See [method@Gtk.IconTheme.set_search_path].
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type filename) (nullable):
 *   a list of icon theme path directories
 */
char **
gtk_icon_theme_get_search_path (GtkIconTheme  *self)
{
  char **paths;

  g_return_val_if_fail (GTK_IS_ICON_THEME (self), NULL);

  gtk_icon_theme_lock (self);

  paths = g_strdupv (self->search_path);

  gtk_icon_theme_unlock (self);

  return paths;
}

/**
 * gtk_icon_theme_add_search_path:
 * @self: a `GtkIconTheme`
 * @path: (type filename): directory name to append to the icon path
 *
 * Appends a directory to the search path.
 *
 * See [method@Gtk.IconTheme.set_search_path].
 */
void
gtk_icon_theme_add_search_path (GtkIconTheme *self,
                                const char   *path)
{
  char **paths;
  int len;

  g_return_if_fail (GTK_IS_ICON_THEME (self));
  g_return_if_fail (path != NULL);
 
  len = g_strv_length (self->search_path);
  paths = g_new (char *, len + 2);
  memcpy (paths, self->search_path, sizeof (char *) * len);
  paths[len] = (char *)path;
  paths[len + 1] = NULL;

  gtk_icon_theme_set_search_path (self, (const char * const *)paths);

  g_free (paths);
}

/**
 * gtk_icon_theme_set_resource_path:
 * @self: a `GtkIconTheme`
 * @path: (array zero-terminated=1) (element-type utf8) (nullable): 
 *   NULL-terminated array of resource paths
 *   that are searched for icons
 *
 * Sets the resource paths that will be looked at when
 * looking for icons, similar to search paths.
 *
 * The resources are considered as part of the hicolor icon theme
 * and must be located in subdirectories that are defined in the
 * hicolor icon theme, such as `@path/16x16/actions/run.png`
 * or `@path/scalable/actions/run.svg`.
 *
 * Icons that are directly placed in the resource path instead
 * of a subdirectory are also considered as ultimate fallback,
 * but they are treated like unthemed icons.
 */
void
gtk_icon_theme_set_resource_path (GtkIconTheme       *self,
                                  const char * const *path)
{
  char **search_path;

  g_return_if_fail (GTK_IS_ICON_THEME (self));

  gtk_icon_theme_lock (self);

  search_path = g_strdupv ((char **)path);
  g_strfreev (self->resource_path);
  self->resource_path = search_path;

  do_theme_change (self);

  gtk_icon_theme_unlock (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_RESOURCE_PATH]);
}

/**
 * gtk_icon_theme_get_resource_path:
 * @self: a `GtkIconTheme`
 *
 * Gets the current resource path.
 *
 * See [method@Gtk.IconTheme.set_resource_path].
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8) (nullable):
 *   A list of resource paths
 */
char **
gtk_icon_theme_get_resource_path (GtkIconTheme  *self)
{
  char **paths;

  g_return_val_if_fail (GTK_IS_ICON_THEME (self), NULL);

  gtk_icon_theme_lock (self);

  paths = g_strdupv (self->resource_path);

  gtk_icon_theme_unlock (self);

  return paths;
}

/**
 * gtk_icon_theme_add_resource_path:
 * @self: a `GtkIconTheme`
 * @path: a resource path
 *
 * Adds a resource path that will be looked at when looking
 * for icons, similar to search paths.
 *
 * See [method@Gtk.IconTheme.set_resource_path].
 *
 * This function should be used to make application-specific icons
 * available as part of the icon theme.
 */
void
gtk_icon_theme_add_resource_path (GtkIconTheme *self,
                                  const char   *path)
{
  char **paths;
  int len;

  g_return_if_fail (GTK_IS_ICON_THEME (self));
  g_return_if_fail (path != NULL);
 
  len = g_strv_length (self->resource_path);
  paths = g_new (char *, len + 2);
  memcpy (paths, self->resource_path, sizeof (char *) * len);
  paths[len] = (char *)path;
  paths[len + 1] = NULL;

  gtk_icon_theme_set_resource_path (self, (const char * const *)paths);

  g_free (paths);
}

/**
 * gtk_icon_theme_set_theme_name:
 * @self: a `GtkIconTheme`
 * @theme_name: (nullable): name of icon theme to use instead of
 *   configured theme, or %NULL to unset a previously set custom theme
 *
 * Sets the name of the icon theme that the `GtkIconTheme` object uses
 * overriding system configuration.
 *
 * This function cannot be called on the icon theme objects returned
 * from [func@Gtk.IconTheme.get_for_display].
 */
void
gtk_icon_theme_set_theme_name (GtkIconTheme *self,
                               const char   *theme_name)
{
  g_return_if_fail (GTK_IS_ICON_THEME (self));
  g_return_if_fail (!self->is_display_singleton);

  gtk_icon_theme_lock (self);

  if (theme_name != NULL)
    {
      self->custom_theme = TRUE;
      if (!self->current_theme || strcmp (theme_name, self->current_theme) != 0)
        {
          g_free (self->current_theme);
          self->current_theme = g_strdup (theme_name);

          do_theme_change (self);
        }
    }
  else
    {
      if (self->custom_theme)
        {
          self->custom_theme = FALSE;
          update_current_theme__mainthread (self);
        }
    }

  gtk_icon_theme_unlock (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_THEME_NAME]);
}

/**
 * gtk_icon_theme_get_theme_name:
 * @self: a `GtkIconTheme`
 *
 * Gets the current icon theme name.
 *
 * Returns (transfer full): the current icon theme name,
 */
char *
gtk_icon_theme_get_theme_name (GtkIconTheme *self)
{
  char *theme_name;

  g_return_val_if_fail (GTK_IS_ICON_THEME (self), NULL);

  gtk_icon_theme_lock (self);

  if (self->custom_theme)
    theme_name = g_strdup (self->current_theme);
  else
    {
      if (self->display)
        {
          GtkSettings *settings = gtk_settings_get_for_display (self->display);
          g_object_get (settings, "gtk-icon-theme-name", &theme_name, NULL);
        }
      else
        theme_name = NULL;
    }

  gtk_icon_theme_unlock (self);

  return theme_name;
}

static void
insert_theme (GtkIconTheme *self,
              const char   *theme_name)
{
  int i;
  GList *l;
  char **dirs;
  char **scaled_dirs;
  char **themes;
  IconTheme *theme = NULL;
  char *path;
  GKeyFile *theme_file;
  GStatBuf stat_buf;

  for (l = self->themes; l != NULL; l = l->next)
    {
      theme = l->data;
      if (strcmp (theme->name, theme_name) == 0)
        return;
    }

  theme_file = NULL;
  for (i = 0; self->search_path[i]; i++)
    {
      IconThemeDirMtime dir_mtime;

      path = g_build_filename (self->search_path[i], theme_name, NULL);
      dir_mtime.cache = NULL;
      dir_mtime.dir = path;
      if (g_stat (path, &stat_buf) == 0 && S_ISDIR (stat_buf.st_mode))
        {
          dir_mtime.mtime = stat_buf.st_mtime;
          dir_mtime.exists = TRUE;

          if (!theme_file)
            {
              char *file = g_build_filename (path, "index.theme", NULL);
              if (g_file_test (file, G_FILE_TEST_IS_REGULAR))
                {
                  theme_file = g_key_file_new ();
                  g_key_file_set_list_separator (theme_file, ',');
                  if (!g_key_file_load_from_file (theme_file, file, 0, NULL))
                    {
                      g_key_file_free (theme_file);
                      theme_file = NULL;
                    }
                }
              g_free (file);
            }
        }
      else
        {
          dir_mtime.mtime = 0;
          dir_mtime.exists = FALSE;
        }

      g_array_insert_val (self->dir_mtimes, 0, dir_mtime);
    }

  if (theme_file == NULL)
    {
      if (strcmp (theme_name, FALLBACK_ICON_THEME) == 0)
        {
          const char *resource_path = "/org/gtk/libgtk/icons/hicolor.index.theme";
          GBytes *index;

          index = g_resources_lookup_data (resource_path,
                                           G_RESOURCE_LOOKUP_FLAGS_NONE,
                                           NULL);
          if (!index)
            g_error ("Cannot find resource %s", resource_path);

          theme_file = g_key_file_new ();
          g_key_file_set_list_separator (theme_file, ',');
          g_key_file_load_from_bytes (theme_file, index, G_KEY_FILE_NONE, NULL);

          g_bytes_unref (index);
        }
      else
        return;
    }

  dirs = g_key_file_get_string_list (theme_file, "Icon Theme", "Directories", NULL, NULL);
  if (!dirs)
    {
      g_warning ("Theme file for %s has no directories", theme_name);
      g_key_file_free (theme_file);
      return;
    }

  scaled_dirs = g_key_file_get_string_list (theme_file, "Icon Theme", "ScaledDirectories", NULL, NULL);

  theme = theme_new (theme_name, theme_file);
  self->themes = g_list_prepend (self->themes, theme);

  for (i = 0; dirs[i] != NULL; i++)
    theme_subdir_load (self, theme, theme_file, dirs[i]);

  if (scaled_dirs)
    {
      for (i = 0; scaled_dirs[i] != NULL; i++)
        theme_subdir_load (self, theme, theme_file, scaled_dirs[i]);
    }

  g_strfreev (dirs);
  g_strfreev (scaled_dirs);

  themes = g_key_file_get_string_list (theme_file,
                                       "Icon Theme",
                                       "Inherits",
                                       NULL,
                                       NULL);
  if (themes)
    {
      for (i = 0; themes[i] != NULL; i++)
        insert_theme (self, themes[i]);

      g_strfreev (themes);
    }

  g_key_file_free (theme_file);
}

static void
free_unthemed_icon (UnthemedIcon *unthemed_icon)
{
  g_free (unthemed_icon->svg_filename);
  g_free (unthemed_icon->no_svg_filename);
  g_free (unthemed_icon);
}

static inline void
strip_suffix_inline (char          *filename,
                     IconCacheFlag  suffix)
{
  if (suffix & ICON_CACHE_FLAG_SYMBOLIC_PNG_SUFFIX)
    filename[strlen (filename) - strlen (".symbolic.png")] = 0;
  else if (suffix & (ICON_CACHE_FLAG_XPM_SUFFIX|
                     ICON_CACHE_FLAG_SVG_SUFFIX|
                     ICON_CACHE_FLAG_PNG_SUFFIX))
    filename[strlen (filename) - 4] = 0;
}

static inline char *
strip_suffix (const char    *filename,
              IconCacheFlag  suffix)
{
  char *dup = g_strdup (filename);
  strip_suffix_inline (dup, suffix);
  return dup;
}

static void
add_unthemed_icon (GtkIconTheme *self,
                   const char   *dir,
                   const char   *file,
                   gboolean      is_resource)
{
  IconCacheFlag new_suffix, old_suffix;
  char *abs_file;
  char *base_name;
  UnthemedIcon *unthemed_icon;

  new_suffix = suffix_from_name (file);

  if (new_suffix == ICON_CACHE_FLAG_NONE)
    return;

  abs_file = g_build_filename (dir, file, NULL);
  base_name = strip_suffix (file, new_suffix);

  unthemed_icon = g_hash_table_lookup (self->unthemed_icons, base_name);

  if (unthemed_icon)
    {
      if (new_suffix == ICON_CACHE_FLAG_SVG_SUFFIX)
        {
          if (unthemed_icon->svg_filename)
            g_free (abs_file);
          else
            unthemed_icon->svg_filename = abs_file;
        }
      else
        {
          if (unthemed_icon->no_svg_filename)
            {
              old_suffix = suffix_from_name (unthemed_icon->no_svg_filename);
              if (new_suffix > old_suffix)
                {
                  g_free (unthemed_icon->no_svg_filename);
                  unthemed_icon->no_svg_filename = abs_file;
                }
              else
                g_free (abs_file);
            }
          else
            unthemed_icon->no_svg_filename = abs_file;
        }

      g_free (base_name);
    }
  else
    {
      unthemed_icon = g_new0 (UnthemedIcon, 1);

      unthemed_icon->is_resource = is_resource;

      if (new_suffix == ICON_CACHE_FLAG_SVG_SUFFIX)
        unthemed_icon->svg_filename = abs_file;
      else
        unthemed_icon->no_svg_filename = abs_file;

      /* takes ownership of base_name */
      g_hash_table_replace (self->unthemed_icons, base_name, unthemed_icon);
    }
}

static void
load_themes (GtkIconTheme *self)
{
  GDir *gdir;
  int base;
  char *dir;
  const char *file;
  GStatBuf stat_buf;
  int j;

  gtk_string_set_init (&self->icons);

  if (self->current_theme)
    insert_theme (self, self->current_theme);

  insert_theme (self, FALLBACK_ICON_THEME);
  self->themes = g_list_reverse (self->themes);

  self->unthemed_icons = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                g_free, (GDestroyNotify)free_unthemed_icon);

  for (base = 0; self->search_path[base]; base++)
    {
      IconThemeDirMtime *dir_mtime;
      dir = self->search_path[base];

      g_array_set_size (self->dir_mtimes, self->dir_mtimes->len + 1);
      dir_mtime = &g_array_index (self->dir_mtimes, IconThemeDirMtime, self->dir_mtimes->len - 1);

      dir_mtime->dir = g_strdup (dir);
      dir_mtime->mtime = 0;
      dir_mtime->exists = FALSE;
      dir_mtime->cache = NULL;

      if (g_stat (dir, &stat_buf) != 0 || !S_ISDIR (stat_buf.st_mode))
        continue;
      dir_mtime->mtime = stat_buf.st_mtime;
      dir_mtime->exists = TRUE;

      dir_mtime->cache = gtk_icon_cache_new_for_path (dir);
      if (dir_mtime->cache != NULL)
        continue;

      gdir = g_dir_open (dir, 0, NULL);
      if (gdir == NULL)
        continue;

      while ((file = g_dir_read_name (gdir)))
        add_unthemed_icon (self, dir, file, FALSE);

      g_dir_close (gdir);
    }

  for (j = 0; self->resource_path[j]; j++)
    {
      char **children;
      int i;

      dir = self->resource_path[j];
      children = g_resources_enumerate_children (dir, 0, NULL);
      if (!children)
        continue;

      for (i = 0; children[i]; i++)
        add_unthemed_icon (self, dir, children[i], TRUE);

      g_strfreev (children);
    }

  self->themes_valid = TRUE;

  self->last_stat_time = g_get_monotonic_time ();

  if (GTK_DISPLAY_DEBUG_CHECK (self->display, ICONTHEME))
    {
      GList *l;
      GString *s;
      s = g_string_new ("Current icon themes ");
      for (l = self->themes; l; l = l->next)
        {
          IconTheme *theme = l->data;
          g_string_append (s, theme->name);
          g_string_append_c (s, ' ');
        }
      gdk_debug_message ("%s", s->str);
      g_string_free (s, TRUE);

      dump_string_set (&self->icons);
    }
}

static gboolean
ensure_valid_themes (GtkIconTheme *self,
                     gboolean      non_blocking)
{
  gboolean was_valid = self->themes_valid;

  if (self->themes_valid)
    {
      gint64 now = g_get_monotonic_time ();

      if ((now - self->last_stat_time) / G_USEC_PER_SEC > 5)
        {
          if (non_blocking)
            return FALSE;

          if (rescan_themes (self))
            {
              icon_cache_clear (self);
              blow_themes (self);
            }
        }
    }

  if (!self->themes_valid)
    {
      gint64 before G_GNUC_UNUSED;

      if (non_blocking)
        return FALSE;

       before = GDK_PROFILER_CURRENT_TIME;

      load_themes (self);

      gdk_profiler_end_mark (before, "Icon theme load", self->current_theme);

      if (was_valid)
        queue_theme_changed (self);
    }

  return TRUE;
}

static inline gboolean
icon_name_is_symbolic (const char *icon_name,
                       int          icon_name_len)
{

  if (icon_name_len < 0)
    icon_name_len = strlen (icon_name);

  if (icon_name_len > strlen ("-symbolic"))
    {
      if (strcmp (icon_name + icon_name_len - strlen ("-symbolic"), "-symbolic") == 0)
        return TRUE;
    }

  if (icon_name_len > strlen ("-symbolic-ltr"))
    {
      if (strcmp (icon_name + icon_name_len - strlen ("-symbolic-ltr"), "-symbolic-ltr") == 0 ||
          strcmp (icon_name + icon_name_len - strlen ("-symbolic-rtl"), "-symbolic-rtl") == 0)
        return TRUE;
    }

  return FALSE;
}

static inline gboolean
icon_uri_is_symbolic (const char *icon_name,
                      int          icon_name_len)
{
  if (icon_name_len < 0)
    icon_name_len = strlen (icon_name);

  if (icon_name_len > strlen ("-symbolic.svg"))
    {
      if (strcmp (icon_name + icon_name_len - strlen ("-symbolic.svg"), "-symbolic.svg") == 0 ||
          strcmp (icon_name + icon_name_len - strlen (".symbolic.png"), ".symbolic.png") == 0)
        return TRUE;
    }

  if (icon_name_len > strlen ("-symbolic-ltr.svg"))
    {
      if (strcmp (icon_name + icon_name_len - strlen ("-symbolic.ltr.svg"), "-symbolic-ltr.svg") == 0 ||
          strcmp (icon_name + icon_name_len - strlen ("-symbolic.rtl.svg"), "-symbolic-rtl.svg") == 0)
        return TRUE;
    }

  return FALSE;
}

static GtkIconPaintable *
real_choose_icon (GtkIconTheme      *self,
                  const char        *icon_names[],
                  int                size,
                  int                scale,
                  GtkIconLookupFlags flags,
                  gboolean           non_blocking)
{
  GList *l;
  GtkIconPaintable *icon = NULL;
  UnthemedIcon *unthemed_icon = NULL;
  const char *icon_name = NULL;
  IconTheme *theme = NULL;
  int i;
  IconKey key;

  if (!ensure_valid_themes (self, non_blocking))
    return NULL;

  key.icon_names = (char **)icon_names;
  key.size = size;
  key.scale = scale;
  key.flags = flags;

  /* This is used in the icontheme unit test */
  if (GTK_DISPLAY_DEBUG_CHECK (self->display, ICONTHEME))
    {
      for (i = 0; icon_names[i]; i++)
        gdk_debug_message ("\tlookup name: %s", icon_names[i]);
    }

  icon = icon_cache_lookup (self, &key);
  if (icon)
    return icon;

  /* For symbolic icons, do a search in all registered themes first;
   * a theme that inherits them from a parent theme might provide
   * an alternative full-color version, but still expect the symbolic icon
   * to show up instead.
   *
   * In other words: We prefer symbolic icons in inherited themes over
   * generic icons in the theme.
   */
  for (l = self->themes; l; l = l->next)
    {
      theme = l->data;
      for (i = 0; icon_names[i] && icon_name_is_symbolic (icon_names[i], -1); i++)
        {
          icon_name = gtk_string_set_lookup (&self->icons, icon_names[i]);
          if (icon_name)
            {
              icon = theme_lookup_icon (theme, icon_name, size, scale, self->pixbuf_supports_svg);
              if (icon)
                goto out;
            }
        }
    }

  for (l = self->themes; l; l = l->next)
    {
      theme = l->data;

      for (i = 0; icon_names[i]; i++)
        {
          icon_name = gtk_string_set_lookup (&self->icons, icon_names[i]);
          if (icon_name)
            {
              icon = theme_lookup_icon (theme, icon_name, size, scale, self->pixbuf_supports_svg);
              if (icon)
                goto out;
            }
        }
    }

  theme = NULL;

  for (i = 0; icon_names[i]; i++)
    {
      unthemed_icon = g_hash_table_lookup (self->unthemed_icons, icon_names[i]);
      if (unthemed_icon)
        {
          icon = icon_paintable_new (icon_names[i], size, scale);

          /* A SVG icon, when allowed, beats out a XPM icon, but not a PNG icon */
          if (self->pixbuf_supports_svg &&
              unthemed_icon->svg_filename &&
              (!unthemed_icon->no_svg_filename ||
               suffix_from_name (unthemed_icon->no_svg_filename) < ICON_CACHE_FLAG_PNG_SUFFIX))
            icon->filename = g_strdup (unthemed_icon->svg_filename);
          else if (unthemed_icon->no_svg_filename)
            icon->filename = g_strdup (unthemed_icon->no_svg_filename);
          else
            {
              g_clear_object (&icon);
            }

          if (icon)
            {
              icon->is_svg = suffix_from_name (icon->filename) == ICON_CACHE_FLAG_SVG_SUFFIX;
              icon->is_resource = unthemed_icon->is_resource;
              goto out;
            }
        }
    }

#ifdef G_OS_WIN32
  /* Still not found an icon, check if reference to a Win32 resource */
  {
      char **resources;
      HICON hIcon = NULL;

      resources = g_strsplit (icon_names[0], ",", 0);
      if (resources[0])
        {
          wchar_t *wfile = g_utf8_to_utf16 (resources[0], -1, NULL, NULL, NULL);
          ExtractIconExW (wfile, resources[1] ? atoi (resources[1]) : 0, &hIcon, NULL, 1);
          g_free (wfile);
        }

      if (hIcon)
        {
          icon = icon_paintable_new (resources[0], size, scale);
          icon->win32_icon = gdk_win32_icon_to_pixbuf_libgtk_only (hIcon, NULL, NULL);
          DestroyIcon (hIcon);
          goto out;
        }
      g_strfreev (resources);
    }
#endif


  /* Fall back to missing icon */
  if (icon == NULL)
    {
      if (GTK_DEBUG_CHECK (ICONFALLBACK))
        {
          char *s = g_strjoinv (", ", (char **)icon_names);
          gdk_debug_message ("No icon found in %s (or fallbacks) for: %s", self->current_theme, s);
          g_free (s);
        }
      icon = icon_paintable_new ("image-missing", size, scale);
      icon->filename = g_strdup (IMAGE_MISSING_RESOURCE_PATH);
      icon->is_resource = TRUE;
    }

 out:
  g_assert (icon != NULL);

  icon->key.icon_names = g_strdupv ((char **)icon_names);
  icon->key.size = size;
  icon->key.scale = scale;
  icon->key.flags = flags;

  icon_cache_add (self, icon);

  return icon;
}

static void
icon_name_list_add_icon (GtkStrvBuilder *icons,
                         const char     *dir_suffix,
                         char           *icon_name)
{
  if (dir_suffix)
    gtk_strv_builder_append (icons, g_strconcat (icon_name, dir_suffix, NULL));
  gtk_strv_builder_append (icons, icon_name);
}

static GtkIconPaintable *
choose_icon (GtkIconTheme      *self,
             const char        *icon_names[],
             int                size,
             int                scale,
             GtkTextDirection   direction,
             GtkIconLookupFlags flags,
             gboolean           non_blocking)
{
  gboolean has_regular = FALSE, has_symbolic = FALSE;
  GtkIconPaintable *icon;
  GtkStrvBuilder new_names;
  const char *dir_suffix;
  guint i;

  switch (direction)
  {
  case GTK_TEXT_DIR_NONE:
    dir_suffix = NULL;
    break;
  case GTK_TEXT_DIR_LTR:
    dir_suffix = "-ltr";
    break;
  case GTK_TEXT_DIR_RTL:
    dir_suffix = "-rtl";
    break;
  default:
    g_assert_not_reached();
    dir_suffix = NULL;
    break;
  }

  for (i = 0; icon_names[i]; i++)
    {
      if (icon_name_is_symbolic (icon_names[i], -1))
        has_symbolic = TRUE;
      else
        has_regular = TRUE;
    }

  if ((flags & GTK_ICON_LOOKUP_FORCE_REGULAR) && has_symbolic)
    {
      gtk_strv_builder_init (&new_names);
      for (i = 0; icon_names[i]; i++)
        {
          if (icon_name_is_symbolic (icon_names[i], -1))
            icon_name_list_add_icon (&new_names, dir_suffix, g_strndup (icon_names[i], strlen (icon_names[i]) - strlen ("-symbolic")));
          else
            icon_name_list_add_icon (&new_names, dir_suffix, g_strdup (icon_names[i]));
        }
      for (i = 0; icon_names[i]; i++)
        {
          if (icon_name_is_symbolic (icon_names[i], -1))
            icon_name_list_add_icon (&new_names, dir_suffix, g_strdup (icon_names[i]));
        }

      icon = real_choose_icon (self,
                               (const char **) gtk_strv_builder_get_data (&new_names),
                               size,
                               scale,
                               flags & ~(GTK_ICON_LOOKUP_FORCE_REGULAR | GTK_ICON_LOOKUP_FORCE_SYMBOLIC),
                               non_blocking);

      gtk_strv_builder_clear (&new_names);
    }
  else if ((flags & GTK_ICON_LOOKUP_FORCE_SYMBOLIC) && has_regular)
    {
      gtk_strv_builder_init (&new_names);
      for (i = 0; icon_names[i]; i++)
        {
          if (!icon_name_is_symbolic (icon_names[i], -1))
            icon_name_list_add_icon (&new_names, dir_suffix, g_strconcat (icon_names[i], "-symbolic", NULL));
          else
            icon_name_list_add_icon (&new_names, dir_suffix, g_strdup (icon_names[i]));
        }
      for (i = 0; icon_names[i]; i++)
        {
          if (!icon_name_is_symbolic (icon_names[i], -1))
            icon_name_list_add_icon (&new_names, dir_suffix, g_strdup (icon_names[i]));
        }

      icon = real_choose_icon (self,
                               (const char **) gtk_strv_builder_get_data (&new_names),
                               size,
                               scale,
                               flags & ~(GTK_ICON_LOOKUP_FORCE_REGULAR | GTK_ICON_LOOKUP_FORCE_SYMBOLIC),
                               non_blocking);

      gtk_strv_builder_clear (&new_names);
    }
  else if (dir_suffix)
    {
      gtk_strv_builder_init (&new_names);
      for (i = 0; icon_names[i]; i++)
        {
          icon_name_list_add_icon (&new_names, dir_suffix, g_strdup (icon_names[i]));
        }

      icon = real_choose_icon (self,
                               (const char **) gtk_strv_builder_get_data (&new_names),
                               size,
                               scale,
                               flags & ~(GTK_ICON_LOOKUP_FORCE_REGULAR | GTK_ICON_LOOKUP_FORCE_SYMBOLIC),
                               non_blocking);

      gtk_strv_builder_clear (&new_names);
    }
  else
    {
      icon = real_choose_icon (self,
                               icon_names,
                               size,
                               scale,
                               flags & ~(GTK_ICON_LOOKUP_FORCE_REGULAR | GTK_ICON_LOOKUP_FORCE_SYMBOLIC),
                               non_blocking);
    }

  return icon;
}

static void
load_icon_thread (GTask        *task,
                  gpointer      source_object,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
  GtkIconPaintable *self = GTK_ICON_PAINTABLE (source_object);

  g_mutex_lock (&self->texture_lock);
  icon_ensure_texture__locked (self, TRUE);
  g_mutex_unlock (&self->texture_lock);
  g_task_return_pointer (task, NULL, NULL);
}

/**
 * gtk_icon_theme_lookup_icon:
 * @self: a `GtkIconTheme`
 * @icon_name: the name of the icon to lookup
 * @fallbacks: (nullable) (array zero-terminated=1):
 * @size: desired icon size.
 * @scale: the window scale this will be displayed on
 * @direction: text direction the icon will be displayed in
 * @flags: flags modifying the behavior of the icon lookup
 *
 * Looks up a named icon for a desired size and window scale,
 * returning a `GtkIconPaintable`.
 *
 * The icon can then be rendered by using it as a `GdkPaintable`,
 * or you can get information such as the filename and size.
 *
 * If the available @icon_name is not available and @fallbacks are
 * provided, they will be tried in order.
 *
 * If no matching icon is found, then a paintable that renders the
 * "missing icon" icon is returned. If you need to do something else
 * for missing icons you need to use [method@Gtk.IconTheme.has_icon].
 *
 * Note that you probably want to listen for icon theme changes and
 * update the icon. This is usually done by overriding the
 * GtkWidgetClass.css-changed() function.
 *
 * Returns: (transfer full): a `GtkIconPaintable` object
 *   containing the icon.
 */
GtkIconPaintable *
gtk_icon_theme_lookup_icon (GtkIconTheme       *self,
                            const char         *icon_name,
                            const char         *fallbacks[],
                            int                 size,
                            int                 scale,
                            GtkTextDirection    direction,
                            GtkIconLookupFlags  flags)
{
  GtkIconPaintable *icon;

  g_return_val_if_fail (GTK_IS_ICON_THEME (self), NULL);
  g_return_val_if_fail (icon_name != NULL, NULL);
  g_return_val_if_fail (scale >= 1, NULL);

  GTK_DISPLAY_DEBUG (self->display, ICONTHEME, "looking up icon %s for scale %d", icon_name, scale);

  gtk_icon_theme_lock (self);

  if (fallbacks)
    {
      gsize n_fallbacks = g_strv_length ((char **) fallbacks);
      const char **names = g_new (const char *, n_fallbacks + 2);

      names[0] = icon_name;
      memcpy (&names[1], fallbacks, sizeof (char *) * n_fallbacks);
      names[n_fallbacks + 1] = NULL;

      icon = choose_icon (self, names, size, scale, direction, flags, FALSE);

      g_free (names);
    }
  else
    {
      const char *names[2];

      names[0] = icon_name;
      names[1] = NULL;

      icon = choose_icon (self, names, size, scale, direction, flags, FALSE);
    }

  gtk_icon_theme_unlock (self);

  if (flags & GTK_ICON_LOOKUP_PRELOAD)
    {
      gboolean has_texture = FALSE;

      /* If we fail to get the lock it is because some other thread is
         currently loading the icon, so we need to do nothing */
      if (g_mutex_trylock (&icon->texture_lock))
        {
          has_texture = icon->texture != NULL;
          g_mutex_unlock (&icon->texture_lock);

          if (!has_texture)
            {
              GTask *task = g_task_new (icon, NULL, NULL, NULL);
              g_task_run_in_thread (task, load_icon_thread);
              g_object_unref (task);
            }
        }
    }

  return icon;
}

/* Error quark */
GQuark
gtk_icon_theme_error_quark (void)
{
  return g_quark_from_static_string ("gtk-icon-theme-error-quark");
}

void
gtk_icon_theme_lookup_symbolic_colors (GtkCssStyle *style,
                                       GdkRGBA      color_out[4])
{
  GtkCssValue *palette, *color;
  const char *names[4] = {
    [GTK_SYMBOLIC_COLOR_ERROR] = "error",
    [GTK_SYMBOLIC_COLOR_WARNING] = "warning",
    [GTK_SYMBOLIC_COLOR_SUCCESS] = "success"
  };
  const GdkRGBA *lookup;
  gsize i;

  color = style->core->color;
  palette = style->core->icon_palette;
  color_out[GTK_SYMBOLIC_COLOR_FOREGROUND] = *gtk_css_color_value_get_rgba (color);

  for (i = 1; i < 4; i++)
    {
      lookup = gtk_css_palette_value_get_color (palette, names[i]);
      if (lookup)
        color_out[i] = *lookup;
      else
        color_out[i] = color_out[GTK_SYMBOLIC_COLOR_FOREGROUND];
    }
}


/**
 * gtk_icon_theme_has_icon:
 * @self: a `GtkIconTheme`
 * @icon_name: the name of an icon
 *
 * Checks whether an icon theme includes an icon
 * for a particular name.
 *
 * Returns: %TRUE if @self includes an
 *  icon for @icon_name.
 */
gboolean
gtk_icon_theme_has_icon (GtkIconTheme *self,
                         const char   *icon_name)
{
  gboolean res = FALSE;

  g_return_val_if_fail (GTK_IS_ICON_THEME (self), FALSE);
  g_return_val_if_fail (icon_name != NULL, FALSE);

  gtk_icon_theme_lock (self);

  ensure_valid_themes (self, FALSE);

  if (gtk_string_set_lookup (&self->icons, icon_name) != NULL ||
      g_hash_table_contains (self->unthemed_icons, icon_name))
    {
      res = TRUE;
      goto out;
    }

 out:
  gtk_icon_theme_unlock (self);

  return res;
}

/**
 * gtk_icon_theme_has_gicon:
 * @self: a `GtkIconTheme`
 * @gicon: a `GIcon`
 *
 * Checks whether an icon theme includes an icon
 * for a particular `GIcon`.
 *
 * Returns: %TRUE if @self includes an icon for @gicon
 *
 * Since: 4.2
 */
gboolean
gtk_icon_theme_has_gicon (GtkIconTheme *self,
                          GIcon        *gicon)
{
  const char * const *names;
  gboolean res = FALSE;

  if (!G_IS_THEMED_ICON (gicon))
    return TRUE;

  names = g_themed_icon_get_names (G_THEMED_ICON (gicon));

  gtk_icon_theme_lock (self);

  ensure_valid_themes (self, FALSE);

  for (int i = 0; names[i]; i++)
    {
      if (gtk_string_set_lookup (&self->icons, names[i]) != NULL ||
          g_hash_table_contains (self->unthemed_icons, names[i]))
        {
          res = TRUE;
          goto out;
        }
    }

 out:
  gtk_icon_theme_unlock (self);

  return res;
}

static void
add_size (gpointer key,
          gpointer value,
          gpointer user_data)
{
  int **res_p = user_data;

  **res_p = GPOINTER_TO_INT (key);

  (*res_p)++;
}

/**
 * gtk_icon_theme_get_icon_sizes:
 * @self: a `GtkIconTheme`
 * @icon_name: the name of an icon
 *
 * Returns an array of integers describing the sizes at which
 * the icon is available without scaling.
 *
 * A size of -1 means that the icon is available in a scalable
 * format. The array is zero-terminated.
 *
 * Returns: (array zero-terminated=1) (transfer full): A newly
 *   allocated array describing the sizes at which the icon is
 *   available. The array should be freed with g_free() when it is no
 *   longer needed.
 */
int *
gtk_icon_theme_get_icon_sizes (GtkIconTheme *self,
                               const char   *icon_name)
{
  GList *l;
  int i;
  GHashTable *sizes;
  int *result, *r;
  const char *interned_icon_name;

  g_return_val_if_fail (GTK_IS_ICON_THEME (self), NULL);

  gtk_icon_theme_lock (self);

  ensure_valid_themes (self, FALSE);

  sizes = g_hash_table_new (g_direct_hash, g_direct_equal);

  interned_icon_name = gtk_string_set_lookup (&self->icons, icon_name);

  for (l = self->themes; l; l = l->next)
    {
      IconTheme *theme = l->data;

      for (i = 0; i < theme->dir_sizes->len; i++)
        {
          IconThemeDirSize *dir_size = &g_array_index (theme->dir_sizes, IconThemeDirSize, i);

          if (dir_size->type != ICON_THEME_DIR_SCALABLE && g_hash_table_lookup_extended (sizes, GINT_TO_POINTER (dir_size->size), NULL, NULL))
            continue;

          if (!g_hash_table_contains (dir_size->icon_hash, interned_icon_name))
            continue;

          if (dir_size->type == ICON_THEME_DIR_SCALABLE)
            g_hash_table_insert (sizes, GINT_TO_POINTER (-1), NULL);
          else
            g_hash_table_insert (sizes, GINT_TO_POINTER (dir_size->size), NULL);
        }
    }

  r = result = g_new0 (int, g_hash_table_size (sizes) + 1);

  g_hash_table_foreach (sizes, add_size, &r);
  g_hash_table_destroy (sizes);

  gtk_icon_theme_unlock (self);

  return result;
}

static void
add_key_to_hash (gpointer key,
                 gpointer value,
                 gpointer user_data)
{
  GHashTable *hash = user_data;

  g_hash_table_insert (hash, key, key);
}

/**
 * gtk_icon_theme_get_icon_names:
 * @self: a `GtkIconTheme`
 *
 * Lists the names of icons in the current icon theme.
 *
 * Returns: (array zero-terminated=1) (element-type utf8) (transfer full): a string array
 *   holding the names of all the icons in the theme. You must
 *   free the array using g_strfreev().
 */
char **
gtk_icon_theme_get_icon_names (GtkIconTheme *self)
{
  GHashTable *icons;
  GHashTableIter iter;
  char **names;
  char *key;
  int i;

  gtk_icon_theme_lock (self);

  ensure_valid_themes (self, FALSE);

  icons = g_hash_table_new (g_str_hash, g_str_equal);
  gtk_string_set_list (&self->icons, icons);

  g_hash_table_foreach (self->unthemed_icons, add_key_to_hash, icons);

  names = g_new (char *, g_hash_table_size (icons) + 1);

  i = 0;
  g_hash_table_iter_init (&iter, icons);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, NULL))
    names[i++] = g_strdup (key);

  names[i] = NULL;

  g_hash_table_destroy (icons);

  gtk_icon_theme_unlock (self);

  return names;
}

static gboolean
rescan_themes (GtkIconTheme *self)
{
  int stat_res;
  GStatBuf stat_buf;
  guint i;

  for (i = 0; i < self->dir_mtimes->len; i++)
    {
      const IconThemeDirMtime *dir_mtime = &g_array_index (self->dir_mtimes, IconThemeDirMtime, i);

      stat_res = g_stat (dir_mtime->dir, &stat_buf);

      /* dir mtime didn't change */
      if (stat_res == 0 && dir_mtime->exists &&
          S_ISDIR (stat_buf.st_mode) &&
          dir_mtime->mtime == stat_buf.st_mtime)
        continue;
      /* didn't exist before, and still doesn't */
      if (!dir_mtime->exists &&
          (stat_res != 0 || !S_ISDIR (stat_buf.st_mode)))
        continue;

      return TRUE;
    }

  self->last_stat_time = g_get_monotonic_time ();

  return FALSE;
}

static IconTheme *
theme_new (const char *theme_name,
           GKeyFile   *theme_file)
{
  IconTheme *theme;

  theme = g_new0 (IconTheme, 1);
  theme->name = g_strdup (theme_name);
  theme->dir_sizes = g_array_new (FALSE, FALSE, sizeof (IconThemeDirSize));
  theme->dirs = g_array_new (FALSE, FALSE, sizeof (IconThemeDir));

  theme->display_name =
    g_key_file_get_locale_string (theme_file, "Icon Theme", "Name", NULL, NULL);
  if (!theme->display_name)
    g_warning ("Theme file for %s has no name", theme_name);

  theme->comment =
    g_key_file_get_locale_string (theme_file,
                                  "Icon Theme", "Comment",
                                  NULL, NULL);
  return theme;
}

static void
theme_destroy (IconTheme *theme)
{
  gsize i;

  g_free (theme->name);
  g_free (theme->display_name);
  g_free (theme->comment);

  for (i = 0; i < theme->dir_sizes->len; i++)
    theme_dir_size_destroy (&g_array_index (theme->dir_sizes, IconThemeDirSize, i));
  g_array_free (theme->dir_sizes, TRUE);

  for (i = 0; i < theme->dirs->len; i++)
    theme_dir_destroy (&g_array_index (theme->dirs, IconThemeDir, i));
  g_array_free (theme->dirs, TRUE);

  g_free (theme);
}

static void
theme_dir_size_destroy (IconThemeDirSize *dir)
{
  if (dir->icon_hash)
    g_hash_table_destroy (dir->icon_hash);
  if (dir->icon_files)
    g_array_free (dir->icon_files, TRUE);
}

static void
theme_dir_destroy (IconThemeDir *dir)
{
  g_free (dir->path);
}

static int
theme_dir_size_difference (IconThemeDirSize *dir_size,
                           int               size,
                           int               scale)
{
  int scaled_size, scaled_dir_size;
  int min, max;

  scaled_size = size * scale;
  scaled_dir_size = dir_size->size * dir_size->scale;

  switch (dir_size->type)
    {
    case ICON_THEME_DIR_FIXED:
      return abs (scaled_size - scaled_dir_size);

    case ICON_THEME_DIR_SCALABLE:
      if (scaled_size < (dir_size->min_size * dir_size->scale))
        return (dir_size->min_size * dir_size->scale) - scaled_size;
      if (size > (dir_size->max_size * dir_size->scale))
        return scaled_size - (dir_size->max_size * dir_size->scale);
      return 0;

    case ICON_THEME_DIR_THRESHOLD:
      min = (dir_size->size - dir_size->threshold) * dir_size->scale;
      max = (dir_size->size + dir_size->threshold) * dir_size->scale;
      if (scaled_size < min)
        return min - scaled_size;
      if (scaled_size > max)
        return scaled_size - max;
      return 0;

    case ICON_THEME_DIR_UNTHEMED:
    default:
      g_assert_not_reached ();
      return 1000;
    }
}

static const char *
string_from_suffix (IconCacheFlag suffix)
{
  switch (suffix)
    {
    case ICON_CACHE_FLAG_XPM_SUFFIX:
      return ".xpm";
    case ICON_CACHE_FLAG_SVG_SUFFIX:
      return ".svg";
    case ICON_CACHE_FLAG_PNG_SUFFIX:
      return ".png";
    case ICON_CACHE_FLAG_SYMBOLIC_PNG_SUFFIX:
      return ".symbolic.png";
    case ICON_CACHE_FLAG_NONE:
    case ICON_CACHE_FLAG_HAS_ICON_FILE:
    default:
      g_assert_not_reached();
      return NULL;
    }
}

static inline IconCacheFlag
suffix_from_name (const char *name)
{
  const gsize name_len = strlen (name);

  if (name_len > 4)
    {
      if (name_len > strlen (".symbolic.png"))
        {
          if (strcmp (name + name_len - strlen (".symbolic.png"), ".symbolic.png") == 0)
            return ICON_CACHE_FLAG_SYMBOLIC_PNG_SUFFIX;
        }

      if (strcmp (name + name_len - strlen (".png"), ".png") == 0)
        return ICON_CACHE_FLAG_PNG_SUFFIX;

      if (strcmp (name + name_len - strlen (".svg"), ".svg") == 0)
        return ICON_CACHE_FLAG_SVG_SUFFIX;

      if (strcmp (name + name_len - strlen (".xpm"), ".xpm") == 0)
        return ICON_CACHE_FLAG_XPM_SUFFIX;
    }

  return ICON_CACHE_FLAG_NONE;
}

static IconCacheFlag
best_suffix (IconCacheFlag suffix,
             gboolean      allow_svg)
{
  if ((suffix & ICON_CACHE_FLAG_SYMBOLIC_PNG_SUFFIX) != 0)
    return ICON_CACHE_FLAG_SYMBOLIC_PNG_SUFFIX;
  else if ((suffix & ICON_CACHE_FLAG_PNG_SUFFIX) != 0)
    return ICON_CACHE_FLAG_PNG_SUFFIX;
  else if (allow_svg && ((suffix & ICON_CACHE_FLAG_SVG_SUFFIX) != 0))
    return ICON_CACHE_FLAG_SVG_SUFFIX;
  else if ((suffix & ICON_CACHE_FLAG_XPM_SUFFIX) != 0)
    return ICON_CACHE_FLAG_XPM_SUFFIX;
  else
    return ICON_CACHE_FLAG_NONE;
}

/* returns TRUE if dir_a is a better match */
static gboolean
compare_dir_size_matches (IconThemeDirSize *dir_a, int difference_a,
                          IconThemeDirSize *dir_b, int difference_b,
                          int requested_size,
                          int requested_scale)
{
  int diff_a;
  int diff_b;

  if (difference_a == 0)
    {
      if (difference_b != 0)
        return TRUE;

      /* a and b both exact matches */
    }
  else
    {
      /* If scaling, *always* prefer downscaling */
      if (dir_a->size >= requested_size &&
          dir_b->size < requested_size)
        return TRUE;

      if (dir_a->size < requested_size &&
          dir_b->size >= requested_size)
        return FALSE;

      /* Otherwise prefer the closest match */

      if (difference_a < difference_b)
        return TRUE;

      if (difference_a > difference_b)
        return FALSE;

      /* same pixel difference */
    }

  if (dir_a->scale == requested_scale &&
      dir_b->scale != requested_scale)
    return TRUE;

  if (dir_a->scale != requested_scale &&
      dir_b->scale == requested_scale)
    return FALSE;

  /* a and b both match the scale */

  if (dir_a->type != ICON_THEME_DIR_SCALABLE &&
      dir_b->type == ICON_THEME_DIR_SCALABLE)
    return TRUE;

  if (dir_a->type == ICON_THEME_DIR_SCALABLE &&
      dir_b->type != ICON_THEME_DIR_SCALABLE)
    return FALSE;

  /* a and b both are scalable */

  diff_a = abs (requested_size * requested_scale - dir_a->size * dir_a->scale);
  diff_b = abs (requested_size * requested_scale - dir_b->size * dir_b->scale);

  return diff_a <= diff_b;
}

static GtkIconPaintable *
theme_lookup_icon (IconTheme   *theme,
                   const char *icon_name, /* interned */
                   int          size,
                   int          scale,
                   gboolean     allow_svg)
{
  IconThemeDirSize *min_dir_size;
  IconThemeFile *min_file;
  int min_difference;
  IconCacheFlag min_suffix = ICON_CACHE_FLAG_PNG_SUFFIX;
  int i;

  min_difference = G_MAXINT;
  min_dir_size = NULL;
  min_file = NULL;

  for (i = 0; i < theme->dir_sizes->len; i++)
    {
      IconThemeDirSize *dir_size = &g_array_index (theme->dir_sizes, IconThemeDirSize, i);
      IconThemeFile *file;
      guint best_suffix;
      int difference;
      gpointer file_index;

      if (!g_hash_table_lookup_extended (dir_size->icon_hash, icon_name, NULL, &file_index))
        continue;

      file = &g_array_index (dir_size->icon_files, IconThemeFile, GPOINTER_TO_INT(file_index));


      if (allow_svg)
        best_suffix = file->best_suffix;
      else
        best_suffix = file->best_suffix_no_svg;

      if (best_suffix == ICON_CACHE_FLAG_NONE)
        continue;

      difference = theme_dir_size_difference (dir_size, size, scale);
      if (min_dir_size == NULL ||
          compare_dir_size_matches (dir_size, difference,
                                    min_dir_size, min_difference,
                                    size, scale))
        {
          min_dir_size = dir_size;
          min_file = file;
          min_suffix = best_suffix;
          min_difference = difference;
        }
    }

  if (min_dir_size)
    {
      GtkIconPaintable *icon;
      IconThemeDir *dir = &g_array_index (theme->dirs, IconThemeDir, min_file->dir_index);
      char *filename;

      icon = icon_paintable_new (icon_name, size, scale);

      filename = g_strconcat (icon_name, string_from_suffix (min_suffix), NULL);
      icon->filename = g_build_filename (dir->path, filename, NULL);
      icon->is_svg = min_suffix == ICON_CACHE_FLAG_SVG_SUFFIX;
      icon->is_resource = dir->is_resource;
      icon->is_symbolic = icon_uri_is_symbolic (filename, -1);
      g_free (filename);

      return icon;
    }

  return NULL;
}

static GHashTable *
scan_directory (GtkIconTheme  *self,
                char          *full_dir,
                GtkStringSet  *set)
{
  GDir *gdir;
  const char *name;
  GHashTable *icons = NULL;

  GTK_DISPLAY_DEBUG (self->display, ICONTHEME, "scanning directory %s", full_dir);

  gdir = g_dir_open (full_dir, 0, NULL);

  if (gdir == NULL)
    return NULL;

  while ((name = g_dir_read_name (gdir)))
    {
      const char *interned;
      IconCacheFlag suffix, hash_suffix;

      suffix = suffix_from_name (name);
      if (suffix == ICON_CACHE_FLAG_NONE)
        continue;

      strip_suffix_inline ((char *)name, suffix);
      interned = gtk_string_set_add (set, name);

      if (!icons)
        icons = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

      hash_suffix = GPOINTER_TO_INT (g_hash_table_lookup (icons, interned));
      g_hash_table_replace (icons, (char *)interned, GUINT_TO_POINTER (hash_suffix|suffix));
    }

  g_dir_close (gdir);

  return icons;
}

static GHashTable *
scan_resource_directory (GtkIconTheme  *self,
                         const char    *full_dir,
                         GtkStringSet  *set)
{
  GHashTable *icons = NULL;
  char **children;
  int i;

  GTK_DISPLAY_DEBUG (self->display, ICONTHEME, "scanning resource directory %s", full_dir);

  children = g_resources_enumerate_children (full_dir, 0, NULL);

  if (children)
    {
      for (i = 0; children[i]; i++)
        {
          char *name = children[i];
          const char *interned;
          IconCacheFlag suffix, hash_suffix;

          suffix = suffix_from_name (name);
          if (suffix == ICON_CACHE_FLAG_NONE)
            continue;

          if (!icons)
            icons = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

          strip_suffix_inline (name, suffix);
          interned = gtk_string_set_add (set, name);

          hash_suffix = GPOINTER_TO_INT (g_hash_table_lookup (icons, interned));
          g_hash_table_replace (icons, (char *)interned, GUINT_TO_POINTER (hash_suffix|suffix));
        }

      g_strfreev (children);
    }

  return icons;
}

static gboolean
theme_dir_size_equal (IconThemeDirSize *a,
                      IconThemeDirSize *b)
{
  return
    a->type == b->type &&
    a->size == b->size &&
    a->min_size == b->min_size &&
    a->max_size == b->max_size &&
    a->threshold == b->threshold &&
    a->scale == b->scale;
 }

static guint32
theme_ensure_dir_size (IconTheme *theme,
                       IconThemeDirType type,
                       int size,
                       int min_size,
                       int max_size,
                       int threshold,
                       int scale)
{
  guint32 index;
  IconThemeDirSize new = { 0 };

  new.type = type;
  new.size = size;
  new.min_size = min_size;
  new.max_size = max_size;
  new.threshold = threshold;
  new.scale = scale;

  for (index = 0; index < theme->dir_sizes->len; index++)
    {
      IconThemeDirSize *dir_size = &g_array_index (theme->dir_sizes, IconThemeDirSize, index);

      if (theme_dir_size_equal (dir_size, &new))
        return index;
    }

  new.icon_files = g_array_new (FALSE, FALSE, sizeof (IconThemeFile));
  /* The keys are interned strings, so use direct hash/equal */
  new.icon_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

  index = theme->dir_sizes->len;
  g_array_append_val (theme->dir_sizes, new);

  return index;
}

static guint32
theme_add_icon_dir (IconTheme *theme,
                    gboolean is_resource,
                    char *path /* takes ownership */)
{
  IconThemeDir new_dir = { 0 };
  guint32 dir_index;

  new_dir.is_resource = is_resource;
  new_dir.path = path;

  dir_index = theme->dirs->len;
  g_array_append_val (theme->dirs, new_dir);
  return dir_index;
}

static void
theme_add_icon_file (IconTheme *theme,
                     const char *icon_name, /* interned */
                     guint suffixes,
                     IconThemeDirSize *dir_size,
                     guint dir_index)
{
  IconThemeFile new_file = { 0 };
  guint index;

  if (g_hash_table_contains (dir_size->icon_hash, icon_name))
    return;

  new_file.dir_index = dir_index;
  new_file.best_suffix = best_suffix (suffixes, TRUE);
  new_file.best_suffix_no_svg = best_suffix (suffixes, FALSE);

  index = dir_size->icon_files->len;
  g_array_append_val (dir_size->icon_files, new_file);

  g_hash_table_insert (dir_size->icon_hash, (char *)icon_name, GINT_TO_POINTER(index));

}

/* Icon names are are already interned */
static void
theme_add_dir_with_icons (IconTheme *theme,
                          IconThemeDirSize *dir_size,
                          gboolean is_resource,
                          char *path /* takes ownership */,
                          GHashTable *icons)
{
  GHashTableIter iter;
  gpointer key, value;
  guint32 dir_index;

  dir_index = theme_add_icon_dir (theme, is_resource, path);

  g_hash_table_iter_init (&iter, icons);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const char *icon_name = key; /* interned */
      guint suffixes = GPOINTER_TO_INT(value);
      theme_add_icon_file (theme, icon_name, suffixes, dir_size, dir_index);
    }
}

static void
theme_subdir_load (GtkIconTheme *self,
                   IconTheme    *theme,
                   GKeyFile     *theme_file,
                   char         *subdir)
{
  char *type_string;
  IconThemeDirType type;
  int size;
  int min_size;
  int max_size;
  int threshold;
  GError *error = NULL;
  guint32 dir_size_index;
  IconThemeDirSize *dir_size;
  int scale;
  guint i;
  GString *str;

  size = g_key_file_get_integer (theme_file, subdir, "Size", &error);
  if (error)
    {
      g_error_free (error);
      g_warning ("Theme directory %s of theme %s has no size field\n",
                 subdir, theme->name);
      return;
    }

  type = ICON_THEME_DIR_THRESHOLD;
  type_string = g_key_file_get_string (theme_file, subdir, "Type", NULL);
  if (type_string)
    {
      if (strcmp (type_string, "Fixed") == 0)
        type = ICON_THEME_DIR_FIXED;
      else if (strcmp (type_string, "Scalable") == 0)
        type = ICON_THEME_DIR_SCALABLE;
      else if (strcmp (type_string, "Threshold") == 0)
        type = ICON_THEME_DIR_THRESHOLD;

      g_free (type_string);
    }

  if (g_key_file_has_key (theme_file, subdir, "MaxSize", NULL))
    max_size = g_key_file_get_integer (theme_file, subdir, "MaxSize", NULL);
  else
    max_size = size;

  if (g_key_file_has_key (theme_file, subdir, "MinSize", NULL))
    min_size = g_key_file_get_integer (theme_file, subdir, "MinSize", NULL);
  else
    min_size = size;

  if (g_key_file_has_key (theme_file, subdir, "Threshold", NULL))
    threshold = g_key_file_get_integer (theme_file, subdir, "Threshold", NULL);
  else
    threshold = 2;

  if (g_key_file_has_key (theme_file, subdir, "Scale", NULL))
    scale = g_key_file_get_integer (theme_file, subdir, "Scale", NULL);
  else
    scale = 1;

  dir_size_index = theme_ensure_dir_size (theme, type, size, min_size, max_size, threshold, scale);
  dir_size = &g_array_index (theme->dir_sizes, IconThemeDirSize, dir_size_index);

  str = g_string_sized_new (256);

  for (i = 0; i < self->dir_mtimes->len; i++)
    {
      IconThemeDirMtime *dir_mtime = &g_array_index (self->dir_mtimes, IconThemeDirMtime, i);

      if (!dir_mtime->exists)
        continue; /* directory doesn't exist */

      g_string_assign (str, dir_mtime->dir);
      if (str->str[str->len - 1] != '/')
        g_string_append_c (str, '/');
      g_string_append (str, subdir);

      /* First, see if we have a cache for the directory */
      if (dir_mtime->cache != NULL || g_file_test (str->str, G_FILE_TEST_IS_DIR))
        {
          GHashTable *icons = NULL;

          if (dir_mtime->cache == NULL)
            {
              /* This will return NULL if the cache doesn't exist or is outdated */
              dir_mtime->cache = gtk_icon_cache_new_for_path (dir_mtime->dir);
            }

          if (dir_mtime->cache != NULL)
            icons = gtk_icon_cache_list_icons_in_directory (dir_mtime->cache, subdir, &self->icons);
          else
            icons = scan_directory (self, str->str, &self->icons);

          if (icons)
            {
              theme_add_dir_with_icons (theme,
                                        dir_size,
                                        FALSE,
                                        g_strdup (str->str),
                                        icons);
              g_hash_table_destroy (icons);
            }
        }
    }

  if (strcmp (theme->name, FALLBACK_ICON_THEME) == 0)
    {
      int r;

      for (r = 0; self->resource_path[r]; r++)
        {
          GHashTable *icons;

          g_string_assign (str, self->resource_path[r]);
          if (str->str[str->len - 1] != '/')
            g_string_append_c (str, '/');
          g_string_append (str, subdir);
          /* Force a trailing / here, to avoid extra copies in GResource */
          if (str->str[str->len - 1] != '/')
            g_string_append_c (str, '/');

          icons = scan_resource_directory (self, str->str, &self->icons);
          if (icons)
            {
              theme_add_dir_with_icons (theme,
                                        dir_size,
                                        TRUE,
                                        g_strdup (str->str),
                                        icons);
              g_hash_table_destroy (icons);
            }
        }
    }

  g_string_free (str, TRUE);
}

/*
 * GtkIconPaintable
 */

static void icon_paintable_init (GdkPaintableInterface *iface);
static void icon_symbolic_paintable_init (GtkSymbolicPaintableInterface *iface);

enum
{
  PROP_0,
  PROP_FILE,
  PROP_ICON_NAME,
  PROP_IS_SYMBOLIC,
};

G_DEFINE_TYPE_WITH_CODE (GtkIconPaintable, gtk_icon_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                icon_paintable_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SYMBOLIC_PAINTABLE,
                                                icon_symbolic_paintable_init))

static void
gtk_icon_paintable_init (GtkIconPaintable *icon)
{
  g_mutex_init (&icon->texture_lock);
}

static GtkIconPaintable *
icon_paintable_new (const char *icon_name,
                    int desired_size,
                    int desired_scale)
{
  GtkIconPaintable *icon;

  icon = g_object_new (GTK_TYPE_ICON_PAINTABLE,
                       "icon-name", icon_name,
                       NULL);

  icon->desired_size = desired_size;
  icon->desired_scale = desired_scale;

  return icon;
}

static void
gtk_icon_paintable_finalize (GObject *object)
{
  GtkIconPaintable *icon = (GtkIconPaintable *) object;

  icon_cache_remove (icon);

  g_strfreev (icon->key.icon_names);

  g_free (icon->filename);
  g_free (icon->icon_name);

  g_clear_object (&icon->loadable);
  g_clear_object (&icon->texture);
#ifdef G_OS_WIN32
  g_clear_object (&icon->win32_icon);
#endif

  g_mutex_clear (&icon->texture_lock);

  G_OBJECT_CLASS (gtk_icon_paintable_parent_class)->finalize (object);
}

static void
gtk_icon_paintable_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkIconPaintable *icon = GTK_ICON_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_take_object (value, gtk_icon_paintable_get_file (icon));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, icon->icon_name);
      break;

    case PROP_IS_SYMBOLIC:
      g_value_set_boolean (value, icon->is_symbolic);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_icon_paintable_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkIconPaintable *icon = GTK_ICON_PAINTABLE (object);
  GFile *file;

  switch (prop_id)
    {
    case PROP_FILE:
      icon->is_resource = FALSE;
      g_clear_pointer (&icon->filename, g_free);

      file = G_FILE (g_value_get_object (value));
      if (file)
        {
          icon->is_resource = g_file_has_uri_scheme (file, "resource");
          if (icon->is_resource)
            {
              char *uri = g_file_get_uri (file);
              icon->filename = g_strdup (uri + 11); /* resource:// */
              g_free (uri);
            }
          else
            icon->filename = g_file_get_path (file);
        }
      break;

    case PROP_ICON_NAME:
      g_free (icon->icon_name);
      icon->icon_name = g_value_dup_string (value);
      break;

    case PROP_IS_SYMBOLIC:
      icon->is_symbolic = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
gtk_icon_paintable_class_init (GtkIconPaintableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_icon_paintable_get_property;
  gobject_class->set_property = gtk_icon_paintable_set_property;
  gobject_class->finalize = gtk_icon_paintable_finalize;

  /**
   * GtkIconPaintable:file: (attributes org.gtk.Property.get=gtk_icon_paintable_get_file)
   *
   * The file representing the icon, if any.
   */
  g_object_class_install_property (gobject_class, PROP_FILE,
                                   g_param_spec_object ("file", NULL, NULL,
                                                        G_TYPE_FILE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK));

  /**
   * GtkIconPaintable:icon-name: (attributes org.gtk.Property.get=gtk_icon_paintable_get_icon_name)
   *
   * The icon name that was chosen during lookup.
   */
  g_object_class_install_property (gobject_class, PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK));

  /**
   * GtkIconPaintable:is-symbolic: (attributes org.gtk.Property.get=gtk_icon_paintable_is_symbolic)
   *
   * Whether the icon is symbolic or not.
   */
  g_object_class_install_property (gobject_class, PROP_IS_SYMBOLIC,
                                   g_param_spec_boolean ("is-symbolic", NULL, NULL,
                                                        FALSE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK));
}

static GFile *
new_resource_file (const char *filename)
{
  char *escaped = g_uri_escape_string (filename,
                                       G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
  char *uri = g_strconcat ("resource://", escaped, NULL);
  GFile *file = g_file_new_for_uri (uri);

  g_free (escaped);
  g_free (uri);

  return file;
}

/**
 * gtk_icon_paintable_get_file: (attributes org.gtk.Method.get_property=file)
 * @self: a `GtkIconPaintable`
 *
 * Gets the `GFile` that was used to load the icon.
 *
 * Returns %NULL if the icon was not loaded from a file.
 *
 * Returns: (nullable) (transfer full): the `GFile` for the icon
 */
GFile *
gtk_icon_paintable_get_file (GtkIconPaintable *icon)
{
  if (icon->filename)
    {
      if (icon->is_resource)
        return new_resource_file (icon->filename);
      else
        return g_file_new_for_path (icon->filename);
    }

  return NULL;
}

/**
 * gtk_icon_paintable_get_icon_name: (attributes org.gtk.Method.get_property=icon-name)
 * @self: a `GtkIconPaintable`
 *
 * Get the icon name being used for this icon.
 *
 * When an icon looked up in the icon theme was not available, the
 * icon theme may use fallback icons - either those specified to
 * gtk_icon_theme_lookup_icon() or the always-available
 * "image-missing". The icon chosen is returned by this function.
 *
 * If the icon was created without an icon theme, this function
 * returns %NULL.
 *
 *
 * Returns: (nullable) (type filename): the themed icon-name for the
 *   icon, or %NULL if its not a themed icon.
 */
const char *
gtk_icon_paintable_get_icon_name (GtkIconPaintable *icon)
{
  g_return_val_if_fail (icon != NULL, NULL);

  return icon->icon_name;
}

/**
 * gtk_icon_paintable_is_symbolic: (attributes org.gtk.Method.get_property=is-symbolic)
 * @self: a `GtkIconPaintable`
 *
 * Checks if the icon is symbolic or not.
 *
 * This currently uses only the file name and not the file contents
 * for determining this. This behaviour may change in the future.
 *
 * Note that to render a symbolic `GtkIconPaintable` properly (with
 * recoloring), you have to set its icon name on a `GtkImage`.
 *
 * Returns: %TRUE if the icon is symbolic, %FALSE otherwise
 */
gboolean
gtk_icon_paintable_is_symbolic (GtkIconPaintable *icon)
{
  g_return_val_if_fail (GTK_IS_ICON_PAINTABLE (icon), FALSE);

  return icon->is_symbolic;
}

/* This function contains the complicated logic for deciding
 * on the size at which to load the icon and loading it at
 * that size.
 */
static void
icon_ensure_texture__locked (GtkIconPaintable *icon,
                             gboolean          in_thread)
{
  gint64 before;
  int pixel_size;
  GError *load_error = NULL;
  gboolean only_fg = FALSE;

  icon_cache_mark_used_if_cached (icon);

  if (icon->texture)
    return;

  before = GDK_PROFILER_CURRENT_TIME;

  /* This is the natural pixel size for the requested icon size + scale in this directory.
   * We precalculate this so we can use it as a rasterization size for svgs.
   */
  pixel_size = icon->desired_size * icon->desired_scale;

  /* At this point, we need to actually get the icon; either from the
   * builtin image or by loading the file
   */
#ifdef G_OS_WIN32
  if (icon->win32_icon)
    {
      icon->texture = gdk_texture_new_for_pixbuf (icon->win32_icon);
    }
  else
#endif
  if (icon->is_resource)
    {
      if (icon->is_svg)
        {
          if (icon->is_symbolic)
            icon->texture = gdk_texture_new_from_resource_symbolic (icon->filename,
                                                                    pixel_size, pixel_size,
                                                                    icon->desired_scale,
                                                                    &only_fg,
                                                                    &load_error);
          else
            icon->texture = gdk_texture_new_from_resource_at_scale (icon->filename,
                                                                    pixel_size, pixel_size,
                                                                    TRUE,
                                                                    &only_fg,
                                                                    &load_error);
        }
      else
        icon->texture = gdk_texture_new_from_resource_with_fg (icon->filename, &only_fg);
    }
  else if (icon->filename)
    {
      if (icon->is_svg)
        {
          if (icon->is_symbolic)
            icon->texture = gdk_texture_new_from_filename_symbolic (icon->filename,
                                                                    pixel_size, pixel_size,
                                                                    icon->desired_scale,
                                                                    &only_fg,
                                                                    &load_error);
          else
            {
              GFile *file = g_file_new_for_path (icon->filename);
              GInputStream *stream = G_INPUT_STREAM (g_file_read (file, NULL, &load_error));

              if (stream)
                {
                  icon->texture = gdk_texture_new_from_stream_at_scale (stream,
                                                                        pixel_size, pixel_size,
                                                                        TRUE,
                                                                        &only_fg,
                                                                        NULL,
                                                                        &load_error);
                  g_object_unref (stream);
                }

              g_object_unref (file);
            }
        }
      else
        {
          icon->texture = gdk_texture_new_from_filename_with_fg (icon->filename, &only_fg, &load_error);
        }
    }
  else
    {
      GInputStream *stream;

      g_assert (icon->loadable);

      stream = g_loadable_icon_load (icon->loadable, pixel_size, NULL, NULL, &load_error);
      if (stream)
        {
          /* SVG icons are a special case - we just immediately scale them
           * to the desired size
           */
          if (icon->is_svg)
            icon->texture = gdk_texture_new_from_stream_at_scale (stream,
                                                                  pixel_size, pixel_size,
                                                                  TRUE,
                                                                  &only_fg,
                                                                  NULL,
                                                                  &load_error);
          else
            icon->texture = gdk_texture_new_from_stream_with_fg (stream, &only_fg, NULL, &load_error);

          g_object_unref (stream);
        }
    }

  icon->only_fg = only_fg;

  if (!icon->texture)
    {
      g_warning ("Failed to load icon %s: %s", icon->filename, load_error ? load_error->message : "");
      g_clear_error (&load_error);
      icon->texture = gdk_texture_new_from_resource (IMAGE_MISSING_RESOURCE_PATH);
      icon->icon_name = g_strdup ("image-missing");
      icon->is_symbolic = FALSE;
      icon->only_fg = FALSE;
    }

  if (GDK_PROFILER_IS_RUNNING)
    {
      gint64 end = GDK_PROFILER_CURRENT_TIME;
      /* Don't report quick (< 0.5 msec) parses */
      if (end - before > 500000 || !in_thread)
        {
          gdk_profiler_add_markf (before, (end - before), in_thread ?  "Icon load (thread)" : "Icon load" ,
                                  "%s size %d@%d", icon->filename, icon->desired_size, icon->desired_scale);
        }
    }
}

static GdkTexture *
gtk_icon_paintable_ensure_texture (GtkIconPaintable *self)
{
  GdkTexture *texture = NULL;

  g_mutex_lock (&self->texture_lock);

  icon_ensure_texture__locked (self, FALSE);

  texture = self->texture;

  g_mutex_unlock (&self->texture_lock);

  g_assert (texture != NULL);

  return texture;
}

static void
init_color_matrix (graphene_matrix_t *color_matrix,
                   graphene_vec4_t   *color_offset,
                   const GdkRGBA     *foreground_color,
                   const GdkRGBA     *success_color,
                   const GdkRGBA     *warning_color,
                   const GdkRGBA     *error_color)
{
  const GdkRGBA fg_default = { 0.7450980392156863, 0.7450980392156863, 0.7450980392156863, 1.0};
  const GdkRGBA success_default = { 0.3046921492332342,0.6015716792553597, 0.023437857633325704, 1.0};
  const GdkRGBA warning_default = {0.9570458533607996, 0.47266346227206835, 0.2421911955443656, 1.0 };
  const GdkRGBA error_default = { 0.796887159533074, 0 ,0, 1.0 };
  const GdkRGBA *fg = foreground_color ? foreground_color : &fg_default;
  const GdkRGBA *sc = success_color ? success_color : &success_default;
  const GdkRGBA *wc = warning_color ? warning_color : &warning_default;
  const GdkRGBA *ec = error_color ? error_color : &error_default;

  graphene_matrix_init_from_float (color_matrix,
                                   (float[16]) {
                                     sc->red - fg->red, sc->green - fg->green, sc->blue - fg->blue, 0,
                                     wc->red - fg->red, wc->green - fg->green, wc->blue - fg->blue, 0,
                                     ec->red - fg->red, ec->green - fg->green, ec->blue - fg->blue, 0,
                                     0, 0, 0, fg->alpha
                                   });
  graphene_vec4_init (color_offset, fg->red, fg->green, fg->blue, 0);
}


static void
icon_paintable_snapshot (GdkPaintable *paintable,
                         GtkSnapshot  *snapshot,
                         double        width,
                         double        height)
{
  gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable), snapshot, width, height, NULL, 0);
}

static void
gtk_icon_paintable_snapshot_symbolic (GtkSymbolicPaintable *paintable,
                                      GtkSnapshot          *snapshot,
                                      double                width,
                                      double                height,
                                      const GdkRGBA        *colors,
                                      gsize                 n_colors)
{
  GtkIconPaintable *icon = GTK_ICON_PAINTABLE (paintable);
  GdkTexture *texture;
  int texture_width, texture_height;
  double render_width;
  double render_height;
  graphene_rect_t render_rect;

  texture = gtk_icon_paintable_ensure_texture (icon);

  texture_width = gdk_texture_get_width (texture);
  texture_height = gdk_texture_get_height (texture);

  /* Keep aspect ratio and center */
  if (texture_width >= texture_height)
    {
      render_width = width;
      render_height = height * ((double)texture_height / texture_width);
    }
  else
    {
      render_width = width * ((double)texture_width / texture_height);
      render_height = height;
    }

  graphene_rect_init (&render_rect,
                      (width - render_width) / 2,
                      (height - render_height) / 2,
                      render_width,
                      render_height);

  if (icon->is_symbolic && icon->only_fg)
    {
      g_debug ("snapshot symbolic icon using mask");
      gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_ALPHA);
      gtk_snapshot_append_texture (snapshot, texture, &render_rect);
      gtk_snapshot_pop (snapshot);
      gtk_snapshot_append_color (snapshot, &colors[0], &render_rect);
      gtk_snapshot_pop (snapshot);
    }
  else if (icon->is_symbolic)
    {
      graphene_matrix_t matrix;
      graphene_vec4_t offset;

      g_debug ("snapshot symbolic icon using color-matrix");
      init_color_matrix (&matrix, &offset,
                         &colors[0], &colors[3],
                         &colors[2], &colors[1]);

      gtk_snapshot_push_color_matrix (snapshot, &matrix, &offset);
      gtk_snapshot_append_texture (snapshot, texture, &render_rect);
      gtk_snapshot_pop (snapshot);
    }
  else
    {
      gtk_snapshot_append_texture (snapshot, texture, &render_rect);
    }
}

static GdkPaintableFlags
icon_paintable_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_SIZE | GDK_PAINTABLE_STATIC_CONTENTS;
}

static int
icon_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkIconPaintable *icon = GTK_ICON_PAINTABLE (paintable);

  return icon->desired_size;
}

static int
icon_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkIconPaintable *icon = GTK_ICON_PAINTABLE (paintable);

  return icon->desired_size;
}

static void
icon_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = icon_paintable_snapshot;
  iface->get_flags = icon_paintable_get_flags;
  iface->get_intrinsic_width = icon_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = icon_paintable_get_intrinsic_height;
}

static void
icon_symbolic_paintable_init (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = gtk_icon_paintable_snapshot_symbolic;
}

/**
 * gtk_icon_paintable_new_for_file:
 * @file: a `GFile`
 * @size: desired icon size
 * @scale: the desired scale
 *
 * Creates a `GtkIconPaintable` for a file with a given size and scale.
 *
 * The icon can then be rendered by using it as a `GdkPaintable`.
 *
 * Returns: (transfer full): a `GtkIconPaintable` containing
 *   for the icon. Unref with g_object_unref()
 */
GtkIconPaintable *
gtk_icon_paintable_new_for_file (GFile *file,
                                 int    size,
                                 int    scale)
{
  GtkIconPaintable *icon;

  icon = icon_paintable_new (NULL, size, scale);
  icon->loadable = G_LOADABLE_ICON (g_file_icon_new (file));
  icon->is_resource = g_file_has_uri_scheme (file, "resource");

  if (icon->is_resource)
    {
      char *uri;

      uri = g_file_get_uri (file);
      icon->filename = g_strdup (uri + strlen ("resource://"));
      g_free (uri);
    }
  else
    {
      icon->filename = g_file_get_path (file);
    }

  icon->is_svg = suffix_from_name (icon->filename) == ICON_CACHE_FLAG_SVG_SUFFIX;
  icon->is_symbolic = icon_uri_is_symbolic (icon->filename, -1);

  return icon;
}

/**
 * gtk_icon_theme_lookup_by_gicon:
 * @self: a `GtkIconTheme`
 * @icon: the `GIcon` to look up
 * @size: desired icon size
 * @scale: the desired scale
 * @direction: text direction the icon will be displayed in
 * @flags: flags modifying the behavior of the icon lookup
 *
 * Looks up a icon for a desired size and window scale.
 *
 * The icon can then be rendered by using it as a `GdkPaintable`,
 * or you can get information such as the filename and size.
 *
 * Returns: (transfer full): a `GtkIconPaintable` containing
 *   information about the icon. Unref with g_object_unref()
 */
GtkIconPaintable *
gtk_icon_theme_lookup_by_gicon (GtkIconTheme       *self,
                                GIcon              *gicon,
                                int                 size,
                                int                 scale,
                                GtkTextDirection    direction,
                                GtkIconLookupFlags  flags)
{
  GtkIconPaintable *paintable = NULL;

  g_return_val_if_fail (GTK_IS_ICON_THEME (self), NULL);
  g_return_val_if_fail (G_IS_ICON (gicon), NULL);
  g_return_val_if_fail (size > 0, NULL);
  g_return_val_if_fail (scale > 0, NULL);

  /* We can't render emblemed icons atm, but at least render the base */
  while (G_IS_EMBLEMED_ICON (gicon))
    gicon = g_emblemed_icon_get_icon (G_EMBLEMED_ICON (gicon));
  g_assert (gicon); /* shut up gcc -Wnull-dereference */

  if (GDK_IS_TEXTURE (gicon))
    {
      paintable = icon_paintable_new (NULL, size, scale);
      paintable->texture = g_object_ref (GDK_TEXTURE (gicon));
    }
  else if (GDK_IS_PIXBUF (gicon))
    {
      paintable = icon_paintable_new (NULL, size, scale);
      paintable->texture = gdk_texture_new_for_pixbuf (GDK_PIXBUF (gicon));
    }
  else if (G_IS_FILE_ICON (gicon))
    {
      GFile *file = g_file_icon_get_file (G_FILE_ICON (gicon));

      paintable = gtk_icon_paintable_new_for_file (file, size, scale);
    }
  else if (G_IS_LOADABLE_ICON (gicon))
    {
      paintable = icon_paintable_new (NULL, size, scale);
      paintable->loadable = G_LOADABLE_ICON (g_object_ref (gicon));
      paintable->is_svg = FALSE;
    }
  else if (G_IS_THEMED_ICON (gicon))
    {
      const char **names;

      names = (const char **) g_themed_icon_get_names (G_THEMED_ICON (gicon));
      paintable = gtk_icon_theme_lookup_icon (self, names[0], &names[1], size, scale, direction, flags);
    }
  else
    {
      g_debug ("Unhandled GIcon type %s", G_OBJECT_TYPE_NAME (gicon));
      paintable = icon_paintable_new ("image-missing", size, scale);
      paintable->filename = g_strdup (IMAGE_MISSING_RESOURCE_PATH);
      paintable->is_resource = TRUE;
    }

  return paintable;
}

/**
 * gtk_icon_theme_get_display:
 * @self: a `GtkIconTheme`
 *
 * Returns the display that the `GtkIconTheme` object was
 * created for.
 *
 * Returns: (nullable) (transfer none): the display of @icon_theme
 */
GdkDisplay *
gtk_icon_theme_get_display (GtkIconTheme *self)
{
  g_return_val_if_fail (GTK_IS_ICON_THEME (self), NULL);

  return self->display;
}

