#pragma once

#define DEFINE_INLINE_ARRAY(Type, prefix, ElementType)              \
  typedef struct _##Type {                                          \
    gsize len;                                                      \
    gsize allocated;                                                \
    ElementType *items;                                             \
  } Type;                                                           \
                                                                    \
  static inline void                                                \
  prefix##_init (Type  *ar,                                         \
                 gsize  initial_size)                               \
  {                                                                 \
    ar->len = 0;                                                    \
    ar->allocated = initial_size ? initial_size : 16;               \
    ar->items = g_new0 (ElementType, ar->allocated);                \
  }                                                                 \
                                                                    \
  static inline void                                                \
  prefix##_clear (Type *ar)                                         \
  {                                                                 \
    ar->len = 0;                                                    \
    ar->allocated = 0;                                              \
    g_clear_pointer (&ar->items, g_free);                           \
  }                                                                 \
                                                                    \
  static inline ElementType *                                       \
  prefix##_head (Type *ar)                                          \
  {                                                                 \
    return &ar->items[0];                                           \
  }                                                                 \
                                                                    \
  static inline ElementType *                                       \
  prefix##_tail (Type *ar)                                          \
  {                                                                 \
    return &ar->items[ar->len-1];                                   \
  }                                                                 \
                                                                    \
  static inline ElementType *                                       \
  prefix##_append (Type *ar)                                        \
  {                                                                 \
    if G_UNLIKELY (ar->len == ar->allocated)                        \
      {                                                             \
        ar->allocated *= 2;                                         \
        ar->items = g_renew (ElementType, ar->items, ar->allocated);\
      }                                                             \
                                                                    \
    ar->len++;                                                      \
                                                                    \
    return prefix##_tail (ar);                                      \
  }                                                                 \
                                                                    \
  static inline ElementType *                                       \
  prefix##_append_n (Type  *ar,                                     \
                     gsize  n)                                      \
  {                                                                 \
    if G_UNLIKELY ((ar->len + n) > ar->allocated)                   \
      {                                                             \
        while ((ar->len + n) > ar->allocated)                       \
          ar->allocated *= 2;                                       \
        ar->items = g_renew (ElementType, ar->items, ar->allocated);\
      }                                                             \
                                                                    \
    ar->len += n;                                                   \
                                                                    \
    return &ar->items[ar->len-n];                                   \
  }                                                                 \
                                                                    \
  static inline gsize                                               \
  prefix##_index_of (Type              *ar,                         \
                     const ElementType *element)                    \
  {                                                                 \
    return element - &ar->items[0];                                 \
  }

