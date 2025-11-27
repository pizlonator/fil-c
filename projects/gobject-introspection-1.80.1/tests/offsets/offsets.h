#ifndef __OFFSETS_H__
#define __OFFSETS_H__

#include <glib.h>
#include <glib-object.h>
#include <time.h>

#include "gitestmacros.h"

/* Test we get the alignment right for various basic types; we put
 * a char in front of of each field to make it more likely that we'll
 * stress out the alignment code.
 */
typedef struct _OffsetsBasic OffsetsBasic;

struct _OffsetsBasic {
  char dummy1;
  gint8 field_int8;
  char dummy2;
  gint16 field_int16;
  char dummy3;
  gint32 field_int32;
  char dummy4;
  gint64 field_int64;
  char dummy5;
  gchar *field_pointer;
  char dummy6;
  gfloat field_float;
  char dummy7;
  gdouble field_double;
  char dummy8;
  gsize field_size;
  char dummy9;
  guchar field_uchar1;
  char dummy10;
  unsigned char field_uchar2;
  char dummy11;
};

typedef enum {
  OFFSETS_ENUM_1 = 1 /* compiler could use int8, uint8, int16, uint16, int32, uint32 */
} OffsetsEnum1;

typedef enum {
  OFFSETS_ENUM_2 = 128 /* compiler could use uint8, int16, uint16, int32, uint32 */
} OffsetsEnum2;

typedef enum {
  OFFSETS_ENUM_3 = 257 /* compiler could use int16, uint16, int32, uint32 */
} OffsetsEnum3;

typedef enum {
  OFFSETS_ENUM_4 = G_MAXSHORT + 1 /* compiler could use uint16, int32, uint32 */
} OffsetsEnum4;

typedef enum {
  OFFSETS_ENUM_5 = G_MAXUSHORT + 1 /* compiler could use int32, uint32 */
} OffsetsEnum5;

typedef enum {
  OFFSETS_ENUM_6 = ((guint)G_MAXINT) + 1 /* compiler could use uint32 */
} OffsetsEnum6;

/* Test that we get the width of enum fields right. The char after
 * each field will have aligment 1 (almost certainly) so should
 * be placed right the field.
 */
typedef struct _OffsetsEnum OffsetsEnum;

struct _OffsetsEnum {
  OffsetsEnum1 enum1;
  char dummy1;
  OffsetsEnum2 enum2;
  char dummy2;
  OffsetsEnum3 enum3;
  char dummy3;
  OffsetsEnum4 enum4;
  char dummy4;
  OffsetsEnum5 enum5;
  char dummy5;
  OffsetsEnum6 enum6;
  char dummy6;
};

/* Test nested structures
 */

typedef struct _OffsetsNestee OffsetsNestee;

struct _OffsetsNestee {
  char field1;
  double field2; /* alignment of structure is greater than its first element */
  char field3; /* structure has tail padding */
};

typedef union _OffsetsNesteeUnion OffsetsNesteeUnion;

union _OffsetsNesteeUnion {
  char field1;
  double field2;
};

typedef struct _OffsetsNested OffsetsNested;

struct _OffsetsNested {
  char dummy1;
  OffsetsNestee nestee;
  char dummy2;
  OffsetsNesteeUnion nestee_union;
  char dummy3;
};

/* Test array offsets
 */

typedef struct _OffsetsArray OffsetsArray;

struct _OffsetsArray
{ 
  gint some_ints[2];
  gint8 some_int8s[3];
  gdouble some_doubles[4];
  OffsetsEnum1 some_enum[2];
  gpointer some_ptrs[5];
};

/* Test multi-dimensional arrays */

typedef struct _OffsetsMultiDimArray OffsetsMultiDimArray;

struct _OffsetsMultiDimArray
{
  gint ints[10][2];
  gchar chars[255][10];
  float floats[11][13][17];
  gchar* pointers1[3][5];
  gpointer pointers2[7][9];
  double** pointers3[2][3][4];
  gchar dummy;
};

/* Test object offsets */

typedef struct _OffsetsObj OffsetsObj;
typedef struct _OffsetsObjClass OffsetsObjClass;

struct _OffsetsObj
{
  GObject parent_instance;

  GObject *other;
};

struct _OffsetsObjClass
{
  GObjectClass parent_class;
};

#endif /* __OFFSETS_H__ */
