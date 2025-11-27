/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* This file gets installed, so we can't assume config.h is available */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "annotation.h"

char backslash_parsing_tester = '\\';

G_DEFINE_TYPE (RegressAnnotationObject, regress_annotation_object, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_STRING_PROPERTY,
  PROP_FUNCTION_PROPERTY,
  PROP_TAB_PROPERTY
};

enum {
  STRING_SIGNAL,
  LIST_SIGNAL,
  DOC_EMPTY_ARG_PARSING,
  ATTRIBUTE_SIGNAL,
  LAST_SIGNAL
};

static guint regress_annotation_object_signals[LAST_SIGNAL] = { 0 };

static void
regress_annotation_object_set_property (GObject         *object,
                                        guint            prop_id,
                                        const GValue    *value,
                                        GParamSpec      *pspec)
{
  switch (prop_id)
    {
    case PROP_STRING_PROPERTY:
      break;
    case PROP_FUNCTION_PROPERTY:
      break;
    case PROP_TAB_PROPERTY:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
regress_annotation_object_get_property (GObject         *object,
                                        guint            prop_id,
                                        GValue          *value,
                                        GParamSpec      *pspec)
{
  switch (prop_id)
    {
    case PROP_STRING_PROPERTY:
      break;
    case PROP_FUNCTION_PROPERTY:
      break;
    case PROP_TAB_PROPERTY:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
regress_annotation_object_class_init (RegressAnnotationObjectClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = regress_annotation_object_set_property;
  gobject_class->get_property = regress_annotation_object_get_property;

  /**
   * RegressAnnotationObject::string-signal:
   * @regress_annotation: the regress_annotation object
   * @string: (type utf8): a string
   *
   * This is a signal which has a broken signal handler,
   * it says it's pointer but it's actually a string.
   *
   * Since: 1.0
   * Deprecated: 1.2: Use other-signal instead
   */
  regress_annotation_object_signals[STRING_SIGNAL] =
    g_signal_new ("string-signal",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  (GSignalCMarshaller)g_cclosure_marshal_VOID__POINTER,
		  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * RegressAnnotationObject::list-signal:
   * @regress_annotation: the regress_annotation object
   * @list: (type GLib.List) (element-type utf8) (transfer container): a list of strings
   *
   * This is a signal which takes a list of strings, but it's not
   * known by GObject as it's only marked as G_TYPE_POINTER
   */
  regress_annotation_object_signals[LIST_SIGNAL] =
    g_signal_new ("list-signal",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  (GSignalCMarshaller)g_cclosure_marshal_VOID__POINTER,
		  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * RegressAnnotationObject::doc-empty-arg-parsing:
   * @regress_annotation: the regress_annotation object
   * @arg1:
   *
   * This signal tests an empty document argument (@arg1)
   */
  regress_annotation_object_signals[DOC_EMPTY_ARG_PARSING] =
    g_signal_new ("doc-empty-arg-parsing",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  (GSignalCMarshaller)g_cclosure_marshal_VOID__POINTER,
		  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * RegressAnnotationObject::attribute-signal:
   * @regress_annotation: the regress_annotation object
   * @arg1: (attributes some.annotation.foo1=val1): a value
   * @arg2: (attributes some.annotation.foo2=val2): another value
   *
   * This signal tests a signal with attributes.
   *
   * Returns: (attributes some.annotation.foo3=val3): the return value
   */
  regress_annotation_object_signals[ATTRIBUTE_SIGNAL] =
    g_signal_new ("attribute-signal",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  NULL, /* marshaller */
		  G_TYPE_STRING,
                  2,
                  G_TYPE_STRING,
                  G_TYPE_STRING);

  /**
   * RegressAnnotationObject:string-property:
   *
   * This is a property which is a string
   *
   * Since: 1.0
   * Deprecated: 1.2: Use better-string-property instead
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STRING_PROPERTY,
                                   g_param_spec_string ("string-property",
                                                        "String property",
                                                        "This property is a string",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  /**
   * RegressAnnotationObject:function-property: (type RegressAnnotationCallback)
   */
  g_object_class_install_property (gobject_class,
                                   PROP_FUNCTION_PROPERTY,
                                   g_param_spec_pointer ("function-property",
                                                         "Function property",
                                                         "This property is a function pointer",
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	  /**
	   * RegressAnnotationObject:tab-property:
	   *
	   * This is a property annotation intentionally indented with a mix
	   * of tabs and strings to test the tab handling capabilities of the scanner.
	   *
	   * Since: 1.2
	   */
  g_object_class_install_property (gobject_class,
                                   PROP_TAB_PROPERTY,
                                   g_param_spec_string ("tab-property",
                                                        "Tab property",
                                                        "This property is a thing",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
regress_annotation_object_init (RegressAnnotationObject *object)
{

}

/**
 * regress_annotation_object_method:
 * @object: a #GObject
 *
 * Return value: an int
 **/
gint
regress_annotation_object_method (RegressAnnotationObject *object)
{
  return 1;
}

/**
 * regress_annotation_object_out:
 * @object: a #GObject
 * @outarg: (out): This is an argument test
 *
 * This is a test for out arguments
 *
 * Return value: an int
 */
gint
regress_annotation_object_out (RegressAnnotationObject *object, int *outarg)
{
  *outarg = 2;
  return 1;
}

/**
 * regress_annotation_object_in:
 * @object: a #GObject
 * @inarg: (in) (transfer none): This is an argument test
 *
 * This is a test for in arguments
 *
 * Return value: an int
 */
gint
regress_annotation_object_in (RegressAnnotationObject *object, int *inarg)
{
  return *inarg;
}


/**
 * regress_annotation_object_inout:
 * @object: a #GObject
 * @inoutarg: (inout): This is an argument test
 *
 * This is a test for out arguments
 *
 * Return value: an int
 */
gint
regress_annotation_object_inout (RegressAnnotationObject *object, int *inoutarg)
{
  return *inoutarg += 1;
}

/**
 * regress_annotation_object_inout2:
 * @object: a #GObject
 * @inoutarg: (inout): This is an argument test
 *
 * This is a second test for out arguments
 *
 * Return value: an int
 */
gint
regress_annotation_object_inout2 (RegressAnnotationObject *object, int *inoutarg)
{
  return *inoutarg += 1;
}


/**
 * regress_annotation_object_inout3:
 * @object: a #GObject
 * @inoutarg: (inout) (allow-none): This is an argument test
 *
 * This is a 3th test for out arguments
 *
 * Return value: an int
 */
gint
regress_annotation_object_inout3 (RegressAnnotationObject *object, int *inoutarg)
{
  if (inoutarg)
    return *inoutarg + 1;
  return 1;
}

/**
 * regress_annotation_object_calleeowns:
 * @object: a #GObject
 * @toown: (out): a #GObject
 *
 * This is a test for out arguments; GObject defaults to transfer
 *
 * Return value: an int
 */
gint
regress_annotation_object_calleeowns (RegressAnnotationObject *object, GObject **toown)
{
  return 1;
}


/**
 * regress_annotation_object_calleesowns:
 * @object: a #GObject
 * @toown1: (out) (transfer full): a #GObject
 * @toown2: (out) (transfer none): a #GObject
 *
 * This is a test for out arguments, one transferred, other not
 *
 * Return value: an int
 */
gint
regress_annotation_object_calleesowns (RegressAnnotationObject *object,
                                       GObject **toown1,
                                       GObject **toown2)
{
  return 1;
}


/**
 * regress_annotation_object_get_strings:
 * @object: a #GObject
 *
 * This is a test for returning a list of strings, where
 * each string needs to be freed.
 *
 * Return value: (element-type utf8) (transfer full): list of strings
 */
GList*
regress_annotation_object_get_strings (RegressAnnotationObject *object)
{
  GList *list = NULL;
  list = g_list_prepend (list, g_strdup ("regress_annotation"));
  list = g_list_prepend (list, g_strdup ("bar"));
  return list;
}

/**
 * regress_annotation_object_get_hash:
 * @object: a #GObject
 *
 * This is a test for returning a hash table mapping strings to
 * objects.
 *
 * Return value: (element-type utf8 GObject) (transfer full): hash table
 */
GHashTable*
regress_annotation_object_get_hash (RegressAnnotationObject *object)
{
  GHashTable *hash = g_hash_table_new_full (g_str_hash, g_str_equal,
					    g_free, g_object_unref);
  g_hash_table_insert (hash, g_strdup ("one"), g_object_ref (object));
  g_hash_table_insert (hash, g_strdup ("two"), g_object_ref (object));
  return hash;
}

/**
 * regress_annotation_object_with_voidp:
 * @data: Opaque pointer handle
 */
void
regress_annotation_object_with_voidp (RegressAnnotationObject *object, void *data)
{

}

/**
 * regress_annotation_object_get_objects:
 * @object: a #GObject
 *
 * This is a test for returning a list of objects.
 * The list itself should be freed, but not the internal objects,
 * intentionally similar example to gtk_container_get_children
 *
 * Return value: (element-type RegressAnnotationObject) (transfer container): list of objects
 */
GSList*
regress_annotation_object_get_objects (RegressAnnotationObject *object)
{
  GSList *list = NULL;
  list = g_slist_prepend (list, object);
  return list;
}

/**
 * regress_annotation_object_create_object:
 * @object: a #GObject
 *
 * Test returning a caller-owned object
 *
 * Return value: (transfer full): The object
 **/
GObject*
regress_annotation_object_create_object (RegressAnnotationObject *object)
{
  return G_OBJECT (g_object_ref (object));
}

/**
 * regress_annotation_object_use_buffer:
 * @object: a #GObject
 *
 **/
void
regress_annotation_object_use_buffer   (RegressAnnotationObject *object,
                                        guchar           *bytes)
{

}

/**
 * regress_annotation_object_compute_sum:
 * @object: a #GObject
 * @nums: (array): Sequence of numbers
 *
 * Test taking a zero-terminated array
 **/
void
regress_annotation_object_compute_sum  (RegressAnnotationObject *object,
                                        int              *nums)
{

}

/**
 * regress_annotation_object_compute_sum_n:
 * @object: a #GObject
 * @nums: (array length=n_nums zero-terminated=0): Sequence of
 *   numbers that are zero-terminated
 * @n_nums: Length of number array
 *
 * Test taking an array with length parameter
 **/
void
regress_annotation_object_compute_sum_n(RegressAnnotationObject *object,
                                        int              *nums,
                                        int               n_nums)
{

}

/**
 * regress_annotation_object_compute_sum_nz:
 * @object: a #RegressAnnotationObject
 * @nums: (array length=n_nums zero-terminated): Sequence of numbers that
 * are zero-terminated
 * @n_nums: Length of number array
 *
 * Test taking a zero-terminated array with length parameter
 **/
void
regress_annotation_object_compute_sum_nz(RegressAnnotationObject *object,
                                         int             *nums,
                                         int              n_nums)
{

}

/**
 * regress_annotation_object_parse_args:
 * @object: a #RegressAnnotationObject
 * @argc: (inout): Length of the argument vector
 * @argv: (inout) (array length=argc zero-terminated=1): Argument vector
 *
 * Test taking a zero-terminated array with length parameter
 **/
void
regress_annotation_object_parse_args(RegressAnnotationObject *object,
                                     int              *argc,
                                     char           ***argv)
{

}

/**
 * regress_annotation_object_string_out:
 * @object: a #RegressAnnotationObject
 * @str_out: (out) (transfer full): string return value
 *
 * Test returning a string as an out parameter
 *
 * Returns: some boolean
 **/
gboolean
regress_annotation_object_string_out(RegressAnnotationObject *object,
                                     char            **str_out)
{
  return FALSE;
}

/**
 * regress_annotation_object_foreach:
 * @object: a #RegressAnnotationObject
 * @func: (scope call): Callback to invoke
 * @user_data: Callback user data
 *
 * Test taking a call-scoped callback
 **/
void
regress_annotation_object_foreach (RegressAnnotationObject *object,
                                   RegressAnnotationForeachFunc func,
                                   gpointer user_data)
{

}

/**
 * regress_annotation_object_set_data:
 * @object: a #RegressAnnotationObject
 * @data: (array length=length): The data
 * @length: Length of the data
 *
 * Test taking a guchar * with a length.
 **/
void
regress_annotation_object_set_data (RegressAnnotationObject *object,
                                    const guchar *data,
                                    gsize length)
{

}

/**
 * regress_annotation_object_set_data2:
 * @object: a #RegressAnnotationObject
 * @data: (array length=length) (element-type gint8): The data
 * @length: Length of the data
 *
 * Test taking a gchar * with a length.
 **/
void
regress_annotation_object_set_data2 (RegressAnnotationObject *object,
                                     const gchar *data,
                                     gsize length)
{

}

/**
 * regress_annotation_object_set_data3:
 * @object: a #RegressAnnotationObject
 * @data: (array length=length) (element-type guint8): The data
 * @length: Length of the data
 *
 * Test taking a gchar * with a length, overriding the array element
 * type.
 **/
void
regress_annotation_object_set_data3 (RegressAnnotationObject *object,
                                     gpointer data,
                                     gsize length)
{

}

/**
 * regress_annotation_object_allow_none:
 * @object: a #GObject
 * @somearg: (allow-none):
 *
 * Returns: (transfer none): %NULL always
 **/
GObject*
regress_annotation_object_allow_none (RegressAnnotationObject *object, const gchar *somearg)
{
  return NULL;
}

/**
 * regress_annotation_object_notrans:
 * @object: a #GObject
 *
 * Returns: (transfer none): An object, not referenced
 **/

GObject*
regress_annotation_object_notrans (RegressAnnotationObject *object)
{
  return NULL;
}

/**
 * regress_annotation_object_do_not_use:
 * @object: a #GObject
 *
 * Returns: (transfer none): %NULL always
 * Deprecated: 0.12: Use regress_annotation_object_create_object() instead.
 **/
GObject*
regress_annotation_object_do_not_use (RegressAnnotationObject *object)
{
  return NULL;
}

/**
 * regress_annotation_object_watch: (skip)
 * @object: A #RegressAnnotationObject
 * @func: The callback
 * @user_data: The callback data
 *
 * This is here just for the sake of being overriden by its
 * regress_annotation_object_watch_full().
 */
void
regress_annotation_object_watch (RegressAnnotationObject *object,
                                 RegressAnnotationForeachFunc func,
                                 gpointer user_data)
{
}

/**
 * regress_annotation_object_watch_full: (rename-to regress_annotation_object_watch)
 * @object: A #RegressAnnotationObject
 * @func: The callback
 * @user_data: The callback data
 * @destroy: Destroy notification
 *
 * Test overriding via the "Rename To" annotation.
 */
void
regress_annotation_object_watch_full (RegressAnnotationObject *object,
                                      RegressAnnotationForeachFunc func,
                                      gpointer user_data,
                                      GDestroyNotify destroy)
{
}

/**
 * regress_annotation_object_hidden_self:
 * @object: (type RegressAnnotationObject): A #RegressAnnotationObject
 **/
void
regress_annotation_object_hidden_self (gpointer object)
{
}

/**
 * regress_annotation_init:
 * @argc: (inout): The number of args.
 * @argv: (inout) (array length=argc): The arguments.
 **/
void
regress_annotation_init (int *argc, char ***argv)
{

}

/**
 * regress_annotation_return_array:
 * @length: (out): Number of return values
 *
 * Return value: (transfer full) (array length=length): The return value
 **/
char **
regress_annotation_return_array (int *length)
{
  return NULL;
}

/**
 * regress_annotation_string_zero_terminated:
 *
 * Return value: (transfer full) (array zero-terminated=1): The return value
 **/
char **
regress_annotation_string_zero_terminated (void)
{
  return NULL;
}

/**
 * regress_annotation_string_zero_terminated_out:
 * @out: (array zero-terminated=1) (inout):
 **/
void
regress_annotation_string_zero_terminated_out (char ***out)
{
}

/**
 * regress_annotation_versioned:
 *
 * Since: 0.6
 **/
void
regress_annotation_versioned (void)
{
}

/**
 * regress_annotation_string_array_length:
 * @n_properties:
 * @properties: (array length=n_properties) (element-type utf8):
 */
void
regress_annotation_string_array_length (guint n_properties, const gchar * const properties[])
{
}

/**
 * regress_annotation_object_extra_annos: (attributes org.foobar=testvalue)
 */
void
regress_annotation_object_extra_annos (RegressAnnotationObject *object)
{
}

/**
 * regress_annotation_custom_destroy:
 * @callback: (destroy destroy) (closure data): Destroy notification
 *
 * Test messing up the heuristic of closure/destroy-notification
 * detection, and fixing it via annotations.
 */
void
regress_annotation_custom_destroy (RegressAnnotationCallback callback,
                                   RegressAnnotationNotifyFunc destroy,
                                   gpointer data)
{
}

/**
 * regress_annotation_get_source_file:
 *
 * Return value: (type filename) (transfer full): Source file
 */
char *
regress_annotation_get_source_file (void)
{
  return NULL;
}

/**
 * regress_annotation_set_source_file:
 * @fname: (type filename): Source file
 *
 */
void
regress_annotation_set_source_file (const char *fname)
{
}

/**
 * regress_annotation_ptr_array:
 * @array: (element-type GObject.Value): the array
 */
void
regress_annotation_ptr_array (GPtrArray *array)
{
}

/**
 * regress_annotation_attribute_func:
 * @object: A #RegressAnnotationObject.
 * @data: (attributes some.annotation=value another.annotation=blahvalue): Some data.
 *
 * Returns: (attributes some.other.annotation=value2 yet.another.annotation=another_value): The return value.
 */
gint
regress_annotation_attribute_func (RegressAnnotationObject *object,
                                   const gchar      *data)
{
  return 42;
}

/**
 * regress_annotation_invalid_regress_annotation:
 * @foo: some text (e.g. example) or else
 */
void
regress_annotation_invalid_regress_annotation (int foo)
{

}


char backslash_parsing_tester_2 = '\\';


/**
 * regress_annotation_test_parsing_bug630862:
 *
 * See https://bugzilla.gnome.org/show_bug.cgi?id=630862
 *
 * Returns: (transfer none): An object, note the colon:in here
 */
GObject  *
regress_annotation_test_parsing_bug630862 (void)
{
  return NULL;
}


/**
 * regress_annotation_space_after_comment_bug631690:
 *
 * Explicitly test having a space after the ** here.
 */
void
regress_annotation_space_after_comment_bug631690 (void)
{
}

/**
 * regress_annotation_return_filename:
 *
 * Returns: (type filename): An annotated filename
 */
gchar*
regress_annotation_return_filename (void)
{
  return g_strdup ("a utf-8 filename");
}

/**
 * regress_annotation_transfer_floating:
 * @object: (in) (transfer floating): an object
 *
 * Returns: (transfer floating): A floating object
 */
GObject *
regress_annotation_transfer_floating (GObject *object)
{
  return NULL;
}
