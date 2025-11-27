#include <glib.h>

#ifndef __CMPH_TYPES_H__
#define __CMPH_TYPES_H__

typedef gint8 cmph_int8;
typedef guint8 cmph_uint8;

typedef gint16 cmph_int16;
typedef guint16 cmph_uint16;

typedef gint32 cmph_int32;
typedef guint32 cmph_uint32;

typedef gint64 cmph_int64;
typedef guint64 cmph_uint64;

typedef enum { CMPH_HASH_JENKINS, CMPH_HASH_COUNT } CMPH_HASH;
extern const char *cmph_hash_names[];
typedef enum { CMPH_BMZ, CMPH_BMZ8, CMPH_CHM, CMPH_BRZ, CMPH_FCH,
               CMPH_BDZ, CMPH_BDZ_PH,
               CMPH_CHD_PH, CMPH_CHD, CMPH_COUNT } CMPH_ALGO;
extern const char *cmph_names[];

#endif
