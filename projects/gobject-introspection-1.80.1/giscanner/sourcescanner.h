/* GObject introspection: public scanner api
 *
 * Copyright (C) 2007 Jürg Billeter
 * Copyright (C) 2008 Johan Dahlin
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef __SOURCE_SCANNER_H__
#define __SOURCE_SCANNER_H__

#include <glib.h>
#include <stdio.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _GISourceComment GISourceComment;
typedef struct _GISourceScanner GISourceScanner;
typedef struct _GISourceSymbol GISourceSymbol;
typedef struct _GISourceType GISourceType;

typedef enum
{
  CSYMBOL_TYPE_INVALID,
  CSYMBOL_TYPE_ELLIPSIS,
  CSYMBOL_TYPE_CONST,
  CSYMBOL_TYPE_OBJECT,
  CSYMBOL_TYPE_FUNCTION,
  CSYMBOL_TYPE_FUNCTION_MACRO,
  CSYMBOL_TYPE_STRUCT,
  CSYMBOL_TYPE_UNION,
  CSYMBOL_TYPE_ENUM,
  CSYMBOL_TYPE_TYPEDEF,
  CSYMBOL_TYPE_MEMBER
} GISourceSymbolType;

typedef enum
{
  CTYPE_INVALID,
  CTYPE_VOID,
  CTYPE_BASIC_TYPE,
  CTYPE_TYPEDEF,
  CTYPE_STRUCT,
  CTYPE_UNION,
  CTYPE_ENUM,
  CTYPE_POINTER,
  CTYPE_ARRAY,
  CTYPE_FUNCTION
} GISourceTypeType;

typedef enum
{
  STORAGE_CLASS_NONE = 0,
  STORAGE_CLASS_TYPEDEF = 1 << 1,
  STORAGE_CLASS_EXTERN = 1 << 2,
  STORAGE_CLASS_STATIC = 1 << 3,
  STORAGE_CLASS_AUTO = 1 << 4,
  STORAGE_CLASS_REGISTER = 1 << 5,
  STORAGE_CLASS_THREAD_LOCAL = 1 << 6
} StorageClassSpecifier;

typedef enum
{
  TYPE_QUALIFIER_NONE = 0,
  TYPE_QUALIFIER_CONST = 1 << 1,
  TYPE_QUALIFIER_RESTRICT = 1 << 2,
  TYPE_QUALIFIER_VOLATILE = 1 << 3,
  TYPE_QUALIFIER_EXTENSION = 1 << 4
} TypeQualifier;

typedef enum
{
  FUNCTION_NONE = 0,
  FUNCTION_INLINE = 1 << 1
} FunctionSpecifier;

typedef enum
{
  UNARY_ADDRESS_OF,
  UNARY_POINTER_INDIRECTION,
  UNARY_PLUS,
  UNARY_MINUS,
  UNARY_BITWISE_COMPLEMENT,
  UNARY_LOGICAL_NEGATION
} UnaryOperator;

struct _GISourceComment
{
  char *comment;
  char *filename;
  int line;
};

struct _GISourceScanner
{
  GFile *current_file;
  gboolean macro_scan;
  gboolean private; /* set by gtk-doc comment <private>/<public> */
  gboolean flags; /* set by gtk-doc comment <flags> */
  GPtrArray *symbols; /* GISourceSymbol */
  GHashTable *files;
  GPtrArray *comments; /* GISourceComment */
  GHashTable *typedef_table;
  GHashTable *const_table;
  gboolean skipping;
  GQueue conditionals;
  GPtrArray *errors;
};

struct _GISourceSymbol
{
  int ref_count;
  GISourceSymbolType type;
  char *ident;
  GISourceType *base_type;
  gboolean const_int_set;
  gboolean private;
  gint64 const_int; /* 64-bit we can handle signed and unsigned 32-bit values */
  gboolean const_int_is_unsigned;
  char *const_string;
  gboolean const_double_set;
  double const_double;
  gboolean const_boolean_set;
  int const_boolean;
  char *source_filename;
  int line;
};

struct _GISourceType
{
  GISourceTypeType type;
  StorageClassSpecifier storage_class_specifier;
  TypeQualifier type_qualifier;
  FunctionSpecifier function_specifier;
  char *name;
  GISourceType *base_type;
  GList *child_list; /* list of GISourceSymbol */
  gboolean is_bitfield;
};

GISourceScanner *   gi_source_scanner_new              (void);
gboolean            gi_source_scanner_lex_filename     (GISourceScanner  *igenerator,
						        const gchar      *filename);
gboolean            gi_source_scanner_parse_file       (GISourceScanner  *igenerator,
						        const gchar      *filename);
void                gi_source_scanner_parse_macros     (GISourceScanner  *scanner,
							GList            *filenames);
void                gi_source_scanner_set_macro_scan   (GISourceScanner  *scanner,
							gboolean          macro_scan);
GPtrArray *         gi_source_scanner_get_symbols      (GISourceScanner  *scanner);
GPtrArray *         gi_source_scanner_get_comments     (GISourceScanner  *scanner);
GPtrArray *         gi_source_scanner_get_errors       (GISourceScanner  *scanner);
void                gi_source_scanner_free             (GISourceScanner  *scanner);

GISourceSymbol *    gi_source_symbol_new               (GISourceSymbolType  type, GFile *file, int line);
gboolean            gi_source_symbol_get_const_boolean (GISourceSymbol     *symbol);
GISourceSymbol *    gi_source_symbol_ref               (GISourceSymbol     *symbol);
void                gi_source_symbol_unref             (GISourceSymbol     *symbol);
GISourceSymbol *    gi_source_symbol_copy              (GISourceSymbol     *symbol);

/* Private */
void                gi_source_scanner_add_symbol       (GISourceScanner  *scanner,
							GISourceSymbol   *symbol);
void                gi_source_scanner_take_comment     (GISourceScanner *scanner,
                                                        GISourceComment *comment);
gboolean            gi_source_scanner_is_typedef       (GISourceScanner  *scanner,
							const char       *name);
void                gi_source_symbol_merge_type        (GISourceSymbol   *symbol,
							GISourceType     *type);
GISourceType *      gi_source_type_new                 (GISourceTypeType  type);
GISourceType *      gi_source_type_copy                (GISourceType     *type);
GISourceType *      gi_source_basic_type_new           (const char       *name);
GISourceType *      gi_source_typedef_new              (const char       *name);
GISourceType * 	    gi_source_struct_new               (const char 	 *name);
GISourceType * 	    gi_source_union_new 	       (const char 	 *name);
GISourceType * 	    gi_source_enum_new 		       (const char 	 *name);
GISourceType * 	    gi_source_pointer_new 	       (GISourceType     *base_type);
GISourceType * 	    gi_source_array_new                (GISourceSymbol   *size);
GISourceType * 	    gi_source_function_new             (void);

void ctype_free (GISourceType * type);

G_END_DECLS

#endif /* __SOURCE_SCANNER_H__ */
