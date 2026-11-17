/*
 * Copyright © 2020 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include <glib.h>

G_BEGIN_DECLS

#ifndef GDK_ARRAY_TYPE_NAME
#define GDK_ARRAY_TYPE_NAME GdkArray
#endif

#ifndef GDK_ARRAY_NAME
#define GDK_ARRAY_NAME gdk_array
#endif

#ifndef GDK_ARRAY_ELEMENT_TYPE
#define GDK_ARRAY_ELEMENT_TYPE gpointer
#endif

#ifdef GDK_ARRAY_PREALLOC
#if GDK_ARRAY_PREALLOC == 0
#undef GDK_ARRAY_PREALLOC
#endif
#endif

#ifdef GDK_ARRAY_NULL_TERMINATED
#define GDK_ARRAY_REAL_SIZE(_size) ((_size) + 1)
#else
#define GDK_ARRAY_REAL_SIZE(_size) (_size)
#endif

/* make this readable */
#define _T_ GDK_ARRAY_ELEMENT_TYPE
#define GdkArray GDK_ARRAY_TYPE_NAME
#define gdk_array_paste_more(GDK_ARRAY_NAME, func_name) GDK_ARRAY_NAME ## _ ## func_name
#define gdk_array_paste(GDK_ARRAY_NAME, func_name) gdk_array_paste_more (GDK_ARRAY_NAME, func_name)
#define gdk_array(func_name) gdk_array_paste (GDK_ARRAY_NAME, func_name)

typedef struct GdkArray GdkArray;

struct GdkArray
{
  _T_ *start;
  _T_ *end;
  _T_ *end_allocation;
#ifdef GDK_ARRAY_PREALLOC
  _T_ preallocated[GDK_ARRAY_REAL_SIZE(GDK_ARRAY_PREALLOC)];
#endif
};

/* no G_GNUC_UNUSED here, if you don't use an array type, remove it. */
static inline void
gdk_array(init) (GdkArray *self)
{
#ifdef GDK_ARRAY_PREALLOC
  self->start = self->preallocated;
  self->end = self->start;
  self->end_allocation = self->start + GDK_ARRAY_PREALLOC;
#ifdef GDK_ARRAY_NULL_TERMINATED
  *self->start = *(_T_[1]) { 0 };
#endif
#else
  self->start = NULL;
  self->end = NULL;
  self->end_allocation = NULL;
#endif
}

G_GNUC_UNUSED static inline gsize
gdk_array(get_capacity) (const GdkArray *self)
{
  return self->end_allocation - self->start;
}

G_GNUC_UNUSED static inline gsize
gdk_array(get_size) (const GdkArray *self)
{
  return self->end - self->start;
}

static inline void
gdk_array(free_elements) (_T_ *start,
                          _T_ *end)
{
#ifdef GDK_ARRAY_FREE_FUNC
  _T_ *e;
  for (e = start; e < end; e++)
#ifdef GDK_ARRAY_BY_VALUE
    GDK_ARRAY_FREE_FUNC (e);
#else
    GDK_ARRAY_FREE_FUNC (*e);
#endif
#endif
}

/* no G_GNUC_UNUSED here */
static inline void
gdk_array(clear) (GdkArray *self)
{
  gdk_array(free_elements) (self->start, self->end);

#ifdef GDK_ARRAY_PREALLOC
  if (self->start != self->preallocated)
#endif
    g_free (self->start);
  gdk_array(init) (self);
}

/*
 * gdk_array_steal:
 * @self: the array
 *
 * Steals all data in the array and clears the array.
 *
 * If you need to know the size of the data, you should query it
 * beforehand.
 *
 * Returns: The array's data
 **/
G_GNUC_UNUSED static inline _T_ *
gdk_array(steal) (GdkArray *self)
{
  _T_ *result;

#ifdef GDK_ARRAY_PREALLOC
  if (self->start == self->preallocated)
    {
      gsize size = GDK_ARRAY_REAL_SIZE (gdk_array(get_size) (self));
      result = g_new (_T_, size);
      memcpy (result, self->preallocated, sizeof (_T_) * size);
    }
  else
#endif
    result = self->start;

  gdk_array(init) (self);

  return result;
}

G_GNUC_UNUSED static inline _T_ *
gdk_array(get_data) (const GdkArray *self)
{
  return self->start;
}

G_GNUC_UNUSED static inline _T_ *
gdk_array(index) (const GdkArray *self,
                  gsize           pos)
{
  return self->start + pos;
}

G_GNUC_UNUSED static inline gboolean
gdk_array(is_empty) (const GdkArray *self)
{
  return self->end == self->start;
}

G_GNUC_UNUSED static inline void
gdk_array(reserve) (GdkArray *self,
                    gsize      n)
{
  gsize new_size, size;

  if (n <= gdk_array(get_capacity) (self))
    return;

  size = gdk_array(get_size) (self);
  new_size = 1 << g_bit_storage (MAX (GDK_ARRAY_REAL_SIZE (n), 16) - 1);

#ifdef GDK_ARRAY_PREALLOC
  if (self->start == self->preallocated)
    {
      self->start = g_new (_T_, new_size);
      memcpy (self->start, self->preallocated, sizeof (_T_) * GDK_ARRAY_REAL_SIZE (size));
    }
  else
#endif
#ifdef GDK_ARRAY_NULL_TERMINATED
  if (self->start == NULL)
    {
      self->start = g_new (_T_, new_size);
      *self->start = *(_T_[1]) { 0 };
    }
  else
#endif
    self->start = g_renew (_T_, self->start, new_size);

  self->end = self->start + size;
  self->end_allocation = self->start + new_size;
#ifdef GDK_ARRAY_NULL_TERMINATED
  self->end_allocation--;
#endif
}

G_GNUC_UNUSED static inline void
gdk_array(splice) (GdkArray *self,
                   gsize      pos,
                   gsize      removed,
                   gboolean   stolen,
                   _T_       *additions,
                   gsize      added)
{
  gsize size;
  gsize remaining;

  size = gdk_array(get_size) (self);
  g_assert (pos + removed <= size);
  remaining = size - pos - removed;

  if (!stolen)
    gdk_array(free_elements) (gdk_array(index) (self, pos),
                              gdk_array(index) (self, pos + removed));

  gdk_array(reserve) (self, size - removed + added);

  if (GDK_ARRAY_REAL_SIZE (remaining) && removed != added)
    memmove (gdk_array(index) (self, pos + added),
             gdk_array(index) (self, pos + removed),
             GDK_ARRAY_REAL_SIZE (remaining) * sizeof (_T_));

  if (added)
    {
      if (additions)
        memcpy (gdk_array(index) (self, pos),
                additions,
                added * sizeof (_T_));
#ifndef GDK_ARRAY_NO_MEMSET
      else
        memset (gdk_array(index) (self, pos), 0, added * sizeof (_T_));
#endif
    }


  /* might overflow, but does the right thing */
  self->end += added - removed;
}

G_GNUC_UNUSED static void
gdk_array(set_size) (GdkArray *self,
                     gsize     new_size)
{
  gsize old_size = gdk_array(get_size) (self);
  if (new_size > old_size)
    gdk_array(splice) (self, old_size, 0, FALSE, NULL, new_size - old_size);
  else
    gdk_array(splice) (self, new_size, old_size - new_size, FALSE, NULL, 0);
}

G_GNUC_UNUSED static void
gdk_array(append) (GdkArray *self,
#ifdef GDK_ARRAY_BY_VALUE
                   _T_       *value)
#else
                   _T_        value)
#endif
{
  gdk_array(splice) (self,
                     gdk_array(get_size) (self),
                     0,
                     FALSE,
#ifdef GDK_ARRAY_BY_VALUE
                     value,
#else
                     &value,
#endif
                     1);
}

#ifdef GDK_ARRAY_BY_VALUE
G_GNUC_UNUSED static _T_ *
gdk_array(get) (const GdkArray *self,
                gsize           pos)
{
  return gdk_array(index) (self, pos);
}
#else
G_GNUC_UNUSED static _T_
gdk_array(get) (const GdkArray *self,
                gsize           pos)
 {
   return *gdk_array(index) (self, pos);
 }
#endif

#ifndef GDK_ARRAY_NO_UNDEF

#undef _T_
#undef GdkArray
#undef gdk_array_paste_more
#undef gdk_array_paste
#undef gdk_array
#undef GDK_ARRAY_REAL_SIZE

#undef GDK_ARRAY_BY_VALUE
#undef GDK_ARRAY_ELEMENT_TYPE
#undef GDK_ARRAY_FREE_FUNC
#undef GDK_ARRAY_NAME
#undef GDK_ARRAY_NULL_TERMINATED
#undef GDK_ARRAY_PREALLOC
#undef GDK_ARRAY_TYPE_NAME
#undef GDK_ARRAY_NO_MEMSET
#endif

G_END_DECLS
