/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2003 Benjamin Otte <in7y118@public.uni-hamburg.de>
 * Copyright (C) 2008-2009 Tim-Philipp Müller <tim centricular net>
 *
 * gstinfo.c: debugging functions
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gstinfo
 * @title: GstInfo
 * @short_description: Debugging and logging facilities
 * @see_also: #gst-running for command line parameters
 * and environment variables that affect the debugging output.
 *
 * GStreamer's debugging subsystem is an easy way to get information about what
 * the application is doing.  It is not meant for programming errors. Use GLib
 * methods (g_warning and friends) for that.
 *
 * The debugging subsystem works only after GStreamer has been initialized
 * - for example by calling gst_init().
 *
 * The debugging subsystem is used to log informational messages while the
 * application runs.  Each messages has some properties attached to it. Among
 * these properties are the debugging category, the severity (called "level"
 * here) and an optional #GObject it belongs to. Each of these messages is sent
 * to all registered debugging handlers, which then handle the messages.
 * GStreamer attaches a default handler on startup, which outputs requested
 * messages to stderr.
 *
 * Messages are output by using shortcut macros like #GST_DEBUG,
 * #GST_CAT_ERROR_OBJECT or similar. These all expand to calling gst_debug_log()
 * with the right parameters.
 * The only thing a developer will probably want to do is define his own
 * categories. This is easily done with 3 lines. At the top of your code,
 * declare
 * the variables and set the default category.
 * |[<!-- language="C" -->
 *   GST_DEBUG_CATEGORY_STATIC (my_category);  // define category (statically)
 *   #define GST_CAT_DEFAULT my_category       // set as default
 * ]|
 * After that you only need to initialize the category.
 * |[<!-- language="C" -->
 *   GST_DEBUG_CATEGORY_INIT (my_category, "my category",
 *                            0, "This is my very own");
 * ]|
 * Initialization must be done before the category is used first.
 * Plugins do this
 * in their plugin_init function, libraries and applications should do that
 * during their initialization.
 *
 * The whole debugging subsystem can be disabled at build time with passing the
 * --disable-gst-debug switch to configure. If this is done, every function,
 * macro and even structs described in this file evaluate to default values or
 * nothing at all.
 * So don't take addresses of these functions or use other tricks.
 * If you must do that for some reason, there is still an option.
 * If the debugging
 * subsystem was compiled out, GST_DISABLE_GST_DEBUG is defined in
 * <gst/gst.h>,
 * so you can check that before doing your trick.
 * Disabling the debugging subsystem will give you a slight (read: unnoticeable)
 * speed increase and will reduce the size of your compiled code. The GStreamer
 * library itself becomes around 10% smaller.
 *
 * Please note that there are naming conventions for the names of debugging
 * categories. These are explained at GST_DEBUG_CATEGORY_INIT().
 */

#define GST_INFO_C
#include "gst_private.h"
#include "gstinfo.h"

#undef gst_debug_remove_log_function
#undef gst_debug_add_log_function

#ifndef GST_DISABLE_GST_DEBUG
#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>
#endif
#include <stdio.h>              /* fprintf */
#include <glib/gstdio.h>
#include <errno.h>
#include <string.h>             /* G_VA_COPY */

#include "gst_private.h"
#include "gstutils.h"
#include "gstquark.h"
#include "gstsegment.h"
#include "gstvalue.h"
#include "gstcapsfeatures.h"

#endif /* GST_DISABLE_GST_DEBUG */

#include <glib/gprintf.h>       /* g_sprintf */

/* our own printf implementation with custom extensions to %p for caps etc. */
#include "printf/printf.h"
#include "printf/printf-extension.h"

static char *gst_info_printf_pointer_extension_func (const char *format,
    void *ptr);

#ifdef HAVE_UNISTD_H
#  include <unistd.h>           /* getpid on UNIX */
#endif

#ifdef G_OS_WIN32
#  define WIN32_LEAN_AND_MEAN   /* prevents from including too many things */
#  include <windows.h>          /* GetStdHandle, windows console */
#  include <processthreadsapi.h>        /* GetCurrentProcessId */
/* getpid() is not allowed in case of UWP, use GetCurrentProcessId() instead
 * which can be used on both desktop and UWP */
#endif

/* use glib's abstraction once it's landed
 * https://gitlab.gnome.org/GNOME/glib/-/merge_requests/2475 */
#ifdef G_OS_WIN32
static inline DWORD
_gst_getpid (void)
{
  return GetCurrentProcessId ();
}
#else
static inline pid_t
_gst_getpid (void)
{
  return getpid ();
}
#endif

#ifdef HAVE_UNWIND
/* No need for remote debugging so turn on the 'local only' optimizations in
 * libunwind */
#define UNW_LOCAL_ONLY

#include <libunwind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#ifdef HAVE_DW
#include <elfutils/libdwfl.h>
static Dwfl *_global_dwfl = NULL;
static GMutex _dwfl_mutex;

#define GST_DWFL_LOCK() g_mutex_lock(&_dwfl_mutex);
#define GST_DWFL_UNLOCK() g_mutex_unlock(&_dwfl_mutex);

static Dwfl *
get_global_dwfl (void)
{
  if (g_once_init_enter_pointer (&_global_dwfl)) {
    static Dwfl_Callbacks callbacks = {
      .find_elf = dwfl_linux_proc_find_elf,
      .find_debuginfo = dwfl_standard_find_debuginfo,
    };
    Dwfl *_dwfl = dwfl_begin (&callbacks);
    g_mutex_init (&_dwfl_mutex);
    g_once_init_leave_pointer (&_global_dwfl, _dwfl);
  }

  return _global_dwfl;
}

#endif /* HAVE_DW */
#endif /* HAVE_UNWIND */

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#define BT_BUF_SIZE 100
#endif /* HAVE_BACKTRACE */

#ifdef HAVE_DBGHELP
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <gmodule.h>
#endif /* HAVE_DBGHELP */

#ifdef G_OS_WIN32
/* We take a lock in order to
 * 1) keep colors and content together for a single line
 * 2) serialise gst_print*() and gst_printerr*() with each other and the debug
 *    log to keep the debug log colouring from interfering with those and
 *    to prevent broken output on the windows terminal.
 * Maybe there is a better way but for now this will do the right
 * thing. */
G_LOCK_DEFINE_STATIC (win_print_mutex);
#endif

extern gboolean gst_is_initialized (void);

/* we want these symbols exported even if debug is disabled, to maintain
 * ABI compatibility. Unless GST_REMOVE_DISABLED is defined. */
#if !defined(GST_DISABLE_GST_DEBUG) || !defined(GST_REMOVE_DISABLED)

/* disabled by default, as soon as some threshold is set > NONE,
 * it becomes enabled. */
gboolean _gst_debug_enabled = FALSE;
GstDebugLevel _gst_debug_min = GST_LEVEL_NONE;

GstDebugCategory *GST_CAT_DEFAULT = NULL;

GstDebugCategory *GST_CAT_GST_INIT = NULL;
GstDebugCategory *GST_CAT_MEMORY = NULL;
GstDebugCategory *GST_CAT_PARENTAGE = NULL;
GstDebugCategory *GST_CAT_STATES = NULL;
GstDebugCategory *GST_CAT_SCHEDULING = NULL;

GstDebugCategory *GST_CAT_BUFFER = NULL;
GstDebugCategory *GST_CAT_BUFFER_LIST = NULL;
GstDebugCategory *GST_CAT_BUS = NULL;
GstDebugCategory *GST_CAT_CAPS = NULL;
GstDebugCategory *GST_CAT_CLOCK = NULL;
GstDebugCategory *GST_CAT_ELEMENT_PADS = NULL;
GstDebugCategory *GST_CAT_PADS = NULL;
GstDebugCategory *GST_CAT_PERFORMANCE = NULL;
GstDebugCategory *GST_CAT_PIPELINE = NULL;
GstDebugCategory *GST_CAT_PLUGIN_LOADING = NULL;
GstDebugCategory *GST_CAT_PLUGIN_INFO = NULL;
GstDebugCategory *GST_CAT_PROPERTIES = NULL;
GstDebugCategory *GST_CAT_NEGOTIATION = NULL;
GstDebugCategory *GST_CAT_REFCOUNTING = NULL;
GstDebugCategory *GST_CAT_ERROR_SYSTEM = NULL;
GstDebugCategory *GST_CAT_EVENT = NULL;
GstDebugCategory *GST_CAT_MESSAGE = NULL;
GstDebugCategory *GST_CAT_PARAMS = NULL;
GstDebugCategory *GST_CAT_CALL_TRACE = NULL;
GstDebugCategory *GST_CAT_SIGNAL = NULL;
GstDebugCategory *GST_CAT_PROBE = NULL;
GstDebugCategory *GST_CAT_REGISTRY = NULL;
GstDebugCategory *GST_CAT_QOS = NULL;
GstDebugCategory *_priv_GST_CAT_POLL = NULL;
GstDebugCategory *GST_CAT_META = NULL;
GstDebugCategory *GST_CAT_LOCKING = NULL;
GstDebugCategory *GST_CAT_CONTEXT = NULL;
GstDebugCategory *_priv_GST_CAT_PROTECTION = NULL;


#endif /* !defined(GST_DISABLE_GST_DEBUG) || !defined(GST_REMOVE_DISABLED) */

#ifndef GST_DISABLE_GST_DEBUG

/* underscore is to prevent conflict with GST_CAT_DEBUG define */
GST_DEBUG_CATEGORY_STATIC (_GST_CAT_DEBUG);

#if 0
#if defined __sgi__
#include <rld_interface.h>
typedef struct DL_INFO
{
  const char *dli_fname;
  void *dli_fbase;
  const char *dli_sname;
  void *dli_saddr;
  int dli_version;
  int dli_reserved1;
  long dli_reserved[4];
}
Dl_info;

#define _RLD_DLADDR             14
int dladdr (void *address, Dl_info * dl);

int
dladdr (void *address, Dl_info * dl)
{
  void *v;

  v = _rld_new_interface (_RLD_DLADDR, address, dl);
  return (int) v;
}
#endif /* __sgi__ */
#endif

static void gst_debug_reset_threshold (gpointer category, gpointer unused);
static void gst_debug_reset_all_thresholds (void);

struct _GstDebugMessage
{
  gchar *message;
  const gchar *format;
  va_list arguments;

  /* The emitter of the message (can be NULL) */
  GObject *object;

  /* Provider of the message. Can be user-provided, or generated dynamically
   * from object */
  gchar *object_id;

  /* Whether object_id was dynamically allocated and should be freed */
  gboolean free_object_id;

  /* heap-allocated write area for short names */
  gchar tmp_id[32];
};

/* list of all name/level pairs from --gst-debug and GST_DEBUG */
static GMutex __level_name_mutex;
static GSList *__level_name = NULL;
typedef struct
{
  GPatternSpec *pat;
  GstDebugLevel level;
}
LevelNameEntry;

static void clear_level_names (void);

/* list of all categories */
static GMutex __cat_mutex;
static GSList *__categories = NULL;

static GstDebugCategory *_gst_debug_get_category_locked (const gchar * name);


/* all registered debug handlers */
typedef struct
{
  GstLogFunction func;
  gpointer user_data;
  GDestroyNotify notify;
}
LogFuncEntry;
static GMutex __log_func_mutex;
static GSList *__log_functions = NULL;

/* whether to add the default log function in gst_init() */
static gboolean add_default_log_func = TRUE;

#define PRETTY_TAGS_DEFAULT  TRUE
static gboolean pretty_tags = PRETTY_TAGS_DEFAULT;

static gint G_GNUC_MAY_ALIAS __default_level = GST_LEVEL_DEFAULT;
static gint G_GNUC_MAY_ALIAS __use_color = GST_DEBUG_COLOR_MODE_ON;

static gchar *
_replace_pattern_in_gst_debug_file_name (gchar * name, const char *token,
    guint val)
{
  gchar *token_start;
  if ((token_start = strstr (name, token))) {
    gsize token_len = strlen (token);
    gchar *name_prefix = name;
    gchar *name_suffix = token_start + token_len;
    token_start[0] = '\0';
    name = g_strdup_printf ("%s%u%s", name_prefix, val, name_suffix);
    g_free (name_prefix);
  }
  return name;
}

static gchar *
_priv_gst_debug_file_name (const gchar * env)
{
  gchar *name;

  name = g_strdup (env);
  name = _replace_pattern_in_gst_debug_file_name (name, "%p", _gst_getpid ());
  name = _replace_pattern_in_gst_debug_file_name (name, "%r", g_random_int ());

  return name;
}

/* Initialize the debugging system */
void
_priv_gst_debug_init (void)
{
  const gchar *env;
  FILE *log_file;

  if (add_default_log_func) {
    env = g_getenv ("GST_DEBUG_FILE");
    if (env != NULL && *env != '\0') {
      if (strcmp (env, "-") == 0) {
        log_file = stdout;
      } else {
        gchar *name = _priv_gst_debug_file_name (env);
        log_file = g_fopen (name, "w");
        g_free (name);
        if (log_file == NULL) {
          g_printerr ("Could not open log file '%s' for writing: %s\n", env,
              g_strerror (errno));
          log_file = stderr;
        }
      }
    } else {
      log_file = stderr;
    }

    gst_debug_add_log_function (gst_debug_log_default, log_file, NULL);
  }

  __gst_printf_pointer_extension_set_func
      (gst_info_printf_pointer_extension_func);

  /* do NOT use a single debug function before this line has been run */
  GST_CAT_DEFAULT = _gst_debug_category_new ("default",
      GST_DEBUG_UNDERLINE, NULL);
  _GST_CAT_DEBUG = _gst_debug_category_new ("GST_DEBUG",
      GST_DEBUG_BOLD | GST_DEBUG_FG_YELLOW, "debugging subsystem");

  /* FIXME: add descriptions here */
  GST_CAT_GST_INIT = _gst_debug_category_new ("GST_INIT",
      GST_DEBUG_BOLD | GST_DEBUG_FG_RED, NULL);
  GST_CAT_MEMORY = _gst_debug_category_new ("GST_MEMORY",
      GST_DEBUG_BOLD | GST_DEBUG_FG_BLUE, "memory");
  GST_CAT_PARENTAGE = _gst_debug_category_new ("GST_PARENTAGE",
      GST_DEBUG_BOLD | GST_DEBUG_FG_WHITE | GST_DEBUG_BG_RED, NULL);
  GST_CAT_STATES = _gst_debug_category_new ("GST_STATES",
      GST_DEBUG_BOLD | GST_DEBUG_FG_RED, NULL);
  GST_CAT_SCHEDULING = _gst_debug_category_new ("GST_SCHEDULING",
      GST_DEBUG_BOLD | GST_DEBUG_FG_MAGENTA, NULL);
  GST_CAT_BUFFER = _gst_debug_category_new ("GST_BUFFER",
      GST_DEBUG_BOLD | GST_DEBUG_BG_GREEN, NULL);
  GST_CAT_BUFFER_LIST = _gst_debug_category_new ("GST_BUFFER_LIST",
      GST_DEBUG_BOLD | GST_DEBUG_BG_GREEN, NULL);
  GST_CAT_BUS = _gst_debug_category_new ("GST_BUS", GST_DEBUG_BG_YELLOW, NULL);
  GST_CAT_CAPS = _gst_debug_category_new ("GST_CAPS",
      GST_DEBUG_BOLD | GST_DEBUG_FG_BLUE, NULL);
  GST_CAT_CLOCK = _gst_debug_category_new ("GST_CLOCK",
      GST_DEBUG_BOLD | GST_DEBUG_FG_YELLOW, NULL);
  GST_CAT_ELEMENT_PADS = _gst_debug_category_new ("GST_ELEMENT_PADS",
      GST_DEBUG_BOLD | GST_DEBUG_FG_WHITE | GST_DEBUG_BG_RED, NULL);
  GST_CAT_PADS = _gst_debug_category_new ("GST_PADS",
      GST_DEBUG_BOLD | GST_DEBUG_FG_RED | GST_DEBUG_BG_BLUE, NULL);
  GST_CAT_PERFORMANCE = _gst_debug_category_new ("GST_PERFORMANCE",
      GST_DEBUG_BOLD | GST_DEBUG_FG_WHITE | GST_DEBUG_BG_RED, NULL);
  GST_CAT_PIPELINE = _gst_debug_category_new ("GST_PIPELINE",
      GST_DEBUG_BOLD | GST_DEBUG_FG_WHITE | GST_DEBUG_BG_RED, NULL);
  GST_CAT_PLUGIN_LOADING = _gst_debug_category_new ("GST_PLUGIN_LOADING",
      GST_DEBUG_BOLD | GST_DEBUG_FG_CYAN, NULL);
  GST_CAT_PLUGIN_INFO = _gst_debug_category_new ("GST_PLUGIN_INFO",
      GST_DEBUG_BOLD | GST_DEBUG_FG_CYAN, NULL);
  GST_CAT_PROPERTIES = _gst_debug_category_new ("GST_PROPERTIES",
      GST_DEBUG_BOLD | GST_DEBUG_FG_WHITE | GST_DEBUG_BG_BLUE, NULL);
  GST_CAT_NEGOTIATION = _gst_debug_category_new ("GST_NEGOTIATION",
      GST_DEBUG_BOLD | GST_DEBUG_FG_BLUE, NULL);
  GST_CAT_REFCOUNTING = _gst_debug_category_new ("GST_REFCOUNTING",
      GST_DEBUG_BOLD | GST_DEBUG_FG_RED | GST_DEBUG_BG_BLUE, NULL);
  GST_CAT_ERROR_SYSTEM = _gst_debug_category_new ("GST_ERROR_SYSTEM",
      GST_DEBUG_BOLD | GST_DEBUG_FG_RED | GST_DEBUG_BG_WHITE, NULL);

  GST_CAT_EVENT = _gst_debug_category_new ("GST_EVENT",
      GST_DEBUG_BOLD | GST_DEBUG_FG_BLUE, NULL);
  GST_CAT_MESSAGE = _gst_debug_category_new ("GST_MESSAGE",
      GST_DEBUG_BOLD | GST_DEBUG_FG_WHITE | GST_DEBUG_BG_RED, NULL);
  GST_CAT_PARAMS = _gst_debug_category_new ("GST_PARAMS",
      GST_DEBUG_BOLD | GST_DEBUG_FG_BLACK | GST_DEBUG_BG_YELLOW, NULL);
  GST_CAT_CALL_TRACE = _gst_debug_category_new ("GST_CALL_TRACE",
      GST_DEBUG_BOLD, NULL);
  GST_CAT_SIGNAL = _gst_debug_category_new ("GST_SIGNAL",
      GST_DEBUG_BOLD | GST_DEBUG_FG_WHITE | GST_DEBUG_BG_RED, NULL);
  GST_CAT_PROBE = _gst_debug_category_new ("GST_PROBE",
      GST_DEBUG_BOLD | GST_DEBUG_FG_GREEN, "pad probes");
  GST_CAT_REGISTRY = _gst_debug_category_new ("GST_REGISTRY", 0, "registry");
  GST_CAT_QOS = _gst_debug_category_new ("GST_QOS", 0, "QoS");
  _priv_GST_CAT_POLL = _gst_debug_category_new ("GST_POLL", 0, "poll");
  GST_CAT_META = _gst_debug_category_new ("GST_META", 0, "meta");
  GST_CAT_LOCKING = _gst_debug_category_new ("GST_LOCKING", 0, "locking");
  GST_CAT_CONTEXT = _gst_debug_category_new ("GST_CONTEXT", 0, NULL);
  _priv_GST_CAT_PROTECTION =
      _gst_debug_category_new ("GST_PROTECTION", 0, "protection");

  env = g_getenv ("GST_DEBUG_OPTIONS");
  if (env != NULL) {
    if (strstr (env, "full_tags") || strstr (env, "full-tags"))
      pretty_tags = FALSE;
    else if (strstr (env, "pretty_tags") || strstr (env, "pretty-tags"))
      pretty_tags = TRUE;
  }

  if (g_getenv ("GST_DEBUG_NO_COLOR") != NULL)
    gst_debug_set_color_mode (GST_DEBUG_COLOR_MODE_OFF);
  env = g_getenv ("GST_DEBUG_COLOR_MODE");
  if (env)
    gst_debug_set_color_mode_from_string (env);

  env = g_getenv ("GST_DEBUG");
  if (env)
    gst_debug_set_threshold_from_string (env, FALSE);
}

/* we can't do this further above, because we initialize the GST_CAT_DEFAULT struct */
#define GST_CAT_DEFAULT _GST_CAT_DEBUG

/**
 * gst_debug_log:
 * @category: category to log
 * @level: level of the message is in
 * @file: the file that emitted the message, usually the __FILE__ identifier
 * @function: the function that emitted the message
 * @line: the line from that the message was emitted, usually __LINE__
 * @object: (transfer none) (allow-none): the object this message relates to,
 *     or %NULL if none
 * @format: a printf style format string
 * @...: optional arguments for the format
 *
 * Logs the given message using the currently registered debugging handlers.
 */
void
gst_debug_log (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    GObject * object, const gchar * format, ...)
{
  va_list var_args;

  va_start (var_args, format);
  gst_debug_log_valist (category, level, file, function, line, object, format,
      var_args);
  va_end (var_args);
}

/**
 * gst_debug_log_id:
 * @category: category to log
 * @level: level of the message is in
 * @file: the file that emitted the message, usually the __FILE__ identifier
 * @function: the function that emitted the message
 * @line: the line from that the message was emitted, usually __LINE__
 * @id: (transfer none) (allow-none): the identifier of the object this message
 *    relates to, or %NULL if none.
 * @format: a printf style format string
 * @...: optional arguments for the format
 *
 * Logs the given message using the currently registered debugging handlers.
 *
 * Since: 1.22
 */
void
gst_debug_log_id (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    const gchar * id, const gchar * format, ...)
{
  va_list var_args;

  va_start (var_args, format);
  gst_debug_log_id_valist (category, level, file, function, line, id, format,
      var_args);
  va_end (var_args);
}

/* based on g_basename(), which we can't use because it was deprecated */
static inline const gchar *
gst_path_basename (const gchar * file_name)
{
  register const gchar *base;

  base = strrchr (file_name, G_DIR_SEPARATOR);

  {
    const gchar *q = strrchr (file_name, '/');
    if (base == NULL || (q != NULL && q > base))
      base = q;
  }

  if (base)
    return base + 1;

  if (g_ascii_isalpha (file_name[0]) && file_name[1] == ':')
    return file_name + 2;

  return file_name;
}

static void
gst_debug_log_full_valist (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    GObject * object, const gchar * id, const gchar * format, va_list args)
{
  GstDebugMessage message;
  LogFuncEntry *entry;
  GSList *handler;

  g_return_if_fail (category != NULL);

  if (level > gst_debug_category_get_threshold (category))
    return;

  g_return_if_fail (file != NULL);
  g_return_if_fail (function != NULL);
  g_return_if_fail (format != NULL);

#ifdef GST_ENABLE_EXTRA_CHECKS
  g_return_if_fail (id != NULL || object == NULL || G_IS_OBJECT (object));
#endif

  message.message = NULL;
  message.format = format;
  message.object = object;
  message.object_id = (gchar *) id;
  message.free_object_id = FALSE;

  G_VA_COPY (message.arguments, args);

  handler = __log_functions;
  while (handler) {
    entry = handler->data;
    handler = g_slist_next (handler);
    entry->func (category, level, file, function, line, object, &message,
        entry->user_data);
  }
  g_free (message.message);
  if (message.free_object_id)
    g_free (message.object_id);
  va_end (message.arguments);
}

/**
 * gst_debug_log_valist:
 * @category: category to log
 * @level: level of the message is in
 * @file: the file that emitted the message, usually the __FILE__ identifier
 * @function: the function that emitted the message
 * @line: the line from that the message was emitted, usually __LINE__
 * @object: (transfer none) (allow-none): the object this message relates to,
 *     or %NULL if none
 * @format: a printf style format string
 * @args: optional arguments for the format
 *
 * Logs the given message using the currently registered debugging handlers.
 */
void
gst_debug_log_valist (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    GObject * object, const gchar * format, va_list args)
{
#ifdef GST_ENABLE_EXTRA_CHECKS
  g_warn_if_fail (object == NULL || G_IS_OBJECT (object));
#endif

  gst_debug_log_full_valist (category, level, file, function, line, object,
      NULL, format, args);
}

/**
 * gst_debug_log_id_valist:
 * @category: category to log
 * @level: level of the message is in
 * @file: the file that emitted the message, usually the __FILE__ identifier
 * @function: the function that emitted the message
 * @line: the line from that the message was emitted, usually __LINE__
 * @id: (transfer none) (allow-none): the identifier of the object this message
 *    relates to or %NULL if none.
 * @format: a printf style format string
 * @args: optional arguments for the format
 *
 * Logs the given message using the currently registered debugging handlers.
 *
 * Since: 1.22
 */
void
gst_debug_log_id_valist (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    const gchar * id, const gchar * format, va_list args)
{
  gst_debug_log_full_valist (category, level, file, function, line, NULL, id,
      format, args);
}

static void
gst_debug_log_literal_full (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    GObject * object, const gchar * id, const gchar * message_string)
{
  GstDebugMessage message;
  LogFuncEntry *entry;
  GSList *handler;

  g_return_if_fail (category != NULL);

  if (level > gst_debug_category_get_threshold (category))
    return;

#ifdef GST_ENABLE_EXTRA_CHECKS
  g_return_if_fail (id != NULL || object == NULL || G_IS_OBJECT (object));
#endif

  g_return_if_fail (file != NULL);
  g_return_if_fail (function != NULL);
  g_return_if_fail (message_string != NULL);

  message.message = (gchar *) message_string;
  message.object = object;
  message.object_id = (gchar *) id;
  message.free_object_id = FALSE;

  handler = __log_functions;
  while (handler) {
    entry = handler->data;
    handler = g_slist_next (handler);
    entry->func (category, level, file, function, line, object, &message,
        entry->user_data);
  }

  if (message.free_object_id)
    g_free (message.object_id);
}

/**
 * gst_debug_log_literal:
 * @category: category to log
 * @level: level of the message is in
 * @file: the file that emitted the message, usually the __FILE__ identifier
 * @function: the function that emitted the message
 * @line: the line from that the message was emitted, usually __LINE__
 * @object: (transfer none) (allow-none): the object this message relates to,
 *     or %NULL if none
 * @message_string: a message string
 *
 * Logs the given message using the currently registered debugging handlers.
 *
 * Since: 1.20
 */
void
gst_debug_log_literal (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    GObject * object, const gchar * message_string)
{
#ifdef GST_ENABLE_EXTRA_CHECKS
  g_warn_if_fail (object == NULL || G_IS_OBJECT (object));
#endif

  gst_debug_log_literal_full (category, level, file, function, line, object,
      NULL, message_string);
}

/**
 * gst_debug_log_id_literal:
 * @category: category to log
 * @level: level of the message is in
 * @file: the file that emitted the message, usually the __FILE__ identifier
 * @function: the function that emitted the message
 * @line: the line from that the message was emitted, usually __LINE__
 * @id: (transfer none) (allow-none): the identifier of the object this message relates to
 *    or %NULL if none
 * @message_string: a message string
 *
 * Logs the given message using the currently registered debugging handlers.
 *
 * Since: 1.22
 */
void
gst_debug_log_id_literal (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    const gchar * id, const gchar * message_string)
{
  gst_debug_log_literal_full (category, level, file, function, line, NULL, id,
      message_string);
}

/**
 * gst_debug_message_get:
 * @message: a debug message
 *
 * Gets the string representation of a #GstDebugMessage. This function is used
 * in debug handlers to extract the message.
 *
 * Returns: (nullable): the string representation of a #GstDebugMessage.
 */
const gchar *
gst_debug_message_get (GstDebugMessage * message)
{
  if (message->message == NULL) {
    int len;

    len = __gst_vasprintf (&message->message, message->format,
        message->arguments);

    if (len < 0)
      message->message = NULL;
  }
  return message->message;
}

/* Return the pad name. Will use the provided 32 byte write_area if it fits
 * within */
static inline gchar *
_heap_pad_name (GstPad * pad, gchar * write_area, gboolean * allocated)
{
  GstObject *parent = GST_OBJECT_PARENT (pad);
  const gchar *parentname =
      parent ? GST_STR_NULL (GST_OBJECT_NAME (parent)) : "''";
  const gchar *padname = GST_STR_NULL (GST_OBJECT_NAME (pad));

  /* 1 byte for ':' and 1 for terminating '\0' */
  if (strlen (parentname) + strlen (padname) + 2 <= 32) {
    g_sprintf (write_area, "%s:%s", parentname, padname);
    *allocated = FALSE;
    return write_area;
  }
  *allocated = TRUE;
  return g_strdup_printf ("%s:%s", parentname, padname);
}

/* Returns the object id. Allocated is set to TRUE if a memory allocation
 * happened and the returned value should be freed */
static gchar *
_get_object_id (GObject * object, gboolean * allocated, gchar * write_area)
{
  gchar *object_id;

  if (GST_IS_PAD (object) && GST_OBJECT_NAME (object)) {
    object_id = _heap_pad_name (GST_PAD_CAST (object), write_area, allocated);
  } else if (GST_IS_OBJECT (object)
      && GST_OBJECT_NAME (object)) {
    object_id = GST_OBJECT_NAME (object);
    *allocated = FALSE;
  } else if (G_IS_OBJECT (object)) {
    object_id = g_strdup_printf ("%s@%p", G_OBJECT_TYPE_NAME (object), object);
    *allocated = TRUE;
  } else {
    object_id = g_strdup_printf ("%p", object);
    *allocated = TRUE;
  }

  return object_id;
}

/**
 * gst_debug_message_get_id:
 * @message: a debug message
 *
 * Get the id of the object that emitted this message. This function is used in
 * debug handlers. Can be empty.
 *
 * Since: 1.22
 *
 * Returns: (nullable): The emitter of a #GstDebugMessage.
 */
const gchar *
gst_debug_message_get_id (GstDebugMessage * message)
{
  if (!message->object_id && message->object) {
    /* Dynamically generate the object id */
    /* Note : we don't use gst_debug_print_object since we only accept a subset
     * and can avoid duplicating if needed */
    message->object_id =
        _get_object_id (message->object, &message->free_object_id,
        (gchar *) & message->tmp_id);
  }
  return message->object_id;
}

#define MAX_BUFFER_DUMP_STRING_LEN  100

/* structure_to_pretty_string:
 * @str: a serialized #GstStructure
 *
 * If the serialized structure contains large buffers such as images the hex
 * representation of those buffers will be shortened so that the string remains
 * readable.
 *
 * Returns: the filtered string
 */
static gchar *
prettify_structure_string (gchar * str)
{
  gchar *pos = str, *end;

  while ((pos = strstr (pos, "(buffer)"))) {
    guint count = 0;

    pos += strlen ("(buffer)");
    for (end = pos; *end != '\0' && *end != ';' && *end != ' '; ++end)
      ++count;
    if (count > MAX_BUFFER_DUMP_STRING_LEN) {
      memcpy (pos + MAX_BUFFER_DUMP_STRING_LEN - 6, "..", 2);
      memcpy (pos + MAX_BUFFER_DUMP_STRING_LEN - 4, pos + count - 4, 4);
      memmove (pos + MAX_BUFFER_DUMP_STRING_LEN, pos + count,
          strlen (pos + count) + 1);
      pos += MAX_BUFFER_DUMP_STRING_LEN;
    }
  }

  return str;
}

static inline gchar *
gst_info_structure_to_string (const GstStructure * s)
{
  if (G_LIKELY (s)) {
    gchar *str = gst_structure_to_string (s);
    if (G_UNLIKELY (pretty_tags && s->name == GST_QUARK (TAGLIST)))
      return prettify_structure_string (str);
    else
      return str;
  }
  return NULL;
}

static inline gchar *
gst_info_describe_buffer (GstBuffer * buffer)
{
  const gchar *offset_str = "none";
  const gchar *offset_end_str = "none";
  gchar offset_buf[32], offset_end_buf[32];

  if (GST_BUFFER_OFFSET_IS_VALID (buffer)) {
    g_snprintf (offset_buf, sizeof (offset_buf), "%" G_GUINT64_FORMAT,
        GST_BUFFER_OFFSET (buffer));
    offset_str = offset_buf;
  }
  if (GST_BUFFER_OFFSET_END_IS_VALID (buffer)) {
    g_snprintf (offset_end_buf, sizeof (offset_end_buf), "%" G_GUINT64_FORMAT,
        GST_BUFFER_OFFSET_END (buffer));
    offset_end_str = offset_end_buf;
  }

  return g_strdup_printf ("buffer: %p, pts %" GST_TIME_FORMAT ", dts %"
      GST_TIME_FORMAT ", dur %" GST_TIME_FORMAT ", size %" G_GSIZE_FORMAT
      ", offset %s, offset_end %s, flags 0x%x", buffer,
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer)),
      GST_TIME_ARGS (GST_BUFFER_DTS (buffer)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)),
      gst_buffer_get_size (buffer), offset_str, offset_end_str,
      GST_BUFFER_FLAGS (buffer));
}

static inline gchar *
gst_info_describe_buffer_list (GstBufferList * list)
{
  GstClockTime pts = GST_CLOCK_TIME_NONE;
  GstClockTime dts = GST_CLOCK_TIME_NONE;
  gsize total_size = 0;
  guint n, i;

  n = gst_buffer_list_length (list);
  for (i = 0; i < n; ++i) {
    GstBuffer *buf = gst_buffer_list_get (list, i);

    if (i == 0) {
      pts = GST_BUFFER_PTS (buf);
      dts = GST_BUFFER_DTS (buf);
    }

    total_size += gst_buffer_get_size (buf);
  }

  return g_strdup_printf ("bufferlist: %p, %u buffers, pts %" GST_TIME_FORMAT
      ", dts %" GST_TIME_FORMAT ", size %" G_GSIZE_FORMAT, list, n,
      GST_TIME_ARGS (pts), GST_TIME_ARGS (dts), total_size);
}

static inline gchar *
gst_info_describe_event (GstEvent * event)
{
  gchar *s, *ret;

  s = gst_info_structure_to_string (gst_event_get_structure (event));
  ret = g_strdup_printf ("%s event: %p, time %" GST_TIME_FORMAT
      ", seq-num %d, %s", GST_EVENT_TYPE_NAME (event), event,
      GST_TIME_ARGS (GST_EVENT_TIMESTAMP (event)), GST_EVENT_SEQNUM (event),
      (s ? s : "(NULL)"));
  g_free (s);
  return ret;
}

static inline gchar *
gst_info_describe_message (GstMessage * message)
{
  gchar *s, *ret;

  s = gst_info_structure_to_string (gst_message_get_structure (message));
  ret = g_strdup_printf ("%s message: %p, time %" GST_TIME_FORMAT
      ", seq-num %d, element '%s', %s", GST_MESSAGE_TYPE_NAME (message),
      message, GST_TIME_ARGS (GST_MESSAGE_TIMESTAMP (message)),
      GST_MESSAGE_SEQNUM (message),
      ((message->src) ? GST_ELEMENT_NAME (message->src) : "(NULL)"),
      (s ? s : "(NULL)"));
  g_free (s);
  return ret;
}

static inline gchar *
gst_info_describe_query (GstQuery * query)
{
  gchar *s, *ret;

  s = gst_info_structure_to_string (gst_query_get_structure (query));
  ret = g_strdup_printf ("%s query: %p, %s", GST_QUERY_TYPE_NAME (query),
      query, (s ? s : "(NULL)"));
  g_free (s);
  return ret;
}

static inline gchar *
gst_info_describe_stream (GstStream * stream)
{
  gchar *ret, *caps_str = NULL, *tags_str = NULL;
  GstCaps *caps;
  GstTagList *tags;

  caps = gst_stream_get_caps (stream);
  if (caps) {
    caps_str = gst_caps_to_string (caps);
    gst_caps_unref (caps);
  }

  tags = gst_stream_get_tags (stream);
  if (tags) {
    tags_str = gst_tag_list_to_string (tags);
    gst_tag_list_unref (tags);
  }

  ret =
      g_strdup_printf ("stream %s %p, ID %s, flags 0x%x, caps [%s], tags [%s]",
      gst_stream_type_get_name (gst_stream_get_stream_type (stream)), stream,
      gst_stream_get_stream_id (stream), gst_stream_get_stream_flags (stream),
      caps_str ? caps_str : "", tags_str ? tags_str : "");

  g_free (caps_str);
  g_free (tags_str);

  return ret;
}

static inline gchar *
gst_info_describe_stream_collection (GstStreamCollection * collection)
{
  gchar *ret;
  GString *streams_str;
  guint i;

  streams_str = g_string_new ("<");
  for (i = 0; i < gst_stream_collection_get_size (collection); i++) {
    GstStream *stream = gst_stream_collection_get_stream (collection, i);
    gchar *s;

    s = gst_info_describe_stream (stream);
    g_string_append_printf (streams_str, " %s,", s);
    g_free (s);
  }
  g_string_append (streams_str, " >");

  ret = g_strdup_printf ("collection %p (%d streams) %s", collection,
      gst_stream_collection_get_size (collection), streams_str->str);

  g_string_free (streams_str, TRUE);
  return ret;
}

static gchar *
gst_debug_print_object (gpointer ptr)
{
  GObject *object = (GObject *) ptr;

#ifdef unused
  /* This is a cute trick to detect unmapped memory, but is unportable,
   * slow, screws around with madvise, and not actually that useful. */
  {
    int ret;

    ret = madvise ((void *) ((unsigned long) ptr & (~0xfff)), 4096, 0);
    if (ret == -1 && errno == ENOMEM) {
      buffer = g_strdup_printf ("%p (unmapped memory)", ptr);
    }
  }
#endif

  /* nicely printed object */
  if (object == NULL) {
    return g_strdup ("(NULL)");
  }
  if (GST_IS_CAPS (ptr)) {
    return gst_caps_to_string ((const GstCaps *) ptr);
  }
  if (GST_IS_STRUCTURE (ptr)) {
    return gst_info_structure_to_string ((const GstStructure *) ptr);
  }
  if (*(GType *) ptr == GST_TYPE_CAPS_FEATURES) {
    return gst_caps_features_to_string ((const GstCapsFeatures *) ptr);
  }
  if (GST_IS_TAG_LIST (ptr)) {
    gchar *str = gst_tag_list_to_string ((GstTagList *) ptr);
    if (G_UNLIKELY (pretty_tags))
      return prettify_structure_string (str);
    else
      return str;
  }
  if (*(GType *) ptr == GST_TYPE_DATE_TIME) {
    return __gst_date_time_serialize ((GstDateTime *) ptr, TRUE);
  }
  if (GST_IS_BUFFER (ptr)) {
    return gst_info_describe_buffer (GST_BUFFER_CAST (ptr));
  }
  if (GST_IS_BUFFER_LIST (ptr)) {
    return gst_info_describe_buffer_list (GST_BUFFER_LIST_CAST (ptr));
  }
#ifdef USE_POISONING
  if (*(guint32 *) ptr == 0xffffffff) {
    return g_strdup_printf ("<poisoned@%p>", ptr);
  }
#endif
  if (GST_IS_MESSAGE (object)) {
    return gst_info_describe_message (GST_MESSAGE_CAST (object));
  }
  if (GST_IS_QUERY (object)) {
    return gst_info_describe_query (GST_QUERY_CAST (object));
  }
  if (GST_IS_EVENT (object)) {
    return gst_info_describe_event (GST_EVENT_CAST (object));
  }
  if (GST_IS_CONTEXT (object)) {
    GstContext *context = GST_CONTEXT_CAST (object);
    gchar *s, *ret;
    const gchar *type;
    const GstStructure *structure;

    type = gst_context_get_context_type (context);
    structure = gst_context_get_structure (context);

    s = gst_info_structure_to_string (structure);

    ret = g_strdup_printf ("context '%s'='%s'", type, s);
    g_free (s);
    return ret;
  }
  if (GST_IS_STREAM (object)) {
    return gst_info_describe_stream (GST_STREAM_CAST (object));
  }
  if (GST_IS_STREAM_COLLECTION (object)) {
    return
        gst_info_describe_stream_collection (GST_STREAM_COLLECTION_CAST
        (object));
  }
  if (GST_IS_PAD (object) && GST_OBJECT_NAME (object)) {
    return g_strdup_printf ("<%s:%s>", GST_DEBUG_PAD_NAME (object));
  }
  if (GST_IS_OBJECT (object) && GST_OBJECT_NAME (object)) {
    return g_strdup_printf ("<%s>", GST_OBJECT_NAME (object));
  }
  if (G_IS_OBJECT (object)) {
    return g_strdup_printf ("<%s@%p>", G_OBJECT_TYPE_NAME (object), object);
  }

  return g_strdup_printf ("%p", ptr);
}

static gchar *
gst_debug_print_segment (gpointer ptr)
{
  GstSegment *segment = (GstSegment *) ptr;

  /* nicely printed segment */
  if (segment == NULL) {
    return g_strdup ("(NULL)");
  }

  switch (segment->format) {
    case GST_FORMAT_UNDEFINED:{
      return g_strdup_printf ("UNDEFINED segment");
    }
    case GST_FORMAT_TIME:{
      return g_strdup_printf ("time segment start=%" GST_TIME_FORMAT
          ", offset=%" GST_TIME_FORMAT ", stop=%" GST_TIME_FORMAT
          ", rate=%f, applied_rate=%f" ", flags=0x%02x, time=%" GST_TIME_FORMAT
          ", base=%" GST_TIME_FORMAT ", position %" GST_TIME_FORMAT
          ", duration %" GST_TIME_FORMAT, GST_TIME_ARGS (segment->start),
          GST_TIME_ARGS (segment->offset), GST_TIME_ARGS (segment->stop),
          segment->rate, segment->applied_rate, (guint) segment->flags,
          GST_TIME_ARGS (segment->time), GST_TIME_ARGS (segment->base),
          GST_TIME_ARGS (segment->position), GST_TIME_ARGS (segment->duration));
    }
    default:{
      const gchar *format_name;

      format_name = gst_format_get_name (segment->format);
      if (G_UNLIKELY (format_name == NULL))
        format_name = "(UNKNOWN FORMAT)";
      return g_strdup_printf ("%s segment start=%" G_GINT64_FORMAT
          ", offset=%" G_GINT64_FORMAT ", stop=%" G_GINT64_FORMAT
          ", rate=%f, applied_rate=%f" ", flags=0x%02x, time=%" G_GINT64_FORMAT
          ", base=%" G_GINT64_FORMAT ", position %" G_GINT64_FORMAT
          ", duration %" G_GINT64_FORMAT, format_name, segment->start,
          segment->offset, segment->stop, segment->rate, segment->applied_rate,
          (guint) segment->flags, segment->time, segment->base,
          segment->position, segment->duration);
    }
  }
}

static char *
gst_info_printf_pointer_extension_func (const char *format, void *ptr)
{
  char *s = NULL;

  if (format[0] == 'p' && format[1] == '\a') {
    switch (format[2]) {
      case 'A':                /* GST_PTR_FORMAT     */
        s = gst_debug_print_object (ptr);
        break;
      case 'B':                /* GST_SEGMENT_FORMAT */
        s = gst_debug_print_segment (ptr);
        break;
      case 'T':                /* GST_TIMEP_FORMAT */
        if (ptr)
          s = g_strdup_printf ("%" GST_TIME_FORMAT,
              GST_TIME_ARGS (*(GstClockTime *) ptr));
        break;
      case 'S':                /* GST_STIMEP_FORMAT */
        if (ptr)
          s = g_strdup_printf ("%" GST_STIME_FORMAT,
              GST_STIME_ARGS (*(gint64 *) ptr));
        break;
      case 'a':                /* GST_WRAPPED_PTR_FORMAT */
        s = priv_gst_string_take_and_wrap (gst_debug_print_object (ptr));
        break;
      default:
        /* must have been compiled against a newer version with an extension
         * we don't known about yet - just ignore and fallback to %p below */
        break;
    }
  }
  if (s == NULL)
    s = g_strdup_printf ("%p", ptr);

  return s;
}

/* Allocation-less generator of color escape code. Provide a 20 byte write
 * area */
static void
_construct_term_color (guint colorinfo, gchar write_area[20])
{
  guint offset;

  memcpy (write_area, "\033[00", 4);
  offset = 4;

  if (colorinfo & GST_DEBUG_BOLD) {
    memcpy (write_area + offset, ";01", 3);
    offset += 3;
  }
  if (colorinfo & GST_DEBUG_UNDERLINE) {
    memcpy (write_area + offset, ";04", 3);
    offset += 3;
  }
  if (colorinfo & GST_DEBUG_FG_MASK) {
    memcpy (write_area + offset, ";3", 2);
    write_area[offset + 2] = '0' + (colorinfo & GST_DEBUG_FG_MASK);
    offset += 3;
  }
  if (colorinfo & GST_DEBUG_BG_MASK) {
    memcpy (write_area + offset, ";4", 2);
    write_area[offset + 2] = '0' + ((colorinfo & GST_DEBUG_BG_MASK) >> 4);
    offset += 3;
  }
  strncpy (write_area + offset, "m", 2);
}

/**
 * gst_debug_construct_term_color:
 * @colorinfo: the color info
 *
 * Constructs a string that can be used for getting the desired color in color
 * terminals.
 * You need to free the string after use.
 *
 * Returns: (transfer full) (type gchar*): a string containing the color
 *     definition
 */
gchar *
gst_debug_construct_term_color (guint colorinfo)
{
  gchar tmp_color[20];

  _construct_term_color (colorinfo, tmp_color);

  return g_strdup (tmp_color);
}

/**
 * gst_debug_construct_win_color:
 * @colorinfo: the color info
 *
 * Constructs an integer that can be used for getting the desired color in
 * windows' terminals (cmd.exe). As there is no mean to underline, we simply
 * ignore this attribute.
 *
 * This function returns 0 on non-windows machines.
 *
 * Returns: an integer containing the color definition
 */
gint
gst_debug_construct_win_color (guint colorinfo)
{
  gint color = 0;
#ifdef G_OS_WIN32
  static const guchar ansi_to_win_fg[8] = {
    0,                          /* black   */
    FOREGROUND_RED,             /* red     */
    FOREGROUND_GREEN,           /* green   */
    FOREGROUND_RED | FOREGROUND_GREEN,  /* yellow  */
    FOREGROUND_BLUE,            /* blue    */
    FOREGROUND_RED | FOREGROUND_BLUE,   /* magenta */
    FOREGROUND_GREEN | FOREGROUND_BLUE, /* cyan    */
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE /* white   */
  };
  static const guchar ansi_to_win_bg[8] = {
    0,
    BACKGROUND_RED,
    BACKGROUND_GREEN,
    BACKGROUND_RED | BACKGROUND_GREEN,
    BACKGROUND_BLUE,
    BACKGROUND_RED | BACKGROUND_BLUE,
    BACKGROUND_GREEN | FOREGROUND_BLUE,
    BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE
  };

  /* we draw black as white, as cmd.exe can only have black bg */
  if ((colorinfo & (GST_DEBUG_FG_MASK | GST_DEBUG_BG_MASK)) == 0) {
    color = ansi_to_win_fg[7];
  }
  if (colorinfo & GST_DEBUG_UNDERLINE) {
    color |= BACKGROUND_INTENSITY;
  }
  if (colorinfo & GST_DEBUG_BOLD) {
    color |= FOREGROUND_INTENSITY;
  }
  if (colorinfo & GST_DEBUG_FG_MASK) {
    color |= ansi_to_win_fg[colorinfo & GST_DEBUG_FG_MASK];
  }
  if (colorinfo & GST_DEBUG_BG_MASK) {
    color |= ansi_to_win_bg[(colorinfo & GST_DEBUG_BG_MASK) >> 4];
  }
#endif
  return color;
}

/* width of %p varies depending on actual value of pointer, which can make
 * output unevenly aligned if multiple threads are involved, hence the %14p
 * (should really be %18p, but %14p seems a good compromise between too many
 * white spaces and likely unalignment on my system) */
#if defined (GLIB_SIZEOF_VOID_P) && GLIB_SIZEOF_VOID_P == 8
#define PTR_FMT "%14p"
#else
#define PTR_FMT "%10p"
#endif
#ifdef G_OS_WIN32
#define PID_FMT "%5lu"
#else
#define PID_FMT "%5d"
#endif
#define CAT_FMT "%20s %s:%d:%s:%s"
#define NOCOLOR_PRINT_FMT " "PID_FMT" "PTR_FMT" %s "CAT_FMT" %s\n"
#define CAT_FMT_ID "%20s %s:%d:%s:<%s>"
#define NOCOLOR_PRINT_FMT_ID " "PID_FMT" "PTR_FMT" %s "CAT_FMT_ID" %s\n"

#ifdef G_OS_WIN32
static const guchar levelcolormap_w32[GST_LEVEL_COUNT] = {
  /* GST_LEVEL_NONE */
  FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
  /* GST_LEVEL_ERROR */
  FOREGROUND_RED | FOREGROUND_INTENSITY,
  /* GST_LEVEL_WARNING */
  FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
  /* GST_LEVEL_INFO */
  FOREGROUND_GREEN | FOREGROUND_INTENSITY,
  /* GST_LEVEL_DEBUG */
  FOREGROUND_GREEN | FOREGROUND_BLUE,
  /* GST_LEVEL_LOG */
  FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
  /* GST_LEVEL_FIXME */
  FOREGROUND_RED | FOREGROUND_GREEN,
  /* GST_LEVEL_TRACE */
  FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
  /* placeholder for log level 8 */
  0,
  /* GST_LEVEL_MEMDUMP */
  FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
};

static const guchar available_colors[] = {
  FOREGROUND_RED, FOREGROUND_GREEN, FOREGROUND_RED | FOREGROUND_GREEN,
  FOREGROUND_BLUE, FOREGROUND_RED | FOREGROUND_BLUE,
  FOREGROUND_GREEN | FOREGROUND_BLUE,
};
#endif /* G_OS_WIN32 */
static const gchar *levelcolormap[GST_LEVEL_COUNT] = {
  "\033[37m",                   /* GST_LEVEL_NONE */
  "\033[31;01m",                /* GST_LEVEL_ERROR */
  "\033[33;01m",                /* GST_LEVEL_WARNING */
  "\033[32;01m",                /* GST_LEVEL_INFO */
  "\033[36m",                   /* GST_LEVEL_DEBUG */
  "\033[37m",                   /* GST_LEVEL_LOG */
  "\033[33;01m",                /* GST_LEVEL_FIXME */
  "\033[37m",                   /* GST_LEVEL_TRACE */
  "\033[37m",                   /* placeholder for log level 8 */
  "\033[37m"                    /* GST_LEVEL_MEMDUMP */
};

static void
_gst_debug_log_preamble (GstDebugMessage * message, const gchar ** file,
    const gchar ** message_str, const gchar ** object_id,
    GstClockTime * elapsed)
{
  gchar c;

  /* Get message string first because printing it might call into our custom
   * printf format extension mechanism which in turn might log something, e.g.
   * from inside gst_structure_to_string() when something can't be serialised.
   * This means we either need to do this outside of any critical section or
   * use a recursive lock instead. As we always need the message string in all
   * code paths, we might just as well get it here first thing and outside of
   * the win_print_mutex critical section. */
  *message_str = gst_debug_message_get (message);

  /* __FILE__ might be a file name or an absolute path or a
   * relative path, irrespective of the exact compiler used,
   * in which case we want to shorten it to the filename for
   * readability. */
  c = (*file)[0];
  if (c == '.' || c == '/' || c == '\\' || (c != '\0' && (*file)[1] == ':')) {
    *file = gst_path_basename (*file);
  }

  *object_id = gst_debug_message_get_id (message);
  *elapsed = GST_CLOCK_DIFF (_priv_gst_start_time, gst_util_get_timestamp ());
}

/**
 * gst_debug_log_get_line:
 * @category: category to log
 * @level: level of the message
 * @file: the file that emitted the message, usually the __FILE__ identifier
 * @function: the function that emitted the message
 * @line: the line from that the message was emitted, usually __LINE__
 * @object: (transfer none) (allow-none): the object this message relates to,
 *     or %NULL if none
 * @message: the actual message
 *
 * Returns the string representation for the specified debug log message
 * formatted in the same way as gst_debug_log_default() (the default handler),
 * without color. The purpose is to make it easy for custom log output
 * handlers to get a log output that is identical to what the default handler
 * would write out.
 *
 * Since: 1.18
 */
gchar *
gst_debug_log_get_line (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    GObject * object, GstDebugMessage * message)
{
  GstClockTime elapsed;
  gchar *ret;
  const gchar *message_str, *object_id;

  _gst_debug_log_preamble (message, &file, &message_str, &object_id, &elapsed);

  if (object_id)
    ret = g_strdup_printf ("%" GST_TIME_FORMAT NOCOLOR_PRINT_FMT_ID,
        GST_TIME_ARGS (elapsed), _gst_getpid (), g_thread_self (),
        gst_debug_level_get_name (level), gst_debug_category_get_name
        (category), file, line, function, object_id, message_str);
  else
    ret = g_strdup_printf ("%" GST_TIME_FORMAT NOCOLOR_PRINT_FMT,
        GST_TIME_ARGS (elapsed), _gst_getpid (), g_thread_self (),
        gst_debug_level_get_name (level), gst_debug_category_get_name
        (category), file, line, function, "", message_str);

  return ret;
}

#ifdef G_OS_WIN32
static void
_gst_debug_fprintf (FILE * file, const gchar * format, ...)
{
  va_list args;
  gchar *str = NULL;
  gint length;

  va_start (args, format);
  length = gst_info_vasprintf (&str, format, args);
  va_end (args);

  if (length == 0 || !str)
    return;

  /* Even if it's valid UTF-8 string, console might print broken string
   * depending on codepage and the content of the given string.
   * Fortunately, g_print* family will take care of the Windows' codepage
   * specific behavior.
   */
  if (file == stderr) {
    g_printerr ("%s", str);
  } else if (file == stdout) {
    g_print ("%s", str);
  } else {
    /* We are writing to file. Text editors/viewers should be able to
     * decode valid UTF-8 string regardless of codepage setting */
    fwrite (str, 1, length, file);

    /* FIXME: fflush here might be redundant if setvbuf works as expected */
    fflush (file);
  }

  g_free (str);
}
#endif

/**
 * gst_debug_log_default:
 * @category: category to log
 * @level: level of the message
 * @file: the file that emitted the message, usually the __FILE__ identifier
 * @function: the function that emitted the message
 * @line: the line from that the message was emitted, usually __LINE__
 * @message: the actual message
 * @object: (transfer none) (allow-none): the object this message relates to,
 *     or %NULL if none
 * @user_data: the FILE* to log to
 *
 * The default logging handler used by GStreamer. Logging functions get called
 * whenever a macro like GST_DEBUG or similar is used. By default this function
 * is setup to output the message and additional info to stderr (or the log file
 * specified via the GST_DEBUG_FILE environment variable) as received via
 * @user_data.
 *
 * You can add other handlers by using gst_debug_add_log_function().
 * And you can remove this handler by calling
 * gst_debug_remove_log_function(gst_debug_log_default);
 */
void
gst_debug_log_default (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    GObject * object, GstDebugMessage * message, gpointer user_data)
{
  gint pid;
  GstClockTime elapsed;
  const gchar *object_id;
  GstDebugColorMode color_mode;
  const gchar *message_str;
  FILE *log_file = user_data ? user_data : stderr;
#ifdef G_OS_WIN32
#define FPRINTF_DEBUG _gst_debug_fprintf
/* _gst_debug_fprintf will do fflush if it's required */
#define FFLUSH_DEBUG(f) ((void)(f))
#else
#define FPRINTF_DEBUG fprintf
#define FFLUSH_DEBUG(f) G_STMT_START { \
    fflush (f); \
  } G_STMT_END
#endif

#ifdef GST_ENABLE_EXTRA_CHECKS
  g_warn_if_fail (object == NULL || G_IS_OBJECT (object));
#endif

  _gst_debug_log_preamble (message, &file, &message_str, &object_id, &elapsed);

  pid = _gst_getpid ();
  color_mode = gst_debug_get_color_mode ();

  if (color_mode != GST_DEBUG_COLOR_MODE_OFF) {
#ifdef G_OS_WIN32
    G_LOCK (win_print_mutex);
    if (color_mode == GST_DEBUG_COLOR_MODE_UNIX) {
#endif
      /* colors, non-windows */
      gchar color[20];
      const gchar *clear;
      gchar pidcolor[10];
      const gchar *levelcolor;

      _construct_term_color (gst_debug_category_get_color (category), color);
      clear = "\033[00m";
      g_sprintf (pidcolor, "\033[%02dm", pid % 6 + 31);
      levelcolor = levelcolormap[level];

      if (object_id) {
#define PRINT_FMT_ID " %s"PID_FMT"%s "PTR_FMT" %s%s%s %s"CAT_FMT_ID"%s %s\n"
        FPRINTF_DEBUG (log_file, "%" GST_TIME_FORMAT PRINT_FMT_ID,
            GST_TIME_ARGS (elapsed), pidcolor, pid, clear, g_thread_self (),
            levelcolor, gst_debug_level_get_name (level), clear, color,
            gst_debug_category_get_name (category), file, line, function,
            object_id, clear, message_str);
      } else {
#define PRINT_FMT " %s"PID_FMT"%s "PTR_FMT" %s%s%s %s"CAT_FMT"%s %s\n"
        FPRINTF_DEBUG (log_file, "%" GST_TIME_FORMAT PRINT_FMT,
            GST_TIME_ARGS (elapsed), pidcolor, pid, clear, g_thread_self (),
            levelcolor, gst_debug_level_get_name (level), clear, color,
            gst_debug_category_get_name (category), file, line, function, "",
            clear, message_str);
      }
      FFLUSH_DEBUG (log_file);
#undef PRINT_FMT
#undef PRINT_FMT_ID
#ifdef G_OS_WIN32
    } else {
      /* colors, windows. */
      const gint clear = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
#define SET_COLOR(c) G_STMT_START { \
  if (log_file == stderr) \
    SetConsoleTextAttribute (GetStdHandle (STD_ERROR_HANDLE), (c)); \
  } G_STMT_END
      /* timestamp */
      FPRINTF_DEBUG (log_file, "%" GST_TIME_FORMAT " ",
          GST_TIME_ARGS (elapsed));
      /* pid */
      SET_COLOR (available_colors[pid % G_N_ELEMENTS (available_colors)]);
      FPRINTF_DEBUG (log_file, PID_FMT, pid);
      /* thread */
      SET_COLOR (clear);
      FPRINTF_DEBUG (log_file, " " PTR_FMT " ", g_thread_self ());
      /* level */
      SET_COLOR (levelcolormap_w32[level]);
      FPRINTF_DEBUG (log_file, "%s ", gst_debug_level_get_name (level));
      /* category */
      SET_COLOR (gst_debug_construct_win_color (gst_debug_category_get_color
              (category)));
      if (object_id) {
        FPRINTF_DEBUG (log_file, CAT_FMT_ID,
            gst_debug_category_get_name (category), file, line, function,
            object_id);
      } else {
        FPRINTF_DEBUG (log_file, CAT_FMT,
            gst_debug_category_get_name (category), file, line, function, "");
      }
      /* message */
      SET_COLOR (clear);
      FPRINTF_DEBUG (log_file, " %s\n", message_str);
    }
    G_UNLOCK (win_print_mutex);
#endif
  } else {
    /* no color, all platforms */
    if (object_id) {
      FPRINTF_DEBUG (log_file, "%" GST_TIME_FORMAT NOCOLOR_PRINT_FMT_ID,
          GST_TIME_ARGS (elapsed), pid, g_thread_self (),
          gst_debug_level_get_name (level),
          gst_debug_category_get_name (category), file, line, function,
          object_id, message_str);
    } else {
      FPRINTF_DEBUG (log_file, "%" GST_TIME_FORMAT NOCOLOR_PRINT_FMT,
          GST_TIME_ARGS (elapsed), pid, g_thread_self (),
          gst_debug_level_get_name (level),
          gst_debug_category_get_name (category), file, line, function, "",
          message_str);
    }
    FFLUSH_DEBUG (log_file);
  }
}

/**
 * gst_debug_level_get_name:
 * @level: the level to get the name for
 *
 * Get the string representation of a debugging level
 *
 * Returns: the name
 */
const gchar *
gst_debug_level_get_name (GstDebugLevel level)
{
  switch (level) {
    case GST_LEVEL_NONE:
      return "";
    case GST_LEVEL_ERROR:
      return "ERROR  ";
    case GST_LEVEL_WARNING:
      return "WARN   ";
    case GST_LEVEL_INFO:
      return "INFO   ";
    case GST_LEVEL_DEBUG:
      return "DEBUG  ";
    case GST_LEVEL_LOG:
      return "LOG    ";
    case GST_LEVEL_FIXME:
      return "FIXME  ";
    case GST_LEVEL_TRACE:
      return "TRACE  ";
    case GST_LEVEL_MEMDUMP:
      return "MEMDUMP";
    default:
      g_warning ("invalid level specified for gst_debug_level_get_name");
      return "";
  }
}

/**
 * gst_debug_add_log_function:
 * @func: the function to use
 * @user_data: user data
 * @notify: called when @user_data is not used anymore
 *
 * Adds the logging function to the list of logging functions.
 * Be sure to use #G_GNUC_NO_INSTRUMENT on that function, it is needed.
 */
void
gst_debug_add_log_function (GstLogFunction func, gpointer user_data,
    GDestroyNotify notify)
{
  LogFuncEntry *entry;
  GSList *list;

  if (func == NULL)
    func = gst_debug_log_default;

  entry = g_new (LogFuncEntry, 1);
  entry->func = func;
  entry->user_data = user_data;
  entry->notify = notify;
  /* FIXME: we leak the old list here - other threads might access it right now
   * in gst_debug_logv. Another solution is to lock the mutex in gst_debug_logv,
   * but that is waaay costly.
   * It'd probably be clever to use some kind of RCU here, but I don't know
   * anything about that.
   */
  g_mutex_lock (&__log_func_mutex);
  list = g_slist_copy (__log_functions);
  __log_functions = g_slist_prepend (list, entry);
  g_mutex_unlock (&__log_func_mutex);

  if (gst_is_initialized ())
    GST_DEBUG ("prepended log function %p (user data %p) to log functions",
        func, user_data);
}

static gint
gst_debug_compare_log_function_by_func (gconstpointer entry, gconstpointer func)
{
  gpointer entryfunc = (gpointer) (((LogFuncEntry *) entry)->func);

  return (entryfunc < func) ? -1 : (entryfunc > func) ? 1 : 0;
}

static gint
gst_debug_compare_log_function_by_data (gconstpointer entry, gconstpointer data)
{
  gpointer entrydata = ((LogFuncEntry *) entry)->user_data;

  return (entrydata < data) ? -1 : (entrydata > data) ? 1 : 0;
}

static guint
gst_debug_remove_with_compare_func (GCompareFunc func, gpointer data)
{
  GSList *found;
  GSList *new, *cleanup = NULL;
  guint removals = 0;

  g_mutex_lock (&__log_func_mutex);
  new = __log_functions;
  cleanup = NULL;
  while ((found = g_slist_find_custom (new, data, func))) {
    if (new == __log_functions) {
      /* make a copy when we have the first hit, so that we modify the copy and
       * make that the new list later */
      new = g_slist_copy (new);
      continue;
    }
    cleanup = g_slist_prepend (cleanup, found->data);
    new = g_slist_delete_link (new, found);
    removals++;
  }
  /* FIXME: We leak the old list here. See _add_log_function for why. */
  __log_functions = new;
  g_mutex_unlock (&__log_func_mutex);

  while (cleanup) {
    LogFuncEntry *entry = cleanup->data;

    if (entry->notify)
      entry->notify (entry->user_data);

    g_free (entry);
    cleanup = g_slist_delete_link (cleanup, cleanup);
  }
  return removals;
}

/**
 * gst_debug_remove_log_function:
 * @func: (scope call) (allow-none): the log function to remove, or %NULL to
 *     remove the default log function
 *
 * Removes all registered instances of the given logging functions.
 *
 * Returns: How many instances of the function were removed
 */
guint
gst_debug_remove_log_function (GstLogFunction func)
{
  guint removals;

  if (func == NULL)
    func = gst_debug_log_default;

  removals =
      gst_debug_remove_with_compare_func
      (gst_debug_compare_log_function_by_func, (gpointer) func);

  if (gst_is_initialized ()) {
    GST_DEBUG ("removed log function %p %d times from log function list", func,
        removals);
  } else {
    /* If the default log function is removed before gst_init() was called,
     * set a flag so we don't add it in gst_init() later */
    if (func == gst_debug_log_default) {
      add_default_log_func = FALSE;
      ++removals;
    }
  }

  return removals;
}

/**
 * gst_debug_remove_log_function_by_data:
 * @data: user data of the log function to remove
 *
 * Removes all registered instances of log functions with the given user data.
 *
 * Returns: How many instances of the function were removed
 */
guint
gst_debug_remove_log_function_by_data (gpointer data)
{
  guint removals;

  removals =
      gst_debug_remove_with_compare_func
      (gst_debug_compare_log_function_by_data, data);

  if (gst_is_initialized ())
    GST_DEBUG
        ("removed %d log functions with user data %p from log function list",
        removals, data);

  return removals;
}

/**
 * gst_debug_set_colored:
 * @colored: Whether to use colored output or not
 *
 * Sets or unsets the use of coloured debugging output.
 * Same as gst_debug_set_color_mode () with the argument being
 * being GST_DEBUG_COLOR_MODE_ON or GST_DEBUG_COLOR_MODE_OFF.
 *
 * This function may be called before gst_init().
 */
void
gst_debug_set_colored (gboolean colored)
{
  GstDebugColorMode new_mode;
  new_mode = colored ? GST_DEBUG_COLOR_MODE_ON : GST_DEBUG_COLOR_MODE_OFF;
  g_atomic_int_set (&__use_color, (gint) new_mode);
}

/**
 * gst_debug_set_color_mode:
 * @mode: The coloring mode for debug output. See @GstDebugColorMode.
 *
 * Changes the coloring mode for debug output.
 *
 * This function may be called before gst_init().
 *
 * Since: 1.2
 */
void
gst_debug_set_color_mode (GstDebugColorMode mode)
{
  g_atomic_int_set (&__use_color, mode);
}

/**
 * gst_debug_set_color_mode_from_string:
 * @mode: The coloring mode for debug output. One of the following:
 * "on", "auto", "off", "disable", "unix".
 *
 * Changes the coloring mode for debug output.
 *
 * This function may be called before gst_init().
 *
 * Since: 1.2
 */
void
gst_debug_set_color_mode_from_string (const gchar * mode)
{
  if ((strcmp (mode, "on") == 0) || (strcmp (mode, "auto") == 0))
    gst_debug_set_color_mode (GST_DEBUG_COLOR_MODE_ON);
  else if ((strcmp (mode, "off") == 0) || (strcmp (mode, "disable") == 0))
    gst_debug_set_color_mode (GST_DEBUG_COLOR_MODE_OFF);
  else if (strcmp (mode, "unix") == 0)
    gst_debug_set_color_mode (GST_DEBUG_COLOR_MODE_UNIX);
}

/**
 * gst_debug_is_colored:
 *
 * Checks if the debugging output should be colored.
 *
 * Returns: %TRUE, if the debug output should be colored.
 */
gboolean
gst_debug_is_colored (void)
{
  GstDebugColorMode mode = g_atomic_int_get (&__use_color);
  return (mode == GST_DEBUG_COLOR_MODE_UNIX || mode == GST_DEBUG_COLOR_MODE_ON);
}

/**
 * gst_debug_get_color_mode:
 *
 * Changes the coloring mode for debug output.
 *
 * Returns: see @GstDebugColorMode for possible values.
 *
 * Since: 1.2
 */
GstDebugColorMode
gst_debug_get_color_mode (void)
{
  return g_atomic_int_get (&__use_color);
}

/**
 * gst_debug_set_active:
 * @active: Whether to use debugging output or not
 *
 * If activated, debugging messages are sent to the debugging
 * handlers.
 * It makes sense to deactivate it for speed issues.
 * > This function is not threadsafe. It makes sense to only call it
 * during initialization.
 */
void
gst_debug_set_active (gboolean active)
{
  _gst_debug_enabled = active;
  if (active)
    _gst_debug_min = GST_LEVEL_COUNT;
  else
    _gst_debug_min = GST_LEVEL_NONE;
}

/**
 * gst_debug_is_active:
 *
 * Checks if debugging output is activated.
 *
 * Returns: %TRUE, if debugging is activated
 */
gboolean
gst_debug_is_active (void)
{
  return _gst_debug_enabled;
}

/**
 * gst_debug_set_default_threshold:
 * @level: level to set
 *
 * Sets the default threshold to the given level and updates all categories to
 * use this threshold.
 *
 * This function may be called before gst_init().
 */
void
gst_debug_set_default_threshold (GstDebugLevel level)
{
  g_atomic_int_set (&__default_level, level);
  gst_debug_reset_all_thresholds ();
}

/**
 * gst_debug_get_default_threshold:
 *
 * Returns the default threshold that is used for new categories.
 *
 * Returns: the default threshold level
 */
GstDebugLevel
gst_debug_get_default_threshold (void)
{
  return (GstDebugLevel) g_atomic_int_get (&__default_level);
}

#if !GLIB_CHECK_VERSION(2,70,0)
#define g_pattern_spec_match_string g_pattern_match_string
#endif

static gboolean
gst_debug_apply_entry (GstDebugCategory * cat, LevelNameEntry * entry)
{
  if (!g_pattern_spec_match_string (entry->pat, cat->name))
    return FALSE;

  if (gst_is_initialized ())
    GST_LOG ("category %s matches pattern %p - gets set to level %d",
        cat->name, entry->pat, entry->level);

  gst_debug_category_set_threshold (cat, entry->level);
  return TRUE;
}

static void
gst_debug_reset_threshold (gpointer category, gpointer unused)
{
  GstDebugCategory *cat = (GstDebugCategory *) category;
  GSList *walk;

  g_mutex_lock (&__level_name_mutex);

  for (walk = __level_name; walk != NULL; walk = walk->next) {
    if (gst_debug_apply_entry (cat, walk->data))
      break;
  }

  g_mutex_unlock (&__level_name_mutex);

  if (walk == NULL)
    gst_debug_category_set_threshold (cat, gst_debug_get_default_threshold ());
}

static void
gst_debug_reset_all_thresholds (void)
{
  g_mutex_lock (&__cat_mutex);
  g_slist_foreach (__categories, gst_debug_reset_threshold, NULL);
  g_mutex_unlock (&__cat_mutex);
}

static void
for_each_threshold_by_entry (gpointer data, gpointer user_data)
{
  GstDebugCategory *cat = (GstDebugCategory *) data;
  LevelNameEntry *entry = (LevelNameEntry *) user_data;

  gst_debug_apply_entry (cat, entry);
}

/**
 * gst_debug_set_threshold_for_name:
 * @name: name of the categories to set
 * @level: level to set them to
 *
 * Sets all categories which match the given glob style pattern to the given
 * level.
 */
void
gst_debug_set_threshold_for_name (const gchar * name, GstDebugLevel level)
{
  GPatternSpec *pat;
  LevelNameEntry *entry;

  g_return_if_fail (name != NULL);

  pat = g_pattern_spec_new (name);
  entry = g_new (LevelNameEntry, 1);
  entry->pat = pat;
  entry->level = level;
  g_mutex_lock (&__level_name_mutex);
  __level_name = g_slist_prepend (__level_name, entry);
  g_mutex_unlock (&__level_name_mutex);
  g_mutex_lock (&__cat_mutex);
  g_slist_foreach (__categories, for_each_threshold_by_entry, entry);
  g_mutex_unlock (&__cat_mutex);
}

/**
 * gst_debug_unset_threshold_for_name:
 * @name: name of the categories to set
 *
 * Resets all categories with the given name back to the default level.
 */
void
gst_debug_unset_threshold_for_name (const gchar * name)
{
  GSList *walk;
  GPatternSpec *pat;

  g_return_if_fail (name != NULL);

  pat = g_pattern_spec_new (name);
  g_mutex_lock (&__level_name_mutex);
  walk = __level_name;
  /* improve this if you want, it's mighty slow */
  while (walk) {
    LevelNameEntry *entry = walk->data;

    if (g_pattern_spec_equal (entry->pat, pat)) {
      __level_name = g_slist_remove_link (__level_name, walk);
      g_pattern_spec_free (entry->pat);
      g_free (entry);
      g_slist_free_1 (walk);
      walk = __level_name;
    } else {
      walk = g_slist_next (walk);
    }
  }
  g_mutex_unlock (&__level_name_mutex);
  g_pattern_spec_free (pat);
  gst_debug_reset_all_thresholds ();
}

GstDebugCategory *
_gst_debug_category_new (const gchar * name, guint color,
    const gchar * description)
{
  GstDebugCategory *cat, *catfound;

  g_return_val_if_fail (name != NULL, NULL);

  cat = g_new (GstDebugCategory, 1);
  cat->name = g_strdup (name);
  cat->color = color;
  if (description != NULL) {
    cat->description = g_strdup (description);
  } else {
    cat->description = g_strdup ("no description");
  }
  g_atomic_int_set (&cat->threshold, 0);
  gst_debug_reset_threshold (cat, NULL);

  /* add to category list */
  g_mutex_lock (&__cat_mutex);
  catfound = _gst_debug_get_category_locked (name);
  if (catfound) {
    g_free ((gpointer) cat->name);
    g_free ((gpointer) cat->description);
    g_free (cat);
    cat = catfound;
  } else {
    __categories = g_slist_prepend (__categories, cat);
  }
  g_mutex_unlock (&__cat_mutex);

  return cat;
}

#ifndef GST_REMOVE_DEPRECATED
/**
 * gst_debug_category_free:
 * @category: #GstDebugCategory to free.
 *
 * Removes and frees the category and all associated resources.
 *
 * Deprecated: This function can easily cause memory corruption, don't use it.
 */
void
gst_debug_category_free (GstDebugCategory * category)
{
}
#endif

/**
 * gst_debug_category_set_threshold:
 * @category: a #GstDebugCategory to set threshold of.
 * @level: the #GstDebugLevel threshold to set.
 *
 * Sets the threshold of the category to the given level. Debug information will
 * only be output if the threshold is lower or equal to the level of the
 * debugging message.
 * > Do not use this function in production code, because other functions may
 * > change the threshold of categories as side effect. It is however a nice
 * > function to use when debugging (even from gdb).
 */
void
gst_debug_category_set_threshold (GstDebugCategory * category,
    GstDebugLevel level)
{
  g_return_if_fail (category != NULL);

  if (level > _gst_debug_min) {
    _gst_debug_enabled = TRUE;
    _gst_debug_min = level;
  }

  g_atomic_int_set (&category->threshold, level);
}

/**
 * gst_debug_category_reset_threshold:
 * @category: a #GstDebugCategory to reset threshold of.
 *
 * Resets the threshold of the category to the default level. Debug information
 * will only be output if the threshold is lower or equal to the level of the
 * debugging message.
 * Use this function to set the threshold back to where it was after using
 * gst_debug_category_set_threshold().
 */
void
gst_debug_category_reset_threshold (GstDebugCategory * category)
{
  gst_debug_reset_threshold (category, NULL);
}

/**
 * gst_debug_category_get_threshold:
 * @category: a #GstDebugCategory to get threshold of.
 *
 * Returns the threshold of a #GstDebugCategory.
 *
 * Returns: the #GstDebugLevel that is used as threshold.
 */
GstDebugLevel
gst_debug_category_get_threshold (GstDebugCategory * category)
{
  return (GstDebugLevel) g_atomic_int_get (&category->threshold);
}

/**
 * gst_debug_category_get_name:
 * @category: a #GstDebugCategory to get name of.
 *
 * Returns the name of a debug category.
 *
 * Returns: the name of the category.
 */
const gchar *
gst_debug_category_get_name (GstDebugCategory * category)
{
  return category->name;
}

/**
 * gst_debug_category_get_color:
 * @category: a #GstDebugCategory to get the color of.
 *
 * Returns the color of a debug category used when printing output in this
 * category.
 *
 * Returns: the color of the category.
 */
guint
gst_debug_category_get_color (GstDebugCategory * category)
{
  return category->color;
}

/**
 * gst_debug_category_get_description:
 * @category: a #GstDebugCategory to get the description of.
 *
 * Returns the description of a debug category.
 *
 * Returns: the description of the category.
 */
const gchar *
gst_debug_category_get_description (GstDebugCategory * category)
{
  return category->description;
}

/**
 * gst_debug_get_all_categories:
 *
 * Returns a snapshot of a all categories that are currently in use . This list
 * may change anytime.
 * The caller has to free the list after use.
 *
 * Returns: (transfer container) (element-type Gst.DebugCategory): the list of
 *     debug categories
 */
GSList *
gst_debug_get_all_categories (void)
{
  GSList *ret;

  g_mutex_lock (&__cat_mutex);
  ret = g_slist_copy (__categories);
  g_mutex_unlock (&__cat_mutex);

  return ret;
}

static GstDebugCategory *
_gst_debug_get_category_locked (const gchar * name)
{
  GstDebugCategory *ret = NULL;
  GSList *node;

  for (node = __categories; node; node = g_slist_next (node)) {
    ret = (GstDebugCategory *) node->data;
    if (!strcmp (name, ret->name)) {
      return ret;
    }
  }
  return NULL;
}

GstDebugCategory *
_gst_debug_get_category (const gchar * name)
{
  GstDebugCategory *ret;

  g_mutex_lock (&__cat_mutex);
  ret = _gst_debug_get_category_locked (name);
  g_mutex_unlock (&__cat_mutex);

  return ret;
}

static gboolean
parse_debug_category (gchar * str, const gchar ** category)
{
  if (!str)
    return FALSE;

  /* works in place */
  g_strstrip (str);

  if (str[0] != '\0') {
    *category = str;
    return TRUE;
  }

  return FALSE;
}

static gboolean
parse_debug_level (gchar * str, GstDebugLevel * level)
{
  if (!str)
    return FALSE;

  /* works in place */
  g_strstrip (str);

  if (g_ascii_isdigit (str[0])) {
    unsigned long l;
    char *endptr;
    l = strtoul (str, &endptr, 10);
    if (endptr > str && endptr[0] == 0) {
      *level = (GstDebugLevel) l;
    } else {
      return FALSE;
    }
  } else if (strcmp (str, "NONE") == 0) {
    *level = GST_LEVEL_NONE;
  } else if (strcmp (str, "ERROR") == 0) {
    *level = GST_LEVEL_ERROR;
  } else if (strncmp (str, "WARN", 4) == 0) {
    *level = GST_LEVEL_WARNING;
  } else if (strcmp (str, "FIXME") == 0) {
    *level = GST_LEVEL_FIXME;
  } else if (strcmp (str, "INFO") == 0) {
    *level = GST_LEVEL_INFO;
  } else if (strcmp (str, "DEBUG") == 0) {
    *level = GST_LEVEL_DEBUG;
  } else if (strcmp (str, "LOG") == 0) {
    *level = GST_LEVEL_LOG;
  } else if (strcmp (str, "TRACE") == 0) {
    *level = GST_LEVEL_TRACE;
  } else if (strcmp (str, "MEMDUMP") == 0) {
    *level = GST_LEVEL_MEMDUMP;
  } else
    return FALSE;

  return TRUE;
}

/**
 * gst_debug_set_threshold_from_string:
 * @list: comma-separated list of "category:level" pairs to be used
 *     as debug logging levels
 * @reset: %TRUE to clear all previously-set debug levels before setting
 *     new thresholds
 * %FALSE if adding the threshold described by @list to the one already set.
 *
 * Sets the debug logging wanted in the same form as with the GST_DEBUG
 * environment variable. You can use wildcards such as `*`, but note that
 * the order matters when you use wild cards, e.g. `foosrc:6,*src:3,*:2` sets
 * everything to log level 2.
 *
 * Since: 1.2
 */
void
gst_debug_set_threshold_from_string (const gchar * list, gboolean reset)
{
  gchar **split;
  gchar **walk;

  g_assert (list);

  if (reset) {
    clear_level_names ();
    gst_debug_set_default_threshold (GST_LEVEL_DEFAULT);
  }

  split = g_strsplit (list, ",", 0);

  for (walk = split; *walk; walk++) {
    if (strchr (*walk, ':')) {
      gchar **values = g_strsplit (*walk, ":", 2);

      if (values[0] && values[1]) {
        GstDebugLevel level;
        const gchar *category;

        if (parse_debug_category (values[0], &category)
            && parse_debug_level (values[1], &level)) {
          gst_debug_set_threshold_for_name (category, level);

          /* bump min-level anyway to allow the category to be registered in the
           * future still */
          if (level > _gst_debug_min) {
            _gst_debug_min = level;
          }
        }
      }

      g_strfreev (values);
    } else {
      GstDebugLevel level;

      if (parse_debug_level (*walk, &level))
        gst_debug_set_default_threshold (level);
    }
  }

  g_strfreev (split);
}

/*** FUNCTION POINTERS ********************************************************/

static GHashTable *__gst_function_pointers;     /* NULL */
static GMutex __dbg_functions_mutex;

/* This function MUST NOT return NULL */
const gchar *
_gst_debug_nameof_funcptr (GstDebugFuncPtr func)
{
  gchar *ptrname;

#ifdef HAVE_DLADDR
  Dl_info dl_info;
#endif

  if (G_UNLIKELY (func == NULL))
    return "(NULL)";

  g_mutex_lock (&__dbg_functions_mutex);
  if (G_LIKELY (__gst_function_pointers)) {
    ptrname = g_hash_table_lookup (__gst_function_pointers, (gpointer) func);
    g_mutex_unlock (&__dbg_functions_mutex);
    if (G_LIKELY (ptrname))
      return ptrname;
  } else {
    g_mutex_unlock (&__dbg_functions_mutex);
  }
  /* we need to create an entry in the hash table for this one so we don't leak
   * the name */
#ifdef HAVE_DLADDR
  if (dladdr ((gpointer) func, &dl_info) && dl_info.dli_sname) {
    const gchar *name = g_intern_string (dl_info.dli_sname);

    _gst_debug_register_funcptr (func, name);
    return name;
  } else
#endif
  {
    gchar *name = g_strdup_printf ("%p", (gpointer) func);
    const gchar *iname = g_intern_string (name);

    g_free (name);

    _gst_debug_register_funcptr (func, iname);
    return iname;
  }
}

void
_gst_debug_register_funcptr (GstDebugFuncPtr func, const gchar * ptrname)
{
  gpointer ptr = (gpointer) func;

  g_mutex_lock (&__dbg_functions_mutex);

  if (!__gst_function_pointers)
    __gst_function_pointers = g_hash_table_new (g_direct_hash, g_direct_equal);
  if (!g_hash_table_lookup (__gst_function_pointers, ptr)) {
    g_hash_table_insert (__gst_function_pointers, ptr, (gpointer) ptrname);
  }

  g_mutex_unlock (&__dbg_functions_mutex);
}

static void
clear_level_names (void)
{
  g_mutex_lock (&__level_name_mutex);
  while (__level_name) {
    LevelNameEntry *level_name_entry = __level_name->data;
    g_pattern_spec_free (level_name_entry->pat);
    g_free (level_name_entry);
    __level_name = g_slist_delete_link (__level_name, __level_name);
  }
  g_mutex_unlock (&__level_name_mutex);
}

void
_priv_gst_debug_cleanup (void)
{
  g_mutex_lock (&__dbg_functions_mutex);

  if (__gst_function_pointers) {
    g_hash_table_unref (__gst_function_pointers);
    __gst_function_pointers = NULL;
  }

  g_mutex_unlock (&__dbg_functions_mutex);

  g_mutex_lock (&__cat_mutex);
  while (__categories) {
    GstDebugCategory *cat = __categories->data;
    g_free ((gpointer) cat->name);
    g_free ((gpointer) cat->description);
    g_free (cat);
    __categories = g_slist_delete_link (__categories, __categories);
  }
  g_mutex_unlock (&__cat_mutex);

  clear_level_names ();

  g_mutex_lock (&__log_func_mutex);
  while (__log_functions) {
    LogFuncEntry *log_func_entry = __log_functions->data;
    if (log_func_entry->notify)
      log_func_entry->notify (log_func_entry->user_data);
    g_free (log_func_entry);
    __log_functions = g_slist_delete_link (__log_functions, __log_functions);
  }
  g_mutex_unlock (&__log_func_mutex);
}

static void
gst_info_dump_mem_line (gchar * linebuf, gsize linebuf_size,
    const guint8 * mem, gsize mem_offset, gsize mem_size)
{
  gchar hexstr[50], ascstr[18], digitstr[4];

  if (mem_size > 16)
    mem_size = 16;

  hexstr[0] = '\0';
  ascstr[0] = '\0';

  if (mem != NULL) {
    guint i = 0;

    mem += mem_offset;
    while (i < mem_size) {
      ascstr[i] = (g_ascii_isprint (mem[i])) ? mem[i] : '.';
      g_snprintf (digitstr, sizeof (digitstr), "%02x ", mem[i]);
      g_strlcat (hexstr, digitstr, sizeof (hexstr));
      ++i;
    }
    ascstr[i] = '\0';
  }

  g_snprintf (linebuf, linebuf_size, "%08x: %-48.48s %-16.16s",
      (guint) mem_offset, hexstr, ascstr);
}

static void
_gst_debug_dump_mem_full (GstDebugCategory * cat, const gchar * file,
    const gchar * func, gint line, GObject * obj, const gchar * object_id,
    const gchar * msg, const guint8 * data, guint length)
{
  guint off = 0;
  gboolean free_object_id = FALSE;
  gchar tmp_id[32];

  if (object_id == NULL && obj)
    object_id = _get_object_id (obj, &free_object_id, (gchar *) & tmp_id);

  gst_debug_log_id ((cat), GST_LEVEL_MEMDUMP, file, func, line, object_id,
      "--------"
      "-------------------------------------------------------------------");

  if (msg != NULL && *msg != '\0') {
    gst_debug_log_id ((cat), GST_LEVEL_MEMDUMP, file, func, line, object_id,
        "%s", msg);
  }

  while (off < length) {
    gchar buf[128];

    /* gst_info_dump_mem_line will process 16 bytes at most */
    gst_info_dump_mem_line (buf, sizeof (buf), data, off, length - off);
    gst_debug_log_id (cat, GST_LEVEL_MEMDUMP, file, func, line, object_id, "%s",
        buf);
    off += 16;
  }

  gst_debug_log_id ((cat), GST_LEVEL_MEMDUMP, file, func, line, object_id,
      "--------"
      "-------------------------------------------------------------------");

  if (free_object_id)
    g_free ((gchar *) object_id);
}

void
_gst_debug_dump_mem (GstDebugCategory * cat, const gchar * file,
    const gchar * func, gint line, GObject * obj, const gchar * msg,
    const guint8 * data, guint length)
{
  _gst_debug_dump_mem_full (cat, file, func, line, obj, NULL, msg, data,
      length);
}

void
_gst_debug_dump_mem_id (GstDebugCategory * cat, const gchar * file,
    const gchar * func, gint line, const gchar * object_id, const gchar * msg,
    const guint8 * data, guint length)
{
  _gst_debug_dump_mem_full (cat, file, func, line, NULL, object_id, msg, data,
      length);
}


#else /* !GST_DISABLE_GST_DEBUG */
#ifndef GST_REMOVE_DISABLED

GstDebugCategory *
_gst_debug_category_new (const gchar * name, guint color,
    const gchar * description)
{
  return NULL;
}

void
_gst_debug_register_funcptr (GstDebugFuncPtr func, const gchar * ptrname)
{
}

/* This function MUST NOT return NULL */
const gchar *
_gst_debug_nameof_funcptr (GstDebugFuncPtr func)
{
  return "(NULL)";
}

void
_priv_gst_debug_cleanup (void)
{
}

void
gst_debug_log (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    GObject * object, const gchar * format, ...)
{
}

void
gst_debug_log_valist (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    GObject * object, const gchar * format, va_list args)
{
}

void
gst_debug_log_literal (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    GObject * object, const gchar * message_string)
{
}

void
gst_debug_log_id_literal (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    const gchar * id, const gchar * message_string)
{
}

const gchar *
gst_debug_message_get (GstDebugMessage * message)
{
  return "";
}

const gchar *
gst_debug_message_get_id (GstDebugMessage * message)
{
  return NULL;
}

void
gst_debug_log_default (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    GObject * object, GstDebugMessage * message, gpointer unused)
{
}

const gchar *
gst_debug_level_get_name (GstDebugLevel level)
{
  return "NONE";
}

void
gst_debug_add_log_function (GstLogFunction func, gpointer user_data,
    GDestroyNotify notify)
{
  if (notify)
    notify (user_data);
}

guint
gst_debug_remove_log_function (GstLogFunction func)
{
  return 0;
}

guint
gst_debug_remove_log_function_by_data (gpointer data)
{
  return 0;
}

void
gst_debug_set_active (gboolean active)
{
}

gboolean
gst_debug_is_active (void)
{
  return FALSE;
}

void
gst_debug_set_colored (gboolean colored)
{
}

void
gst_debug_set_color_mode (GstDebugColorMode mode)
{
}

void
gst_debug_set_color_mode_from_string (const gchar * str)
{
}

gboolean
gst_debug_is_colored (void)
{
  return FALSE;
}

GstDebugColorMode
gst_debug_get_color_mode (void)
{
  return GST_DEBUG_COLOR_MODE_OFF;
}

void
gst_debug_set_threshold_from_string (const gchar * list, gboolean reset)
{
}

void
gst_debug_set_default_threshold (GstDebugLevel level)
{
}

GstDebugLevel
gst_debug_get_default_threshold (void)
{
  return GST_LEVEL_NONE;
}

void
gst_debug_set_threshold_for_name (const gchar * name, GstDebugLevel level)
{
}

void
gst_debug_unset_threshold_for_name (const gchar * name)
{
}

#ifndef GST_REMOVE_DEPRECATED
void
gst_debug_category_free (GstDebugCategory * category)
{
}
#endif

void
gst_debug_category_set_threshold (GstDebugCategory * category,
    GstDebugLevel level)
{
}

void
gst_debug_category_reset_threshold (GstDebugCategory * category)
{
}

GstDebugLevel
gst_debug_category_get_threshold (GstDebugCategory * category)
{
  return GST_LEVEL_NONE;
}

const gchar *
gst_debug_category_get_name (GstDebugCategory * category)
{
  return "";
}

guint
gst_debug_category_get_color (GstDebugCategory * category)
{
  return 0;
}

const gchar *
gst_debug_category_get_description (GstDebugCategory * category)
{
  return "";
}

GSList *
gst_debug_get_all_categories (void)
{
  return NULL;
}

GstDebugCategory *
_gst_debug_get_category (const gchar * name)
{
  return NULL;
}

gchar *
gst_debug_construct_term_color (guint colorinfo)
{
  return g_strdup ("00");
}

gint
gst_debug_construct_win_color (guint colorinfo)
{
  return 0;
}

void
_gst_debug_dump_mem (GstDebugCategory * cat, const gchar * file,
    const gchar * func, gint line, GObject * obj, const gchar * msg,
    const guint8 * data, guint length)
{
}
#endif /* GST_REMOVE_DISABLED */
#endif /* GST_DISABLE_GST_DEBUG */

/**
 * gst_info_vasprintf:
 * @result: (out): the resulting string
 * @format: a printf style format string
 * @args: the va_list of printf arguments for @format
 *
 * Allocates and fills a string large enough (including the terminating null
 * byte) to hold the specified printf style @format and @args.
 *
 * This function deals with the GStreamer specific printf specifiers
 * #GST_PTR_FORMAT and #GST_SEGMENT_FORMAT.  If you do not have these specifiers
 * in your @format string, you do not need to use this function and can use
 * alternatives such as g_vasprintf().
 *
 * Free @result with g_free().
 *
 * Returns: the length of the string allocated into @result or -1 on any error
 *
 * Since: 1.8
 */
gint
gst_info_vasprintf (gchar ** result, const gchar * format, va_list args)
{
  return __gst_vasprintf (result, format, args);
}

/**
 * gst_info_strdup_vprintf:
 * @format: a printf style format string
 * @args: the va_list of printf arguments for @format
 *
 * Allocates, fills and returns a null terminated string from the printf style
 * @format string and @args.
 *
 * See gst_info_vasprintf() for when this function is required.
 *
 * Free with g_free().
 *
 * Returns: (nullable): a newly allocated null terminated string or %NULL on any error
 *
 * Since: 1.8
 */
gchar *
gst_info_strdup_vprintf (const gchar * format, va_list args)
{
  gchar *ret;

  if (gst_info_vasprintf (&ret, format, args) < 0)
    ret = NULL;

  return ret;
}

/**
 * gst_info_strdup_printf:
 * @format: a printf style format string
 * @...: the printf arguments for @format
 *
 * Allocates, fills and returns a 0-terminated string from the printf style
 * @format string and corresponding arguments.
 *
 * See gst_info_vasprintf() for when this function is required.
 *
 * Free with g_free().
 *
 * Returns: (nullable): a newly allocated null terminated string or %NULL on any error
 *
 * Since: 1.8
 */
gchar *
gst_info_strdup_printf (const gchar * format, ...)
{
  gchar *ret;
  va_list args;

  va_start (args, format);
  ret = gst_info_strdup_vprintf (format, args);
  va_end (args);

  return ret;
}

/**
 * gst_print:
 * @format: a printf style format string
 * @...: the printf arguments for @format
 *
 * Outputs a formatted message via the GLib print handler. The default print
 * handler simply outputs the message to stdout.
 *
 * This function will not append a new-line character at the end, unlike
 * gst_println() which will.
 *
 * All strings must be in ASCII or UTF-8 encoding.
 *
 * This function differs from g_print() in that it supports all the additional
 * printf specifiers that are supported by GStreamer's debug logging system,
 * such as #GST_PTR_FORMAT and #GST_SEGMENT_FORMAT.
 *
 * This function is primarily for printing debug output.
 *
 * Since: 1.12
 */
void
gst_print (const gchar * format, ...)
{
  va_list args;
  gchar *str;

  va_start (args, format);
  str = gst_info_strdup_vprintf (format, args);
  va_end (args);

#ifdef G_OS_WIN32
  G_LOCK (win_print_mutex);
#endif

  g_print ("%s", str);

#ifdef G_OS_WIN32
  G_UNLOCK (win_print_mutex);
#endif
  g_free (str);
}

/**
 * gst_println:
 * @format: a printf style format string
 * @...: the printf arguments for @format
 *
 * Outputs a formatted message via the GLib print handler. The default print
 * handler simply outputs the message to stdout.
 *
 * This function will append a new-line character at the end, unlike
 * gst_print() which will not.
 *
 * All strings must be in ASCII or UTF-8 encoding.
 *
 * This function differs from g_print() in that it supports all the additional
 * printf specifiers that are supported by GStreamer's debug logging system,
 * such as #GST_PTR_FORMAT and #GST_SEGMENT_FORMAT.
 *
 * This function is primarily for printing debug output.
 *
 * Since: 1.12
 */
void
gst_println (const gchar * format, ...)
{
  va_list args;
  gchar *str;

  va_start (args, format);
  str = gst_info_strdup_vprintf (format, args);
  va_end (args);

#ifdef G_OS_WIN32
  G_LOCK (win_print_mutex);
#endif

  g_print ("%s\n", str);

#ifdef G_OS_WIN32
  G_UNLOCK (win_print_mutex);
#endif
  g_free (str);
}

/**
 * gst_printerr:
 * @format: a printf style format string
 * @...: the printf arguments for @format
 *
 * Outputs a formatted message via the GLib error message handler. The default
 * handler simply outputs the message to stderr.
 *
 * This function will not append a new-line character at the end, unlike
 * gst_printerrln() which will.
 *
 * All strings must be in ASCII or UTF-8 encoding.
 *
 * This function differs from g_printerr() in that it supports the additional
 * printf specifiers that are supported by GStreamer's debug logging system,
 * such as #GST_PTR_FORMAT and #GST_SEGMENT_FORMAT.
 *
 * This function is primarily for printing debug output.
 *
 * Since: 1.12
 */
void
gst_printerr (const gchar * format, ...)
{
  va_list args;
  gchar *str;

  va_start (args, format);
  str = gst_info_strdup_vprintf (format, args);
  va_end (args);

#ifdef G_OS_WIN32
  G_LOCK (win_print_mutex);
#endif

  g_printerr ("%s", str);

#ifdef G_OS_WIN32
  G_UNLOCK (win_print_mutex);
#endif
  g_free (str);
}

/**
 * gst_printerrln:
 * @format: a printf style format string
 * @...: the printf arguments for @format
 *
 * Outputs a formatted message via the GLib error message handler. The default
 * handler simply outputs the message to stderr.
 *
 * This function will append a new-line character at the end, unlike
 * gst_printerr() which will not.
 *
 * All strings must be in ASCII or UTF-8 encoding.
 *
 * This function differs from g_printerr() in that it supports the additional
 * printf specifiers that are supported by GStreamer's debug logging system,
 * such as #GST_PTR_FORMAT and #GST_SEGMENT_FORMAT.
 *
 * This function is primarily for printing debug output.
 *
 * Since: 1.12
 */
void
gst_printerrln (const gchar * format, ...)
{
  va_list args;
  gchar *str;

  va_start (args, format);
  str = gst_info_strdup_vprintf (format, args);
  va_end (args);

#ifdef G_OS_WIN32
  G_LOCK (win_print_mutex);
#endif

  g_printerr ("%s\n", str);

#ifdef G_OS_WIN32
  G_UNLOCK (win_print_mutex);
#endif
  g_free (str);
}

#ifdef HAVE_UNWIND
#ifdef HAVE_DW
static gboolean
append_debug_info (GString * trace, Dwfl * dwfl, const void *ip)
{
  Dwfl_Line *line;
  Dwarf_Addr addr;
  Dwfl_Module *module;
  const gchar *function_name;

  addr = (uintptr_t) ip;
  module = dwfl_addrmodule (dwfl, addr);
  function_name = dwfl_module_addrname (module, addr);

  g_string_append_printf (trace, "%s (", function_name ? function_name : "??");

  line = dwfl_getsrc (dwfl, addr);
  if (line != NULL) {
    gint nline;
    Dwarf_Addr addr;
    const gchar *filename = dwfl_lineinfo (line, &addr,
        &nline, NULL, NULL, NULL);

    g_string_append_printf (trace, "%s:%d", strrchr (filename,
            G_DIR_SEPARATOR) + 1, nline);
  } else {
    const gchar *eflfile = NULL;

    dwfl_module_info (module, NULL, NULL, NULL, NULL, NULL, &eflfile, NULL);
    g_string_append_printf (trace, "%s:%p", eflfile ? eflfile : "??", ip);
  }

  return TRUE;
}
#endif /* HAVE_DW */

static gchar *
generate_unwind_trace (GstStackTraceFlags flags)
{
  gint unret;
  unw_context_t uc;
  unw_cursor_t cursor;
  gboolean use_libunwind = TRUE;
  GString *trace = g_string_new (NULL);

#ifdef HAVE_DW
  Dwfl *dwfl = NULL;

  if ((flags & GST_STACK_TRACE_SHOW_FULL)) {
    dwfl = get_global_dwfl ();
    if (G_UNLIKELY (dwfl == NULL)) {
      GST_WARNING ("Failed to initialize dwlf");
      goto done;
    }
    GST_DWFL_LOCK ();
  }
#endif /* HAVE_DW */

  unret = unw_getcontext (&uc);
  if (unret) {
    GST_DEBUG ("Could not get libunwind context (%d)", unret);

    goto done;
  }
  unret = unw_init_local (&cursor, &uc);
  if (unret) {
    GST_DEBUG ("Could not init libunwind context (%d)", unret);

    goto done;
  }
#ifdef HAVE_DW
  /* Due to plugins being loaded, mapping of process might have changed,
   * so always scan it. */
  if (dwfl_linux_proc_report (dwfl, _gst_getpid ()) != 0)
    goto done;
#endif

  while (unw_step (&cursor) > 0) {
#ifdef HAVE_DW
    if (dwfl) {
      unw_word_t ip;

      unret = unw_get_reg (&cursor, UNW_REG_IP, &ip);
      if (unret) {
        GST_DEBUG ("libunwind could not read frame info (%d)", unret);

        goto done;
      }

      if (append_debug_info (trace, dwfl, (void *) (ip - 4))) {
        use_libunwind = FALSE;
        g_string_append (trace, ")\n");
      }
    }
#endif /* HAVE_DW */

    if (use_libunwind) {
      char name[32];

      unw_word_t offset = 0;
      unw_get_proc_name (&cursor, name, sizeof (name), &offset);
      g_string_append_printf (trace, "%s (0x%" G_GSIZE_FORMAT ")\n", name,
          (gsize) offset);
    }
  }

done:
#ifdef HAVE_DW
  if (dwfl)
    GST_DWFL_UNLOCK ();
#endif

  return g_string_free (trace, FALSE);
}

#endif /* HAVE_UNWIND */

#ifdef HAVE_BACKTRACE
static gchar *
generate_backtrace_trace (void)
{
  int j, nptrs;
  void *buffer[BT_BUF_SIZE];
  char **strings;
  GString *trace;

  nptrs = backtrace (buffer, BT_BUF_SIZE);

  strings = backtrace_symbols (buffer, nptrs);

  if (!strings)
    return NULL;

  trace = g_string_new (NULL);

  for (j = 0; j < nptrs; j++)
    g_string_append_printf (trace, "%s\n", strings[j]);

  free (strings);

  return g_string_free (trace, FALSE);
}
#else
#define generate_backtrace_trace() NULL
#endif /* HAVE_BACKTRACE */

#ifdef HAVE_DBGHELP
/* *INDENT-OFF* */
static struct
{
  DWORD (WINAPI * pSymSetOptions) (DWORD SymOptions);
  BOOL  (WINAPI * pSymInitialize) (HANDLE hProcess,
                                   PCSTR UserSearchPath,
                                   BOOL fInvadeProcess);
  BOOL  (WINAPI * pStackWalk64)   (DWORD MachineType,
                                   HANDLE hProcess,
                                   HANDLE hThread,
                                   LPSTACKFRAME64 StackFrame,
                                   PVOID ContextRecord,
                                   PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
                                   PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
                                   PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
                                   PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);
  PVOID (WINAPI * pSymFunctionTableAccess64) (HANDLE hProcess,
                                              DWORD64 AddrBase);
  DWORD64 (WINAPI * pSymGetModuleBase64) (HANDLE hProcess,
                                          DWORD64 qwAddr);
  BOOL (WINAPI * pSymFromAddr) (HANDLE hProcess,
                                DWORD64 Address,
                                PDWORD64 Displacement,
                                PSYMBOL_INFO Symbol);
  BOOL (WINAPI * pSymGetModuleInfo64) (HANDLE hProcess,
                                       DWORD64 qwAddr,
                                       PIMAGEHLP_MODULE64 ModuleInfo);
  BOOL (WINAPI * pSymGetLineFromAddr64) (HANDLE hProcess,
                                         DWORD64 qwAddr,
                                         PDWORD pdwDisplacement,
                                         PIMAGEHLP_LINE64 Line64);
} dbg_help_vtable = { NULL,};
/* *INDENT-ON* */

static GModule *dbg_help_module = NULL;

static gboolean
dbghelp_load_symbol (const gchar * symbol_name, gpointer * symbol)
{
  if (dbg_help_module &&
      !g_module_symbol (dbg_help_module, symbol_name, symbol)) {
    GST_WARNING ("Cannot load %s symbol", symbol_name);
    g_module_close (dbg_help_module);
    dbg_help_module = NULL;
  }

  return dbg_help_module != NULL;
}

static gboolean
dbghelp_initialize_symbols (HANDLE process)
{
  static gsize initialization_value = 0;

  if (g_once_init_enter (&initialization_value)) {
    GST_INFO ("Initializing Windows symbol handler");

    dbg_help_module = g_module_open ("dbghelp.dll", G_MODULE_BIND_LAZY);
    dbghelp_load_symbol ("SymSetOptions",
        (gpointer *) & dbg_help_vtable.pSymSetOptions);
    dbghelp_load_symbol ("SymInitialize",
        (gpointer *) & dbg_help_vtable.pSymInitialize);
    dbghelp_load_symbol ("StackWalk64",
        (gpointer *) & dbg_help_vtable.pStackWalk64);
    dbghelp_load_symbol ("SymFunctionTableAccess64",
        (gpointer *) & dbg_help_vtable.pSymFunctionTableAccess64);
    dbghelp_load_symbol ("SymGetModuleBase64",
        (gpointer *) & dbg_help_vtable.pSymGetModuleBase64);
    dbghelp_load_symbol ("SymFromAddr",
        (gpointer *) & dbg_help_vtable.pSymFromAddr);
    dbghelp_load_symbol ("SymGetModuleInfo64",
        (gpointer *) & dbg_help_vtable.pSymGetModuleInfo64);
    dbghelp_load_symbol ("SymGetLineFromAddr64",
        (gpointer *) & dbg_help_vtable.pSymGetLineFromAddr64);

    if (dbg_help_module) {
      dbg_help_vtable.pSymSetOptions (SYMOPT_LOAD_LINES);
      dbg_help_vtable.pSymInitialize (process, NULL, TRUE);
      GST_INFO ("Initialized Windows symbol handler");
    }

    g_once_init_leave (&initialization_value, 1);
  }

  return dbg_help_module != NULL;
}

static gchar *
generate_dbghelp_trace (void)
{
  HANDLE process = GetCurrentProcess ();
  HANDLE thread = GetCurrentThread ();
  IMAGEHLP_MODULE64 module_info;
  DWORD machine;
  CONTEXT context;
  STACKFRAME64 frame = { 0 };
  PVOID save_context;
  GString *trace = NULL;

  if (!dbghelp_initialize_symbols (process))
    return NULL;

  memset (&context, 0, sizeof (CONTEXT));
  context.ContextFlags = CONTEXT_FULL;

  RtlCaptureContext (&context);

  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrStack.Mode = AddrModeFlat;
  frame.AddrFrame.Mode = AddrModeFlat;

#if (defined _M_IX86)
  machine = IMAGE_FILE_MACHINE_I386;
  frame.AddrFrame.Offset = context.Ebp;
  frame.AddrPC.Offset = context.Eip;
  frame.AddrStack.Offset = context.Esp;
#elif (defined _M_X64)
  machine = IMAGE_FILE_MACHINE_AMD64;
  frame.AddrFrame.Offset = context.Rbp;
  frame.AddrPC.Offset = context.Rip;
  frame.AddrStack.Offset = context.Rsp;
#else
  return NULL;
#endif

  trace = g_string_new (NULL);

  module_info.SizeOfStruct = sizeof (module_info);
  save_context = (machine == IMAGE_FILE_MACHINE_I386) ? NULL : &context;

  while (TRUE) {
    char buffer[sizeof (SYMBOL_INFO) + MAX_SYM_NAME * sizeof (TCHAR)];
    PSYMBOL_INFO symbol = (PSYMBOL_INFO) buffer;
    IMAGEHLP_LINE64 line;
    DWORD displacement = 0;

    symbol->SizeOfStruct = sizeof (SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    line.SizeOfStruct = sizeof (line);

    if (!dbg_help_vtable.pStackWalk64 (machine, process, thread, &frame,
            save_context, 0, dbg_help_vtable.pSymFunctionTableAccess64,
            dbg_help_vtable.pSymGetModuleBase64, 0)) {
      break;
    }

    if (dbg_help_vtable.pSymFromAddr (process, frame.AddrPC.Offset, 0, symbol))
      g_string_append_printf (trace, "%s ", symbol->Name);
    else
      g_string_append (trace, "?? ");

    if (dbg_help_vtable.pSymGetLineFromAddr64 (process, frame.AddrPC.Offset,
            &displacement, &line))
      g_string_append_printf (trace, "(%s:%lu)", line.FileName,
          line.LineNumber);
    else if (dbg_help_vtable.pSymGetModuleInfo64 (process, frame.AddrPC.Offset,
            &module_info))
      g_string_append_printf (trace, "(%s)", module_info.ImageName);
    else
      g_string_append_printf (trace, "(%s)", "??");

    g_string_append (trace, "\n");
  }

  return g_string_free (trace, FALSE);
}
#endif /* HAVE_DBGHELP */

/**
 * gst_debug_get_stack_trace:
 * @flags: A set of #GstStackTraceFlags to determine how the stack trace should
 * look like. Pass #GST_STACK_TRACE_SHOW_NONE to retrieve a minimal backtrace.
 *
 * Returns: (nullable): a stack trace, if libunwind or glibc backtrace are
 * present, else %NULL.
 *
 * Since: 1.12
 */
gchar *
gst_debug_get_stack_trace (GstStackTraceFlags flags)
{
  gchar *trace = NULL;
#ifdef HAVE_BACKTRACE
  gboolean have_backtrace = TRUE;
#else
  gboolean have_backtrace = FALSE;
#endif

#ifdef HAVE_UNWIND
  if ((flags & GST_STACK_TRACE_SHOW_FULL) || !have_backtrace)
    trace = generate_unwind_trace (flags);
#elif defined(HAVE_DBGHELP)
  trace = generate_dbghelp_trace ();
#endif

  if (trace)
    return trace;
  else if (have_backtrace)
    return generate_backtrace_trace ();

  return NULL;
}

/**
 * gst_debug_print_stack_trace:
 *
 * If libunwind, glibc backtrace or DbgHelp are present
 * a stack trace is printed.
 */
void
gst_debug_print_stack_trace (void)
{
  gchar *trace = gst_debug_get_stack_trace (GST_STACK_TRACE_SHOW_FULL);

  if (trace) {
#ifdef G_OS_WIN32
    G_LOCK (win_print_mutex);
#endif

    g_print ("%s\n", trace);

#ifdef G_OS_WIN32
    G_UNLOCK (win_print_mutex);
#endif
  }

  g_free (trace);
}

#ifndef GST_DISABLE_GST_DEBUG
typedef struct
{
  guint max_size_per_thread;
  guint thread_timeout;
  GQueue threads;
  GHashTable *thread_index;
} GstRingBufferLogger;

typedef struct
{
  GList *link;
  gint64 last_use;
  GThread *thread;

  GQueue log;
  gsize log_size;
} GstRingBufferLog;

G_LOCK_DEFINE_STATIC (ring_buffer_logger);
static GstRingBufferLogger *ring_buffer_logger = NULL;

static void
gst_ring_buffer_logger_log (GstDebugCategory * category,
    GstDebugLevel level, const gchar * file, const gchar * function,
    gint line, GObject * object, GstDebugMessage * message, gpointer user_data)
{
  GstRingBufferLogger *logger = user_data;
  GThread *thread;
  GstClockTime elapsed;
  gchar c;
  gchar *output;
  gsize output_len;
  GstRingBufferLog *log;
  gint64 now = g_get_monotonic_time ();
  const gchar *message_str = gst_debug_message_get (message);
  const gchar *object_id = gst_debug_message_get_id (message);

  /* __FILE__ might be a file name or an absolute path or a
   * relative path, irrespective of the exact compiler used,
   * in which case we want to shorten it to the filename for
   * readability. */
  c = file[0];
  if (c == '.' || c == '/' || c == '\\' || (c != '\0' && file[1] == ':')) {
    file = gst_path_basename (file);
  }

  elapsed = GST_CLOCK_DIFF (_priv_gst_start_time, gst_util_get_timestamp ());
  thread = g_thread_self ();

  if (object_id) {
    /* no color, all platforms */
    output =
        g_strdup_printf ("%" GST_TIME_FORMAT NOCOLOR_PRINT_FMT_ID,
        GST_TIME_ARGS (elapsed), _gst_getpid (), thread,
        gst_debug_level_get_name (level),
        gst_debug_category_get_name (category), file, line, function,
        object_id, message_str);
  } else {
    /* no color, all platforms */
    output =
        g_strdup_printf ("%" GST_TIME_FORMAT NOCOLOR_PRINT_FMT,
        GST_TIME_ARGS (elapsed), _gst_getpid (), thread,
        gst_debug_level_get_name (level),
        gst_debug_category_get_name (category), file, line, function, "",
        message_str);
  }

  output_len = strlen (output);


  G_LOCK (ring_buffer_logger);

  if (logger->thread_timeout > 0) {
    gchar *buf;

    /* Remove all threads that saw no output since thread_timeout seconds.
     * By construction these are all at the tail of the queue, and the queue
     * is ordered by last use, so we just need to look at the tail.
     */
    while (logger->threads.tail) {
      log = logger->threads.tail->data;
      if (log->last_use + logger->thread_timeout * G_USEC_PER_SEC >= now)
        break;

      g_hash_table_remove (logger->thread_index, log->thread);
      while ((buf = g_queue_pop_head (&log->log)))
        g_free (buf);
      g_free (log);
      g_queue_pop_tail (&logger->threads);
    }
  }

  /* Get logger for this thread, and put it back at the
   * head of the threads queue */
  log = g_hash_table_lookup (logger->thread_index, thread);
  if (!log) {
    log = g_new0 (GstRingBufferLog, 1);
    g_queue_init (&log->log);
    log->log_size = 0;
    g_queue_push_head (&logger->threads, log);
    log->link = logger->threads.head;
    log->thread = thread;
    g_hash_table_insert (logger->thread_index, thread, log);
  } else {
    g_queue_unlink (&logger->threads, log->link);
    g_queue_push_head_link (&logger->threads, log->link);
  }
  log->last_use = now;

  if (output_len < logger->max_size_per_thread) {
    gchar *buf;

    /* While using a GQueue here is not the most efficient thing to do, we
     * have to allocate a string for every output anyway and could just store
     * that instead of copying it to an actual ringbuffer.
     * Better than GQueue would be GstQueueArray, but that one is in
     * libgstbase and we can't use it here. That one allocation will not make
     * much of a difference anymore, considering the number of allocations
     * needed to get to this point...
     */
    while (log->log_size + output_len > logger->max_size_per_thread) {
      buf = g_queue_pop_head (&log->log);
      log->log_size -= strlen (buf);
      g_free (buf);
    }
    g_queue_push_tail (&log->log, output);
    log->log_size += output_len;
  } else {
    gchar *buf;

    /* Can't really write anything as the line is bigger than the maximum
     * allowed log size already, so just remove everything */

    while ((buf = g_queue_pop_head (&log->log)))
      g_free (buf);
    g_free (output);
    log->log_size = 0;
  }

  G_UNLOCK (ring_buffer_logger);
}

/**
 * gst_debug_ring_buffer_logger_get_logs:
 *
 * Fetches the current logs per thread from the ring buffer logger. See
 * gst_debug_add_ring_buffer_logger() for details.
 *
 * Returns: (transfer full) (array zero-terminated=1): NULL-terminated array of
 * strings with the debug output per thread
 *
 * Since: 1.14
 */
gchar **
gst_debug_ring_buffer_logger_get_logs (void)
{
  gchar **logs, **tmp;
  GList *l;

  g_return_val_if_fail (ring_buffer_logger != NULL, NULL);

  G_LOCK (ring_buffer_logger);

  tmp = logs = g_new0 (gchar *, ring_buffer_logger->threads.length + 1);
  for (l = ring_buffer_logger->threads.head; l; l = l->next) {
    GstRingBufferLog *log = l->data;
    GList *l;
    gchar *p;
    gsize len;

    *tmp = p = g_new0 (gchar, log->log_size + 1);

    for (l = log->log.head; l; l = l->next) {
      len = strlen (l->data);
      memcpy (p, l->data, len);
      p += len;
    }

    tmp++;
  }

  G_UNLOCK (ring_buffer_logger);

  return logs;
}

static void
gst_ring_buffer_logger_free (GstRingBufferLogger * logger)
{
  G_LOCK (ring_buffer_logger);
  if (ring_buffer_logger == logger) {
    GstRingBufferLog *log;

    while ((log = g_queue_pop_head (&logger->threads))) {
      gchar *buf;
      while ((buf = g_queue_pop_head (&log->log)))
        g_free (buf);
      g_free (log);
    }

    g_hash_table_unref (logger->thread_index);

    g_free (logger);
    ring_buffer_logger = NULL;
  }
  G_UNLOCK (ring_buffer_logger);
}

/**
 * gst_debug_add_ring_buffer_logger:
 * @max_size_per_thread: Maximum size of log per thread in bytes
 * @thread_timeout: Timeout for threads in seconds
 *
 * Adds a memory ringbuffer based debug logger that stores up to
 * @max_size_per_thread bytes of logs per thread and times out threads after
 * @thread_timeout seconds of inactivity.
 *
 * Logs can be fetched with gst_debug_ring_buffer_logger_get_logs() and the
 * logger can be removed again with gst_debug_remove_ring_buffer_logger().
 * Only one logger at a time is possible.
 *
 * Since: 1.14
 */
void
gst_debug_add_ring_buffer_logger (guint max_size_per_thread,
    guint thread_timeout)
{
  GstRingBufferLogger *logger;

  G_LOCK (ring_buffer_logger);

  if (ring_buffer_logger) {
    g_warn_if_reached ();
    G_UNLOCK (ring_buffer_logger);
    return;
  }

  logger = ring_buffer_logger = g_new0 (GstRingBufferLogger, 1);

  logger->max_size_per_thread = max_size_per_thread;
  logger->thread_timeout = thread_timeout;
  logger->thread_index = g_hash_table_new (g_direct_hash, g_direct_equal);
  g_queue_init (&logger->threads);

  gst_debug_add_log_function (gst_ring_buffer_logger_log, logger,
      (GDestroyNotify) gst_ring_buffer_logger_free);
  G_UNLOCK (ring_buffer_logger);
}

/**
 * gst_debug_remove_ring_buffer_logger:
 *
 * Removes any previously added ring buffer logger with
 * gst_debug_add_ring_buffer_logger().
 *
 * Since: 1.14
 */
void
gst_debug_remove_ring_buffer_logger (void)
{
  gst_debug_remove_log_function (gst_ring_buffer_logger_log);
}

#else /* GST_DISABLE_GST_DEBUG */
#ifndef GST_REMOVE_DISABLED

gchar **
gst_debug_ring_buffer_logger_get_logs (void)
{
  return NULL;
}

void
gst_debug_add_ring_buffer_logger (guint max_size_per_thread,
    guint thread_timeout)
{
}

void
gst_debug_remove_ring_buffer_logger (void)
{
}

#endif /* GST_REMOVE_DISABLED */
#endif /* GST_DISABLE_GST_DEBUG */
