#ifndef __REGRESS_ANNOTATION_OBJECT_H__
#define __REGRESS_ANNOTATION_OBJECT_H__

#include "gitestmacros.h"

#include <glib-object.h>

typedef enum /*< flags,prefix=ANN >*/
{
  ANN_FLAG_FOO = 1,
  ANN_FLAG_BAR = 2,
  /**
   * ANN_FLAG_FOOBAR:
   *
   * Since: 1.4
   */
  ANN_FLAG_FOOBAR = 3,
} RegressAnnotationBitfield;

/**
 * RegressAnnotationCallback:
 * @in: (in) (transfer none): array of ints
 *
 * This is a callback.
 * Return value: (transfer none): array of ints
 */
typedef const gint* (*RegressAnnotationCallback) (const gint *in);

/**
 * RegressAnnotationListCallback:
 * @in: (in) (transfer none) (element-type utf8): list of strings
 *
 * This is a callback taking a list.
 * Return value: (transfer container) (element-type utf8): list of strings
 */
typedef GList* (*RegressAnnotationListCallback) (GList *in);

/**
 * RegressAnnotationNotifyFunc:
 * @data: (closure): The user data
 *
 * This is a callback with a 'closure' argument that is not named
 * 'user_data' and hence has to be annotated.
 */
typedef void (*RegressAnnotationNotifyFunc) (gpointer data);

/**
 * RegressAnnotationObject: (attributes org.example.Test=cows)
 *
 * This is an object used to test annotations.
 */
typedef struct _RegressAnnotationObject          RegressAnnotationObject;
typedef struct _RegressAnnotationObjectClass     RegressAnnotationObjectClass;

typedef void (*RegressAnnotationForeachFunc) (RegressAnnotationObject *object,
                                       const char *item,
                                       gpointer user_data);

struct _RegressAnnotationObject
{
  GObject parent_instance;
};

struct _RegressAnnotationObjectClass
{
  GObjectClass parent_class;
};

_GI_TEST_EXTERN
GType    regress_annotation_object_get_type (void);

_GI_TEST_EXTERN
gint     regress_annotation_object_method       (RegressAnnotationObject *object);

_GI_TEST_EXTERN
gint     regress_annotation_object_out          (RegressAnnotationObject *object,
					 int              *outarg);

_GI_TEST_EXTERN
GObject* regress_annotation_object_create_object(RegressAnnotationObject *object);

_GI_TEST_EXTERN
GObject* regress_annotation_object_allow_none   (RegressAnnotationObject *object,
					 const gchar      *somearg);

_GI_TEST_EXTERN
GObject* regress_annotation_object_notrans      (RegressAnnotationObject *object);

_GI_TEST_EXTERN
gint     regress_annotation_object_inout        (RegressAnnotationObject *object,
					 int              *inoutarg);

_GI_TEST_EXTERN
gint     regress_annotation_object_inout2       (RegressAnnotationObject *object,
					 int              *inoutarg);

_GI_TEST_EXTERN
gint     regress_annotation_object_inout3       (RegressAnnotationObject *object,
					 int              *inoutarg);

_GI_TEST_EXTERN
gint     regress_annotation_object_in           (RegressAnnotationObject *object,
					 int              *inarg);

_GI_TEST_EXTERN
gint     regress_annotation_object_calleeowns   (RegressAnnotationObject *object,
					 GObject          **toown);

_GI_TEST_EXTERN
gint     regress_annotation_object_calleesowns  (RegressAnnotationObject *object,
					 GObject          **toown1,
					 GObject          **toown2);

_GI_TEST_EXTERN
GList*   regress_annotation_object_get_strings  (RegressAnnotationObject *object);

_GI_TEST_EXTERN
GHashTable*regress_annotation_object_get_hash   (RegressAnnotationObject *object);

_GI_TEST_EXTERN
void     regress_annotation_object_with_voidp   (RegressAnnotationObject *object,
					 void             *data);

_GI_TEST_EXTERN
GSList*  regress_annotation_object_get_objects  (RegressAnnotationObject *object);


_GI_TEST_EXTERN
void     regress_annotation_object_use_buffer   (RegressAnnotationObject *object,
					 guchar           *bytes);


_GI_TEST_EXTERN
void     regress_annotation_object_compute_sum  (RegressAnnotationObject *object,
					 int              *nums);


_GI_TEST_EXTERN
void     regress_annotation_object_compute_sum_n(RegressAnnotationObject *object,
					 int              *nums,
					 int               n_nums);

_GI_TEST_EXTERN
void     regress_annotation_object_compute_sum_nz(RegressAnnotationObject *object,
                                          int             *nums,
                                          int              n_nums);

_GI_TEST_EXTERN
void     regress_annotation_object_parse_args   (RegressAnnotationObject *object,
                                         int              *argc,
                                         char           ***argv);

_GI_TEST_EXTERN
gboolean regress_annotation_object_string_out   (RegressAnnotationObject *object,
                                         char            **str_out);

_GI_TEST_EXTERN
void     regress_annotation_object_foreach      (RegressAnnotationObject *object,
                                         RegressAnnotationForeachFunc func,
                                         gpointer user_data);


_GI_TEST_EXTERN
void     regress_annotation_object_set_data     (RegressAnnotationObject *object,
                                         const guchar     *data,
                                         gsize             length);

_GI_TEST_EXTERN
void     regress_annotation_object_set_data2    (RegressAnnotationObject *object,
                                         const gchar      *data,
                                         gsize             length);

_GI_TEST_EXTERN
void     regress_annotation_object_set_data3    (RegressAnnotationObject *object,
                                         gpointer          data,
                                         gsize             length);


_GI_TEST_EXTERN
GObject* regress_annotation_object_do_not_use   (RegressAnnotationObject *object);

_GI_TEST_EXTERN
void     regress_annotation_object_watch        (RegressAnnotationObject *object,
                                         RegressAnnotationForeachFunc func,
                                         gpointer user_data);

_GI_TEST_EXTERN
void     regress_annotation_object_watch_full   (RegressAnnotationObject *object,
                                         RegressAnnotationForeachFunc func,
                                         gpointer user_data,
                                         GDestroyNotify destroy);

_GI_TEST_EXTERN
void     regress_annotation_object_hidden_self  (gpointer object);


_GI_TEST_EXTERN
void     regress_annotation_init                (int              *argc, 
					 char           ***argv);

_GI_TEST_EXTERN
char **  regress_annotation_return_array        (int             *length);

_GI_TEST_EXTERN
void     regress_annotation_versioned           (void);

_GI_TEST_EXTERN
char **  regress_annotation_string_zero_terminated (void);

_GI_TEST_EXTERN
void     regress_annotation_string_zero_terminated_out (char ***out);


_GI_TEST_EXTERN
void     regress_annotation_string_array_length (guint n_properties, const gchar * const properties[]);


_GI_TEST_EXTERN
void     regress_annotation_object_extra_annos (RegressAnnotationObject *object);


_GI_TEST_EXTERN
void     regress_annotation_custom_destroy (RegressAnnotationCallback callback,
                                    RegressAnnotationNotifyFunc destroy,
                                    gpointer data);

_GI_TEST_EXTERN
char *   regress_annotation_get_source_file (void);

_GI_TEST_EXTERN
void     regress_annotation_set_source_file (const char *fname);


_GI_TEST_EXTERN
gint     regress_annotation_attribute_func (RegressAnnotationObject *object,
                                    const gchar      *data);


_GI_TEST_EXTERN
void     regress_annotation_invalid_regress_annotation (int foo);

/**
 * RegressAnnotationStruct:
 *
 * This is a test of an array of object in an field of a struct.
 */
struct RegressAnnotationStruct
{
  RegressAnnotationObject *objects[10];
};

/**
 * RegressAnnotationFields:
 * @field1: Some documentation
 * @arr: (array length=len): an array of length @len
 * @len: the length of array
 *
 * This is a struct for testing field documentation and annotations
 */
struct RegressAnnotationFields
{
  int field1;
  guchar *arr;
  gulong len;
  /**
   * RegressAnnotationFields.field4:
   *
   * A new field, breaking ABI is fun!
   *
   * Since: 1.4
   */
  guint field4;
};

_GI_TEST_EXTERN
void    regress_annotation_ptr_array (GPtrArray *array);


_GI_TEST_EXTERN
GObject  * regress_annotation_test_parsing_bug630862 (void);


_GI_TEST_EXTERN
void regress_annotation_space_after_comment_bug631690 (void);


_GI_TEST_EXTERN
gchar* regress_annotation_return_filename (void);


_GI_TEST_EXTERN
GObject * regress_annotation_transfer_floating (GObject *object);

/* This one we can handle properly */
#define REGRESS_ANNOTATION_CALCULATED_DEFINE (10 * 10)

/**
 * REGRESS_ANNOTATION_CALCULATED_LARGE: (value 10000000000UL)
 *
 * Constant to define a calculated large value
 *
 * Since: 1.4
 */
#define REGRESS_ANNOTATION_CALCULATED_LARGE (1000 * G_GINT64_CONSTANT (10000000))

/**
 * REGRESS_ANNOTATION_CALCULATED_LARGE_DIV: (value 1000000UL)
 *
 * Constant to define a calculated large value
 */
#define REGRESS_ANNOTATION_CALCULATED_LARGE_DIV (1000 / G_GINT64_CONSTANT (10000000))

#endif /* __REGRESS_ANNOTATION_OBJECT_H__ */

