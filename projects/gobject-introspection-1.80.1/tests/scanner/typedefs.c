#include "config.h"

#include "typedefs.h"

static TypedefsBoxedWithTypedefBefore *
typedefs_boxed_with_typedef_before_ref(TypedefsBoxedWithTypedefBefore *self)
{
    return self;
}

static void
typedefs_boxed_with_typedef_before_unref(TypedefsBoxedWithTypedefBefore *self)
{
}

G_DEFINE_BOXED_TYPE (TypedefsBoxedWithTypedefBefore,
                     typedefs_boxed_with_typedef_before,
                     typedefs_boxed_with_typedef_before_ref,
                     typedefs_boxed_with_typedef_before_unref);


static TypedefsBoxedWithTypedefAfter *
typedefs_boxed_with_typedef_after_ref(TypedefsBoxedWithTypedefAfter *self)
{
    return self;
}

static void
typedefs_boxed_with_typedef_after_unref(TypedefsBoxedWithTypedefAfter *self)
{
}

G_DEFINE_BOXED_TYPE (TypedefsBoxedWithTypedefAfter,
                     typedefs_boxed_with_typedef_after,
                     typedefs_boxed_with_typedef_after_ref,
                     typedefs_boxed_with_typedef_after_unref);


static TypedefsBoxedWithTagAndTypedef *
typedefs_boxed_with_tag_and_typedef_ref(TypedefsBoxedWithTagAndTypedef *self)
{
    return self;
}

static void
typedefs_boxed_with_tag_and_typedef_unref(TypedefsBoxedWithTagAndTypedef *self)
{
}

G_DEFINE_BOXED_TYPE (TypedefsBoxedWithTagAndTypedef,
                     typedefs_boxed_with_tag_and_typedef,
                     typedefs_boxed_with_tag_and_typedef_ref,
                     typedefs_boxed_with_tag_and_typedef_unref);


static TypedefsBoxedWithAnonymousTypedef *
typedefs_boxed_with_anonymous_typedef_ref(TypedefsBoxedWithAnonymousTypedef *self)
{
    return self;
}

static void
typedefs_boxed_with_anonymous_typedef_unref(TypedefsBoxedWithAnonymousTypedef *self)
{
}

G_DEFINE_BOXED_TYPE (TypedefsBoxedWithAnonymousTypedef,
                     typedefs_boxed_with_anonymous_typedef,
                     typedefs_boxed_with_anonymous_typedef_ref,
                     typedefs_boxed_with_anonymous_typedef_unref);


struct _TypedefsBoxedWithHiddenStruct {
    int value;
};

static TypedefsBoxedWithHiddenStruct *
typedefs_boxed_with_hidden_struct_ref(TypedefsBoxedWithHiddenStruct *self)
{
    return self;
}

static void
typedefs_boxed_with_hidden_struct_unref(TypedefsBoxedWithHiddenStruct *self)
{
}

G_DEFINE_BOXED_TYPE (TypedefsBoxedWithHiddenStruct,
                     typedefs_boxed_with_hidden_struct,
                     typedefs_boxed_with_hidden_struct_ref,
                     typedefs_boxed_with_hidden_struct_unref);

