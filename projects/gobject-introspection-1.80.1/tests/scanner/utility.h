#ifndef __UTILITY_H__
#define __UTILITY_H__

#include <glib-object.h>

#include "gitestmacros.h"

#define UTILITY_TYPE_OBJECT              (utility_object_get_type ())
#define UTILITY_OBJECT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), UTILITY_TYPE_OBJECT, UtilityObject))
#define UTILITY_IS_OBJECT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), UTILITY_TYPE_OBJECT))

typedef struct _UtilityObject          UtilityObject;
typedef struct _UtilityObjectClass     UtilityObjectClass;

struct _UtilityObject
{
  GObject parent_instance;
};

struct _UtilityObjectClass
{
  GObjectClass parent_class;
};

/* This one is similar to Pango.Glyph */
typedef guint32 UtilityGlyph;

typedef struct
{
  int tag;
  union
  {
    gpointer v_pointer;
    double v_real;
    long v_integer;
  } value;
} UtilityTaggedValue;

typedef union
{
  guint8 value;
  struct
  {
    guint8 first_nibble : 4;
    guint8 second_nibble : 4;
  } parts;
} UtilityByte;

/* This one is similiar to Soup.Buffer */
typedef struct
{
  const char *data;
  gsize       length;
} UtilityBuffer;

typedef void (*UtilityFileFunc)(const char *path, gpointer user_data);


_GI_TEST_EXTERN
GType                 utility_object_get_type          (void) G_GNUC_CONST;

_GI_TEST_EXTERN
void                  utility_object_watch_dir         (UtilityObject *object,
                                                        const char *path,
                                                        UtilityFileFunc func,
                                                        gpointer user_data,
                                                        GDestroyNotify destroy);

typedef enum
{
  UTILITY_ENUM_A,
  UTILITY_ENUM_B,
  UTILITY_ENUM_C
} UtilityEnumType;

/* The shift operators here should imply bitfield */ 
typedef enum
{
  UTILITY_FLAG_A = 1 << 0,
  UTILITY_FLAG_B = 1 << 1,
  UTILITY_FLAG_C = 1 << 2
} UtilityFlagType;

typedef struct
{
  int field;
  guint bitfield1 : 3;
  guint bitfield2 : 2;
  guint8 data[16];
} UtilityStruct;

typedef union
{
  char *pointer;
  glong integer;
  double real;
} UtilityUnion;

_GI_TEST_EXTERN
void utility_dir_foreach (const char *path, UtilityFileFunc func, gpointer user_data);

#endif /* __UTILITY_H__ */
