/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: struct definitions for the binary
 * typelib format, validation
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008,2009 Red Hat, Inc.
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
 */

#ifndef __G_TYPELIB_H__
#define __G_TYPELIB_H__

#include <gmodule.h>
#include "girepository.h"

G_BEGIN_DECLS

/**
 * SECTION:gitypelib-internal
 * @title: GITypelib Internals
 * @short_description: Layout and accessors for typelib
 * @stability: Stable
 *
 * The "typelib" is a binary, readonly, memory-mappable database
 * containing reflective information about a GObject library.
 *
 * What the typelib describes and the types used are the same for every
 * platform so, apart the endianness of its scalar values, the typelib
 * database must be considered architecture-independent.
 *
 * The format of GObject typelib is strongly influenced by the Mozilla XPCOM
 * format.
 *
 * Some of the differences to XPCOM include:
 *
 * - Type information is stored not quite as compactly (XPCOM stores it inline
 *   in function descriptions in variable-sized blobs of 1 to n bytes. We store
 *   16 bits of type information for each parameter, which is enough to encode
 *   simple types inline. Complex (e.g. recursive) types are stored out of line
 *   in a separate list of types.
 * - String and complex type data is stored outside of typelib entry blobs,
 *   references are stored as offsets relative to the start of the typelib.
 *   One possibility is to store the strings and types in a pools at the end
 *   of the typelib.
 *
 * The typelib has the following general format:
 *
 * |[<!-- language="C" -->
 *   typelib ::= header, section-index, directory, blobs, attributes, attributedata
 *
 *   directory ::= list of entries
 *
 *   entry ::= blob type, name, namespace, offset
 *   blob ::= function|callback|struct|boxed|enum|flags|object|interface|constant|union
 *   attribute ::= offset, key, value
 *   attributedata ::= string data for attributes
 * ]|
 *
 * Details
 *
 * We describe the fragments that make up the typelib in the form of C structs
 * (although some fall short of being valid C structs since they contain
 * multiple flexible arrays).
 */

/*
TYPELIB HISTORY
-----

Version 1.1
- Add ref/unref/set-value/get-value functions to Object, to be able
  to support instantiatable fundamental types which are not GObject based.

Version 1.0
- Rename class_struct to gtype_struct, add to interfaces

Changes since 0.9:
- Add padding to structures

Changes since 0.8:
- Add class struct concept to ObjectBlob
- Add is_class_struct bit to StructBlob

Changes since 0.7:
- Add dependencies

Changes since 0.6:
- rename metadata to typelib, to follow xpcom terminology

Changes since 0.5:
- basic type cleanup:
  + remove GString
  + add [u]int, [u]long, [s]size_t
  + rename string to utf8, add filename
- allow blob_type to be zero for non-local entries

Changes since 0.4:
- add a UnionBlob

Changes since 0.3:
- drop short_name for ValueBlob

Changes since 0.2:
- make inline types 4 bytes after all, remove header->types and allow
  types to appear anywhere
- allow error domains in the directory

Changes since 0.1:

- drop comments about _GOBJ_METADATA
- drop string pool, strings can appear anywhere
- use 'blob' as collective name for the various blob types
- rename 'type' field in blobs to 'blob_type'
- rename 'type_name' and 'type_init' fields to 'gtype_name', 'gtype_init'
- shrink directory entries to 12 bytes
- merge struct and boxed blobs
- split interface blobs into enum, object and interface blobs
- add an 'unregistered' flag to struct and enum blobs
- add a 'wraps_vfunc' flag to function blobs and link them to
  the vfuncs they wrap
- restrict value blobs to only occur inside enums and flags again
- add constant blobs, allow them toplevel, in interfaces and in objects
- rename 'receiver_owns_value' and 'receiver_owns_container' to
  'transfer_ownership' and 'transfer_container_ownership'
- add a 'struct_offset' field to virtual function and field blobs
- add 'dipper' and 'optional' flags to arg blobs
- add a 'true_stops_emit' flag to signal blobs
- add variable blob sizes to header
- store offsets to signature blobs instead of including them directly
- change the type offset to be measured in words rather than bytes
*/

/**
 * G_IR_MAGIC:
 *
 * Identifying prefix for the typelib.  This was inspired by XPCOM,
 * which in turn borrowed from PNG.
 */
#define G_IR_MAGIC "GOBJ\nMETADATA\r\n\032"

/**
 * GTypelibBlobType:
 * @BLOB_TYPE_INVALID: Should not appear in code
 * @BLOB_TYPE_FUNCTION: A #FunctionBlob
 * @BLOB_TYPE_CALLBACK: A #CallbackBlob
 * @BLOB_TYPE_STRUCT: A #StructBlob
 * @BLOB_TYPE_BOXED: Can be either a #StructBlob or #UnionBlob
 * @BLOB_TYPE_ENUM: An #EnumBlob
 * @BLOB_TYPE_FLAGS: An #EnumBlob
 * @BLOB_TYPE_OBJECT: An #ObjectBlob
 * @BLOB_TYPE_INTERFACE: An #InterfaceBlob
 * @BLOB_TYPE_CONSTANT: A #ConstantBlob
 * @BLOB_TYPE_INVALID_0: Deleted, used to be ErrorDomain.
 * @BLOB_TYPE_UNION: A #UnionBlob
 *
 * The integral value of this enumeration appears in each "Blob" component of
 * a typelib to identify its type.
 */
typedef enum {
  BLOB_TYPE_INVALID,
  BLOB_TYPE_FUNCTION,
  BLOB_TYPE_CALLBACK,
  BLOB_TYPE_STRUCT,
  BLOB_TYPE_BOXED,
  BLOB_TYPE_ENUM,
  BLOB_TYPE_FLAGS,
  BLOB_TYPE_OBJECT,
  BLOB_TYPE_INTERFACE,
  BLOB_TYPE_CONSTANT,
  BLOB_TYPE_INVALID_0,
  BLOB_TYPE_UNION
} GTypelibBlobType;


#if defined (G_CAN_INLINE) && defined (G_ALWAYS_INLINE)

G_ALWAYS_INLINE
inline gboolean
_blob_is_registered_type (GTypelibBlobType blob_type)
{
  switch (blob_type)
    {
      case BLOB_TYPE_STRUCT:
      case BLOB_TYPE_UNION:
      case BLOB_TYPE_ENUM:
      case BLOB_TYPE_FLAGS:
      case BLOB_TYPE_OBJECT:
      case BLOB_TYPE_INTERFACE:
        return TRUE;
      default:
        return FALSE;
    }
}

#define BLOB_IS_REGISTERED_TYPE(blob) \
  _blob_is_registered_type ((GTypelibBlobType) (blob)->blob_type)

#else

#define BLOB_IS_REGISTERED_TYPE(blob)               \
        ((blob)->blob_type == BLOB_TYPE_STRUCT ||   \
         (blob)->blob_type == BLOB_TYPE_UNION  ||   \
         (blob)->blob_type == BLOB_TYPE_ENUM   ||   \
         (blob)->blob_type == BLOB_TYPE_FLAGS  ||   \
         (blob)->blob_type == BLOB_TYPE_OBJECT ||   \
         (blob)->blob_type == BLOB_TYPE_INTERFACE)

#endif /* defined (G_CAN_INLINE) && defined (G_ALWAYS_INLINE) */

/**
 * Header:
 * @magic: See #G_IR_MAGIC.
 * @major_version: The major version number of the typelib format. Major version
 *   number changes indicate incompatible changes to the tyeplib format.
 * @minor_version: The minor version number of the typelib format. Minor version
 *   number changes indicate compatible changes and should still allow the
 *   typelib to be parsed by a parser designed for the same @major_version.
 * @reserved: Reserved for future use.
 * @n_entries: The number of entries in the directory.
 * @n_local_entries: The number of entries referring to blobs in this typelib.
 *   The local entries must occur before the unresolved entries.
 * @directory: Offset of the directory in the typelib.
 * @n_attributes: Number of attribute blocks
 * @attributes: Offset of the list of attributes in the typelib.
 * @dependencies: Offset of a single string, which is the list of immediate
 *   dependencies, separated by the '|' character.  The dependencies are
 *   required in order to avoid having programs consuming a typelib check for
 *   an "Unresolved" type return from every API call.
 * @size: The size in bytes of the typelib.
 * @namespace: Offset of the namespace string in the typelib.
 * @nsversion: Offset of the namespace version string in the typelib.
 * @shared_library: This field is the set of shared libraries associated with
 *   the typelib.  The entries are separated by the '|' (pipe) character.
 * @c_prefix: The prefix for the function names of the library
 * @entry_blob_size: The sizes of fixed-size blobs. Recording this information
 *   here allows to write parser which continue to work if the format is
 *   extended by adding new fields to the end of the fixed-size blobs.
 * @function_blob_size: See @entry_blob_size.
 * @callback_blob_size: See @entry_blob_size.
 * @signal_blob_size: See @entry_blob_size.
 * @vfunc_blob_size: See @entry_blob_size.
 * @arg_blob_size: See @entry_blob_size.
 * @property_blob_size: See @entry_blob_size.
 * @field_blob_size: See @entry_blob_size.
 * @value_blob_size: See @entry_blob_size.
 * @attribute_blob_size: See @entry_blob_size.
 * @constant_blob_size: See @entry_blob_size.
 * @error_domain_blob_size: See @entry_blob_size.
 * @signature_blob_size: See @entry_blob_size.
 * @enum_blob_size: See @entry_blob_size.
 * @struct_blob_size: See @entry_blob_size.
 * @object_blob_size: See @entry_blob_size.
 * @interface_blob_size: For variable-size blobs, the size of the struct up to
 *   the first flexible array member. Recording this information here allows
 *   to write parser which continue to work if the format is extended by
 *   adding new fields before the first flexible array member in
 *   variable-size blobs.
 * @union_blob_size: See @entry_blob_size.
 * @sections: Offset of section blob array
 * @padding: TODO
 *
 * The header structure appears exactly once at the beginning of a typelib.  It is a
 * collection of meta-information, such as the number of entries and dependencies.
 */
typedef struct {
  gchar   magic[16];
  guint8  major_version;
  guint8  minor_version;
  guint16 reserved;
  guint16 n_entries;
  guint16 n_local_entries;
  guint32 directory;
  guint32 n_attributes;
  guint32 attributes;

  guint32 dependencies;

  guint32 size;
  guint32 namespace;
  guint32 nsversion;
  guint32 shared_library;
  guint32 c_prefix;

  guint16 entry_blob_size;
  guint16 function_blob_size;
  guint16 callback_blob_size;
  guint16 signal_blob_size;
  guint16 vfunc_blob_size;
  guint16 arg_blob_size;
  guint16 property_blob_size;
  guint16 field_blob_size;
  guint16 value_blob_size;
  guint16 attribute_blob_size;
  guint16 constant_blob_size;
  guint16 error_domain_blob_size;

  guint16 signature_blob_size;
  guint16 enum_blob_size;
  guint16 struct_blob_size;
  guint16 object_blob_size;
  guint16 interface_blob_size;
  guint16 union_blob_size;

  guint32 sections;

  guint16 padding[6];
} Header;

/**
 * SectionType:
 * @GI_SECTION_END: TODO
 * @GI_SECTION_DIRECTORY_INDEX: TODO
 *
 * TODO
 */
typedef enum {
  GI_SECTION_END = 0,
  GI_SECTION_DIRECTORY_INDEX = 1
} SectionType;

/**
 * Section:
 * @id: A #SectionType
 * @offset: Integer offset for this section
 *
 * A section is a blob of data that's (at least theoretically) optional,
 * and may or may not be present in the typelib.  Presently, just used
 * for the directory index.  This allows a form of dynamic extensibility
 * with different tradeoffs from the format minor version.
 */
typedef struct {
  guint32 id;
  guint32 offset;
} Section;


/**
 * DirEntry:
 * @blob_type: A #GTypelibBlobType
 * @local: Whether this entry refers to a blob in this typelib.
 * @reserved: Reserved for future use.
 * @name: The name of the entry.
 * @offset: If is_local is set, this is the offset of the blob in the typelib.
 *   Otherwise, it is the offset of the namespace in which the blob has to be
 *   looked up by name.
 *
 * References to directory entries are stored as 1-based 16-bit indexes.
 *
 * All blobs pointed to by a directory entry start with the same layout for
 * the first 8 bytes (the reserved flags may be used by some blob types)
 */
typedef struct {
  guint16 blob_type;

  guint16 local    : 1;
  guint16 reserved :15;
  guint32 name;
  guint32 offset;
} DirEntry;

/**
 * SimpleTypeBlobFlags:
 * @reserved: Reserved for future use.
 * @reserved2: Reserved for future use.
 * @pointer: TODO
 * @reserved3: Reserved for future use.
 * @tag: A #GITypeTag
 *
 * TODO
 */
typedef struct {
  guint reserved   : 8;
  guint reserved2  :16;
  guint pointer    : 1;
  guint reserved3  : 2;
  guint tag        : 5;
} SimpleTypeBlobFlags;

union _SimpleTypeBlob
{
  SimpleTypeBlobFlags flags;
  guint32    offset;
};

/**
 * SimpleTypeBlob:
 * @flags: TODO
 * @offset: Offset relative to header->types that points to a TypeBlob.
 *   Unlike other offsets, this is in words (ie 32bit units) rather
 *   than bytes.
 *
 * The SimpleTypeBlob is the general purpose "reference to a type" construct,
 * used in method parameters, returns, callback definitions, fields, constants,
 * etc. It's actually just a 32 bit integer which you can see from the union
 * definition. This is for efficiency reasons, since there are so many
 * references to types.
 *
 * SimpleTypeBlob is divided into two cases; first, if "reserved" and
 * "reserved2" are both zero, the type tag for a basic type is embedded in the
 * "tag" bits. This allows e.g. GI_TYPE_TAG_UTF8, GI_TYPE_TAG_INT and the like
 * to be embedded directly without taking up extra space.
 *
 * References to "interfaces" (objects, interfaces) are more complicated;
 * In this case, the integer is actually an offset into the directory (see
 * above).  Because the header is larger than 2^8=256 bits, all offsets will
 * have one of the upper 24 bits set.
 */
typedef union _SimpleTypeBlob SimpleTypeBlob;

/**
 * ArgBlob:
 * @name: A suggested name for the parameter.
 * @in: The parameter is an input to the function
 * @out: The parameter is used to return an output of the function. Parameters
 *   can be both in and out. Out parameters implicitly add another level of
 *   indirection to the parameter type. Ie if the type is uint32 in an out
 *   parameter, the function actually takes an uint32*.
 * @caller_allocates: The parameter is a pointer to a struct or object that
 *   will receive an output of the function.
 * @nullable: Only meaningful for types which are passed as pointers. For an
 *   in parameter, indicates if it is ok to pass NULL in. Gor an out
 *   parameter, indicates whether it may return NULL. Note that NULL is a
 *   valid GList and GSList value, thus allow_none will normally be set
 *   for parameters of these types.
 * @optional: For an out parameter, indicates that NULL may be passed in
 *   if the value is not needed.
 * @transfer_ownership: For an in parameter, indicates that the function takes
 *   over ownership of the parameter value. For an out parameter, it indicates
 *   that the caller is responsible for freeing the return value.
 * @transfer_container_ownership: For container types, indicates that the
 *   ownership of the container, but not of its contents is transferred.
 *   This is typically the case for out parameters returning lists of
 *   statically allocated things.
 * @return_value: The parameter should be considered the return value of the
 *   function. Only out parameters can be marked as return value, and there
 *   can be at most one per function call. If an out parameter is marked as
 *   return value, the actual return value of the function should be either
 *   void or a boolean indicating the success of the call.
 * @scope: A #GIScopeType. If the parameter is of a callback type, this denotes
 *   the scope of the user_data and the callback function pointer itself
 *   (for languages that emit code at run-time).
 * @skip: Indicates that the parameter is only useful in C and should be skipped.
 * @reserved: Reserved for future use.
 * @closure: Index of the closure (user_data) parameter associated with the
 *   callback, or -1.
 * @destroy: Index of the destroy notfication callback parameter associated
 *   with the callback, or -1.
 * @padding: TODO
 * @arg_type: Describes the type of the parameter. See details below.
 *
 * Types are specified by four bytes. If the three high bytes are zero,
 * the low byte describes a basic type, otherwise the 32bit number is an
 * offset which points to a TypeBlob.
 */
typedef struct {
  guint32        name;

  guint          in                           : 1;
  guint          out                          : 1;
  guint          caller_allocates             : 1;
  guint          nullable                     : 1;
  guint          optional                     : 1;
  guint          transfer_ownership           : 1;
  guint          transfer_container_ownership : 1;
  guint          return_value                 : 1;
  guint          scope                        : 3;
  guint          skip                         : 1;
  guint          reserved                     :20;
  gint8          closure;
  gint8          destroy;

  guint16        padding;

  SimpleTypeBlob arg_type;
} ArgBlob;

/**
 * SignatureBlob:
 * @return_type: Describes the type of the return value. See details below.
 * @may_return_null: Only relevant for pointer types. Indicates whether the
 *   caller must expect NULL as a return value.
 * @caller_owns_return_value: If set, the caller is responsible for freeing
 *   the return value if it is no longer needed.
 * @caller_owns_return_container: This flag is only relevant if the return type
 *   is a container type. If the flag is set, the caller is resonsible for
 *   freeing the container, but not its contents.
 * @skip_return: Indicates that the return value is only useful in C and should
 *   be skipped.
 * @instance_transfer_ownership: When calling, the function assumes ownership of
 *   the instance parameter.
 * @throws: Denotes the signature takes an additional #GError argument beyond
 *   the annotated arguments.
 * @reserved: Reserved for future use.
 * @n_arguments: The number of arguments that this function expects, also the
 *   length of the array of ArgBlobs.
 * @arguments: An array of ArgBlob for the arguments of the function.
 *
 * TODO
 */
typedef struct {
  SimpleTypeBlob return_type;

  guint16        may_return_null              : 1;
  guint16        caller_owns_return_value     : 1;
  guint16        caller_owns_return_container : 1;
  guint16        skip_return                  : 1;
  guint16        instance_transfer_ownership  : 1;
  guint16        throws                       : 1;
  guint16        reserved                     :10;

  guint16        n_arguments;

  ArgBlob        arguments[];
} SignatureBlob;

/**
 * CommonBlob:
 * @blob_type: A #GTypelibBlobType
 * @deprecated: Whether the blob is deprecated.
 * @reserved: Reserved for future use.
 * @name: The name of the blob.
 *
 * The #CommonBlob is shared between #FunctionBlob,
 * #CallbackBlob, #SignalBlob.
 *
 * TODO
 */
typedef struct {
  guint16 blob_type;  /* 1 */

  guint16 deprecated : 1;
  guint16 reserved   :15;
  guint32 name;
} CommonBlob;

/**
 * FunctionBlob:
 * @blob_type: #BLOB_TYPE_FUNCTION
 * @deprecated: The function is deprecated.
 * @setter: The function is a setter for a property. Language bindings may
 *   prefer to not bind individual setters and rely on the generic
 *   g_object_set().
 * @getter: The function is a getter for a property. Language bindings may
 *   prefer to not bind individual getters and rely on the generic
 *   g_object_get().
 * @constructor: The function acts as a constructor for the object it is
 *   contained in.
 * @wraps_vfunc: The function is a simple wrapper for a virtual function.
 * @throws: This is now additionally stored in the #SignatureBlob. (deprecated)
 * @index: Index of the property that this function is a setter or getter of
 *   in the array of properties of the containing interface, or index
 *   of the virtual function that this function wraps.
 * @name: TODO
 * @symbol: The symbol which can be used to obtain the function pointer with
 *   dlsym().
 * @signature: Offset of the SignatureBlob describing the parameter types and the
 *   return value type.
 * @is_static: The function is a "static method"; in other words it's a pure
 *   function whose name is conceptually scoped to the object.
 * @reserved: Reserved for future use.
 * @reserved2: Reserved for future use.
 *
 * TODO
 */
typedef struct {
  guint16 blob_type;  /* 1 */

  guint16 deprecated  : 1;
  guint16 setter      : 1;
  guint16 getter      : 1;
  guint16 constructor : 1;
  guint16 wraps_vfunc : 1;
  guint16 throws      : 1;
  guint16 index       :10;
  /* Note the bits above need to match CommonBlob
   * and are thus exhausted, extend things using
   * the reserved block below. */

  guint32 name;
  guint32 symbol;
  guint32 signature;

  guint16 is_static   : 1;
  guint16 reserved    : 15;
  guint16 reserved2   : 16;
} FunctionBlob;

/**
 * CallbackBlob:
 * @blob_type: TODO
 * @deprecated: TODO
 * @reserved: Reserved for future use.
 * @name: TODO
 * @signature: Offset of the #SignatureBlob describing the parameter types and
 *   the return value type.
 *
 * TODO
 */
typedef struct {
  guint16 blob_type;  /* 2 */

  guint16 deprecated : 1;
  guint16 reserved   :15;
  guint32 name;
  guint32 signature;
} CallbackBlob;

/**
 * InterfaceTypeBlob:
 * @pointer: Whether this type represents an indirection
 * @reserved: Reserved for future use.
 * @tag: A #GITypeTag
 * @reserved2: Reserved for future use.
 * @interface: Index of the directory entry for the interface.
 *
 * If the interface is an enum of flags type, is_pointer is 0, otherwise it is 1.
 */
typedef struct {
  guint8  pointer  :1;
  guint8  reserved :2;
  guint8  tag      :5;
  guint8  reserved2;
  guint16 interface;
} InterfaceTypeBlob;

/**
 * ArrayTypeDimension:
 * @length: TODO
 * @size: TODO
 *
 * TODO
 */
typedef union {
  guint16 length;
  guint16 size;
} ArrayTypeDimension;

/**
 * ArrayTypeBlob:
 * @pointer: TODO
 * @reserved: Reserved for future use.
 * @tag: TODO
 * @zero_terminated: Indicates that the array must be terminated by a suitable
 *   #NULL value.
 * @has_length: Indicates that length points to a parameter specifying the
 *   length of the array. If both has_length and zero_terminated are set, the
 *   convention is to pass -1 for the length if the array is zero-terminated.
 * @has_size: Indicates that size is the fixed size of the array.
 * @array_type: Indicates whether this is a C array, GArray, GPtrArray, or
 *   GByteArray. If something other than a C array, the length and element
 *   size are implicit in the structure.
 * @reserved2: Reserved for future use.
 * @dimensions: TODO
 * @type: TODO
 *
 * TODO
 */
typedef struct {
  guint16 pointer         :1;
  guint16 reserved        :2;
  guint16 tag             :5;

  guint16 zero_terminated :1;
  guint16 has_length      :1;
  guint16 has_size        :1;
  guint16 array_type      :2;
  guint16 reserved2       :3;

  ArrayTypeDimension dimensions;

  SimpleTypeBlob type;
} ArrayTypeBlob;

/**
 * ParamTypeBlob:
 * @pointer: TODO
 * @reserved: Reserved for future use.
 * @tag: TODO
 * @reserved2: Reserved for future use.
 * @n_types: The number of parameter types to follow.
 * @type: Describes the type of the list elements.
 *
 * TODO
 */
typedef struct {
  guint8	 pointer  :1;
  guint8	 reserved :2;
  guint8	 tag      :5;

  guint8	 reserved2;
  guint16	 n_types;

  SimpleTypeBlob type[];
} ParamTypeBlob;

/**
 * ErrorTypeBlob:
 * @pointer: TODO
 * @reserved: TODO
 * @tag: TODO
 * @reserved2: TODO
 * @n_domains: TODO: must be 0
 * @domains: TODO
 *
 * TODO
 */
typedef struct {
  guint8  pointer  :1;
  guint8  reserved :2;
  guint8  tag      :5;

  guint8  reserved2;

  guint16 n_domains; /* Must be 0 */
  guint16 domains[];
}  ErrorTypeBlob;

/**
 * ValueBlob:
 * @deprecated: Whether this value is deprecated
 * @unsigned_value: if set, value is a 32-bit unsigned integer cast to gint32
 * @reserved: Reserved for future use.
 * @name: Name of blob
 * @value: The numerical value
 *
 * Values commonly occur in enums and flags.
 */
typedef struct {
  guint32 deprecated : 1;
  guint32 unsigned_value : 1;
  guint32 reserved   :30;
  guint32 name;
  gint32 value;
} ValueBlob;

/**
 * FieldBlob:
 * @name: The name of the field.
 * @readable: TODO
 * @writable: How the field may be accessed.
 * @has_embedded_type: An anonymous type follows the FieldBlob.
 * @reserved: Reserved for future use.
 * @bits: If this field is part of a bitfield, the number of bits which it
 *   uses, otherwise 0.
 * @struct_offset: The offset of the field in the struct. The value 0xFFFF
 *   indicates that the struct offset is unknown.
 * @reserved2: Reserved for future use.
 * @type: The type of the field.
 *
 * TODO
 */
typedef struct {
  guint32        name;

  guint8         readable :1;
  guint8         writable :1;
  guint8         has_embedded_type :1;
  guint8         reserved :5;
  guint8         bits;

  guint16        struct_offset;

  guint32        reserved2;

  SimpleTypeBlob type;
} FieldBlob;

/**
 * RegisteredTypeBlob:
 * @blob_type: TODO
 * @deprecated: TODO
 * @unregistered: TODO
 * @reserved: Reserved for future use.
 * @name: TODO
 * @gtype_name: The name under which the type is registered with GType.
 * @gtype_init: The symbol name of the get_type<!-- -->() function which registers the
 *   type.
 *
 * TODO
 */
typedef struct {
  guint16 blob_type;
  guint16 deprecated   : 1;
  guint16 unregistered : 1;
  guint16 reserved :14;
  guint32 name;

  guint32 gtype_name;
  guint32 gtype_init;
} RegisteredTypeBlob;

/**
 * StructBlob:
 * @blob_type: #BLOB_TYPE_STRUCT
 * @deprecated: Whether this structure is deprecated
 * @unregistered: If this is set, the type is not registered with GType.
 * @is_gtype_struct: Whether this structure is the class or interface layout
 *   for a GObject
 * @alignment: The byte boundary that the struct is aligned to in memory
 * @foreign: If the type is foreign, eg if it's expected to be overridden by
 *   a native language binding instead of relying of introspected bindings.
 * @reserved: Reserved for future use.
 * @name: TODO
 * @gtype_name: String name of the associated #GType
 * @gtype_init: String naming the symbol which gets the runtime #GType
 * @size: The size of the struct in bytes.
 * @n_fields: TODO
 * @n_methods: TODO
 * @copy_func: String pointing to a function which can be called to copy
 *   the contents of this struct type
 * @free_func: String pointing to a function which can be called to free
 *   the contents of this struct type
 *
 * TODO
 */
typedef struct {
  guint16   blob_type;

  guint16   deprecated   : 1;
  guint16   unregistered : 1;
  guint16   is_gtype_struct : 1;
  guint16   alignment    : 6;
  guint16   foreign      : 1;
  guint16   reserved     : 6;

  guint32   name;

  guint32   gtype_name;
  guint32   gtype_init;

  guint32   size;

  guint16   n_fields;
  guint16   n_methods;

  guint32   copy_func;
  guint32   free_func;
} StructBlob;

/**
 * UnionBlob:
 * @blob_type: TODO
 * @deprecated: TODO
 * @unregistered: If this is set, the type is not registered with GType.
 * @discriminated: Is set if the union is discriminated
 * @alignment: The byte boundary that the union is aligned to in memory
 * @reserved: Reserved for future use.
 * @name: TODO
 * @gtype_name: String name of the associated #GType
 * @gtype_init: String naming the symbol which gets the runtime #GType
 * @size: TODO
 * @n_fields: Length of the arrays
 * @n_functions: TODO
 * @copy_func: String pointing to a function which can be called to copy
 *   the contents of this union type
 * @free_func: String pointing to a function which can be called to free
 *   the contents of this union type
 * @discriminator_offset: Offset from the beginning of the union where the
 *   discriminator of a discriminated union is located. The value 0xFFFF
 *   indicates that the discriminator offset is unknown.
 * @discriminator_type: Type of the discriminator
 *
 * TODO
 */
typedef struct {
  guint16      blob_type;
  guint16      deprecated    : 1;
  guint16      unregistered  : 1;
  guint16      discriminated : 1;
  guint16      alignment     : 6;
  guint16      reserved      : 7;
  guint32      name;

  guint32      gtype_name;
  guint32      gtype_init;

  guint32      size;

  guint16      n_fields;
  guint16      n_functions;

  guint32      copy_func;
  guint32      free_func;

  gint32       discriminator_offset;
  SimpleTypeBlob discriminator_type;
} UnionBlob;

/**
 * EnumBlob:
 * @blob_type: TODO
 * @deprecated: TODO
 * @unregistered: If this is set, the type is not registered with GType.
 * @storage_type: The tag of the type used for the enum in the C ABI
 *   (will be a signed or unsigned integral type)
 * @reserved: Reserved for future use.
 * @name: TODO
 * @gtype_name: String name of the associated #GType
 * @gtype_init: String naming the symbol which gets the runtime #GType
 * @n_values: The length of the values array.
 * @n_methods: The length of the methods array.
 * @error_domain: String naming the #GError domain this enum is associated with
 * @values: TODO
 *
 * TODO
 */
typedef struct {
  guint16   blob_type;

  guint16   deprecated   : 1;
  guint16   unregistered : 1;
  guint16   storage_type : 5;
  guint16   reserved     : 9;

  guint32   name;

  guint32   gtype_name;
  guint32   gtype_init;

  guint16   n_values;
  guint16   n_methods;

  guint32   error_domain;

  ValueBlob values[];
} EnumBlob;

#define ACCESSOR_SENTINEL       0x3ff

/**
 * PropertyBlob:
 * @name: The name of the property.
 * @deprecated: TODO
 * @readable: TODO
 * @writable: TODO
 * @construct: TODO
 * @construct_only: The ParamFlags used when registering the property.
 * @transfer_ownership: When writing, the type containing the property takes
 *   ownership of the value. When reading, the returned value needs to be
 *   released by the caller.
 * @transfer_container_ownership: For container types indicates that the
 *   ownership of the container, but not of its contents, is transferred.
 *   This is typically the case when reading lists of statically allocated
 *   things.
 * @setter: the index of the setter function for this property, if @writable
 *   is set; if the method is not known, the value will be set to %ACCESSOR_SENTINEL
 * @getter: ths index of the getter function for this property, if @readable
 *   is set; if the method is not known, the value will be set to %ACCESSOR_SENTINEL
 * @reserved: Reserved for future use.
 * @reserved2: Reserved for future use.
 * @type: Describes the type of the property.
 *
 * TODO
 */
typedef struct {
  guint32        name;

  guint32        deprecated                   : 1;
  guint32        readable                     : 1;
  guint32        writable                     : 1;
  guint32        construct                    : 1;
  guint32        construct_only               : 1;
  guint32        transfer_ownership           : 1;
  guint32        transfer_container_ownership : 1;
  guint32        setter                       :10;
  guint32        getter                       :10;
  guint32        reserved                     : 5;

  guint32        reserved2;

  SimpleTypeBlob type;
} PropertyBlob;

/**
 * SignalBlob:
 * @deprecated: TODO
 * @run_first: TODO
 * @run_last: TODO
 * @run_cleanup: TODO
 * @no_recurse: TODO
 * @detailed: TODO
 * @action: TODO
 * @no_hooks: The flags used when registering the signal.
 * @has_class_closure: Set if the signal has a class closure.
 * @true_stops_emit: Whether the signal has true-stops-emit semantics
 * @reserved: Reserved for future use.
 * @class_closure: The index of the class closure in the list of virtual
 *   functions of the object or interface on which the signal is defined.
 * @name: The name of the signal.
 * @reserved2: Reserved for future use.
 * @signature: Offset of the SignatureBlob describing the parameter types
 *   and the return value type.
 *
 * TODO
 */
typedef struct {
  guint16 deprecated        : 1;
  guint16 run_first         : 1;
  guint16 run_last          : 1;
  guint16 run_cleanup       : 1;
  guint16 no_recurse        : 1;
  guint16 detailed          : 1;
  guint16 action            : 1;
  guint16 no_hooks          : 1;
  guint16 has_class_closure : 1;
  guint16 true_stops_emit   : 1;
  guint16 reserved          : 6;

  guint16 class_closure;

  guint32 name;

  guint32 reserved2;

  guint32 signature;
} SignalBlob;

/**
 * VFuncBlob:
 * @name: The name of the virtual function.
 * @must_chain_up: If set, every implementation of this virtual function must
 *   chain up to the implementation of the parent class.
 * @must_be_implemented: If set, every derived class must override this virtual
 *   function.
 * @must_not_be_implemented: If set, derived class must not override this
 *   virtual function.
 * @class_closure: Set if this virtual function is the class closure of a
 *   signal.
 * @throws: This is now additionally stored in the #SignatureBlob. (deprecated)
 * @reserved: Reserved for future use.
 * @signal: The index of the signal in the list of signals of the object or
 *   interface to which this virtual function belongs.
 * @struct_offset: The offset of the function pointer in the class struct.
 *   The value 0xFFFF indicates that the struct offset is unknown.
 * @invoker: If a method invoker for this virtual exists, this is the offset
 *   in the class structure of the method. If no method is known, this value
 *   will be 0x3ff.
 * @reserved2: Reserved for future use.
 * @reserved3: Reserved for future use.
 * @signature: Offset of the SignatureBlob describing the parameter types and
 *   the return value type.
 *
 * TODO
 */
typedef struct {
  guint32 name;

  guint16 must_chain_up           : 1;
  guint16 must_be_implemented     : 1;
  guint16 must_not_be_implemented : 1;
  guint16 class_closure           : 1;
  guint16 throws                  : 1;
  guint16 reserved                :11;
  guint16 signal;

  guint16 struct_offset;
  guint16 invoker : 10; /* Number of bits matches @index in FunctionBlob */
  guint16 reserved2 : 6;

  guint32 reserved3;
  guint32 signature;
} VFuncBlob;

/**
 * ObjectBlob:
 * @blob_type: #BLOB_TYPE_OBJECT
 * @deprecated: whether the type is deprecated
 * @abstract: whether the type can be instantiated
 * @fundamental: this object is not a GObject derived type, instead it's
 *   an additional fundamental type.
 * @final_: whether the type can be derived
 * @reserved: Reserved for future use.
 * @name: TODO
 * @gtype_name: String name of the associated #GType
 * @gtype_init: String naming the symbol which gets the runtime #GType
 * @parent: The directory index of the parent type. This is only set for
 *   objects. If an object does not have a parent, it is zero.
 * @gtype_struct: TODO
 * @n_interfaces: TODO
 * @n_fields: TODO
 * @n_properties: TODO
 * @n_methods: TODO
 * @n_signals: TODO
 * @n_vfuncs: TODO
 * @n_constants: The lengths of the arrays.Up to 16bits of padding may be
 *   inserted between the arrays to ensure that they start on a 32bit
 *   boundary.
 * @n_field_callbacks: The number of n_fields which are also callbacks.
 *   This is used to calculate the fields section size in constant time.
 * @ref_func: String pointing to a function which can be called to increase
 *   the reference count for an instance of this object type.
 * @unref_func: String pointing to a function which can be called to decrease
 *   the reference count for an instance of this object type.
 * @set_value_func: String pointing to a function which can be called to
 *   convert a pointer of this object to a GValue
 * @get_value_func: String pointing to a function which can be called to
 *   convert extract a pointer to this object from a GValue
 * @reserved3: Reserved for future use.
 * @reserved4: Reserved for future use.
 * @interfaces: An array of indices of directory entries for the implemented
 *   interfaces.
 *
 * TODO
 */
typedef struct {
  guint16   blob_type;  /* 7 */
  guint16   deprecated   : 1;
  guint16   abstract     : 1;
  guint16   fundamental  : 1;
  guint16   final_       : 1;
  guint16   reserved     :12;
  guint32   name;

  guint32   gtype_name;
  guint32   gtype_init;

  guint16   parent;
  guint16   gtype_struct;

  guint16   n_interfaces;
  guint16   n_fields;
  guint16   n_properties;
  guint16   n_methods;
  guint16   n_signals;
  guint16   n_vfuncs;
  guint16   n_constants;
  guint16   n_field_callbacks;

  guint32   ref_func;
  guint32   unref_func;
  guint32   set_value_func;
  guint32   get_value_func;

  guint32   reserved3;
  guint32   reserved4;

  guint16   interfaces[];
} ObjectBlob;

/**
 * InterfaceBlob:
 * @blob_type: TODO
 * @deprecated: TODO
 * @reserved: Reserved for future use.
 * @name: TODO
 * @gtype_name: TODO
 * @gtype_init: TODO
 * @gtype_struct: Name of the interface "class" C structure
 * @n_prerequisites: Number of prerequisites
 * @n_properties: Number of properties
 * @n_methods: Number of methods
 * @n_signals: Number of signals
 * @n_vfuncs: Number of virtual functions
 * @n_constants: The lengths of the arrays. Up to 16bits of padding may be
 *   inserted between the arrays to ensure that they start on a 32bit
 *   boundary.
 * @padding: TODO
 * @reserved2: Reserved for future use.
 * @reserved3: Reserved for future use.
 * @prerequisites: An array of indices of directory entries for required
 *   interfaces.
 *
 * TODO
 */
typedef struct {
  guint16 blob_type;
  guint16 deprecated   : 1;
  guint16 reserved     :15;
  guint32 name;

  guint32 gtype_name;
  guint32 gtype_init;
  guint16 gtype_struct;

  guint16 n_prerequisites;
  guint16 n_properties;
  guint16 n_methods;
  guint16 n_signals;
  guint16 n_vfuncs;
  guint16 n_constants;

  guint16 padding;

  guint32 reserved2;
  guint32 reserved3;

  guint16 prerequisites[];
} InterfaceBlob;

/**
 * ConstantBlob:
 * @blob_type: TODO
 * @deprecated: TODO
 * @reserved: Reserved for future use.
 * @name: TODO
 * @type: The type of the value. In most cases this should be a numeric type
 *   or string.
 * @size: The size of the value in bytes.
 * @offset: The offset of the value in the typelib.
 * @reserved2: Reserved for future use.
 *
 * TODO
 */
typedef struct {
  guint16        blob_type;
  guint16        deprecated   : 1;
  guint16        reserved     :15;
  guint32        name;

  SimpleTypeBlob type;

  guint32        size;
  guint32        offset;

  guint32        reserved2;
} ConstantBlob;

/**
 * AttributeBlob:
 * @offset: The offset of the typelib entry to which this attribute refers.
 *   Attributes are kept sorted by offset, so that the attributes of an
 *   entry can be found by a binary search.
 * @name: The name of the attribute, a string.
 * @value: The value of the attribute (also a string)
 *
 * TODO
 */
typedef struct {
  guint32 offset;
  guint32 name;
  guint32 value;
} AttributeBlob;

struct _GITypelib {
  /*< private >*/
  guchar *data;
  gsize len;
  gboolean owns_memory;
  GMappedFile *mfile;
  GList *modules;
  gboolean open_attempted;
};

DirEntry *g_typelib_get_dir_entry (GITypelib *typelib,
				   guint16   index);

DirEntry *g_typelib_get_dir_entry_by_name (GITypelib *typelib,
					   const char *name);

DirEntry *g_typelib_get_dir_entry_by_gtype_name (GITypelib *typelib,
						 const gchar *gtype_name);

DirEntry *g_typelib_get_dir_entry_by_error_domain (GITypelib *typelib,
						   GQuark     error_domain);

gboolean  g_typelib_matches_gtype_name_prefix (GITypelib *typelib,
					       const gchar *gtype_name);


GI_AVAILABLE_IN_ALL
void      g_typelib_check_sanity (void);

/**
 * g_typelib_get_string:
 * @typelib: TODO
 * @offset: TODO
 *
 * TODO
 *
 * Returns: TODO
 */
#define   g_typelib_get_string(typelib,offset) ((const gchar*)&(typelib->data)[(offset)])


/**
 * GITypelibError:
 * @G_TYPELIB_ERROR_INVALID: the typelib is invalid
 * @G_TYPELIB_ERROR_INVALID_HEADER: the typelib header is invalid
 * @G_TYPELIB_ERROR_INVALID_DIRECTORY: the typelib directory is invalid
 * @G_TYPELIB_ERROR_INVALID_ENTRY: a typelib entry is invalid
 * @G_TYPELIB_ERROR_INVALID_BLOB: a typelib blob is invalid
 *
 * A error set while validating the #GITypelib
 */
typedef enum
{
  G_TYPELIB_ERROR_INVALID,
  G_TYPELIB_ERROR_INVALID_HEADER,
  G_TYPELIB_ERROR_INVALID_DIRECTORY,
  G_TYPELIB_ERROR_INVALID_ENTRY,
  G_TYPELIB_ERROR_INVALID_BLOB
} GITypelibError;

/**
 * G_TYPELIB_ERROR:
 *
 * TODO
 */
#define G_TYPELIB_ERROR (g_typelib_error_quark ())

GQuark g_typelib_error_quark (void);


GI_AVAILABLE_IN_ALL
gboolean g_typelib_validate (GITypelib  *typelib,
			     GError    **error);


/* defined in gibaseinfo.c */
AttributeBlob *_attribute_blob_find_first (GIBaseInfo *info,
                                           guint32     blob_offset);

/**
 * GITypelibHashBuilder:
 *
 * TODO
 */
typedef struct _GITypelibHashBuilder GITypelibHashBuilder;

GITypelibHashBuilder * _gi_typelib_hash_builder_new (void);

void _gi_typelib_hash_builder_add_string (GITypelibHashBuilder *builder, const char *str, guint16 value);

gboolean _gi_typelib_hash_builder_prepare (GITypelibHashBuilder *builder);

guint32 _gi_typelib_hash_builder_get_buffer_size (GITypelibHashBuilder *builder);

void _gi_typelib_hash_builder_pack (GITypelibHashBuilder *builder, guint8* mem, guint32 size);

void _gi_typelib_hash_builder_destroy (GITypelibHashBuilder *builder);

guint16 _gi_typelib_hash_search (guint8* memory, const char *str, guint n_entries);


G_END_DECLS

#endif  /* __G_TYPELIB_H__ */

