/*
 * Copyright © 2011 Red Hat Inc.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */


static inline GtkBitmask *
_gtk_bitmask_new (void)
{
  return _gtk_bitmask_from_bits (0);
}

static inline GtkBitmask *
_gtk_bitmask_copy (const GtkBitmask *mask)
{
  if (_gtk_bitmask_is_allocated (mask))
    return _gtk_allocated_bitmask_copy (mask);
  else
    return (GtkBitmask *) mask;
}

static inline void
_gtk_bitmask_free (GtkBitmask *mask)
{
  if (_gtk_bitmask_is_allocated (mask))
    _gtk_allocated_bitmask_free (mask);
}

static inline char *
_gtk_bitmask_to_string (const GtkBitmask *mask)
{
  GString *string;
  
  string = g_string_new (NULL);
  _gtk_allocated_bitmask_print (mask, string);
  return g_string_free (string, FALSE);
}

static inline void
_gtk_bitmask_print (const GtkBitmask *mask,
                    GString          *string)
{
  _gtk_allocated_bitmask_print (mask, string);
}

static inline GtkBitmask *
_gtk_bitmask_intersect (GtkBitmask       *mask,
                        const GtkBitmask *other)
{
  return _gtk_allocated_bitmask_intersect (mask, other);
}

static inline GtkBitmask *
_gtk_bitmask_union (GtkBitmask       *mask,
                    const GtkBitmask *other)
{
  if (_gtk_bitmask_is_allocated (mask) ||
      _gtk_bitmask_is_allocated (other))
    return _gtk_allocated_bitmask_union (mask, other);
  else
    return _gtk_bitmask_from_bits (_gtk_bitmask_to_bits (mask)
                                   | _gtk_bitmask_to_bits (other));
}

static inline GtkBitmask *
_gtk_bitmask_subtract (GtkBitmask       *mask,
                       const GtkBitmask *other)
{
  return _gtk_allocated_bitmask_subtract (mask, other);
}

static inline gboolean
_gtk_bitmask_get (const GtkBitmask *mask,
                  guint             index_)
{
  if (_gtk_bitmask_is_allocated (mask))
    return _gtk_allocated_bitmask_get (mask, index_);
  else
    return index_ < GTK_BITMASK_N_DIRECT_BITS
           ? !!(_gtk_bitmask_to_bits (mask) & (((gsize) 1) << index_))
           : FALSE;
}

static inline GtkBitmask *
_gtk_bitmask_set (GtkBitmask *mask,
                  guint       index_,
                  gboolean    value)
{
  if (_gtk_bitmask_is_allocated (mask) ||
      (index_ >= GTK_BITMASK_N_DIRECT_BITS && value))
    return _gtk_allocated_bitmask_set (mask, index_, value);
  else if (index_ < GTK_BITMASK_N_DIRECT_BITS)
    {
      gsize bits = _gtk_bitmask_to_bits (mask);

      if (value)
        bits |= ((gsize) 1) << index_;
      else
        bits &= ~(((gsize) 1) << index_);

      return _gtk_bitmask_from_bits (bits);
    }
  else
    return mask;
}

static inline GtkBitmask *
_gtk_bitmask_invert_range (GtkBitmask *mask,
                           guint       start,
                           guint       end)
{
  g_assert (start <= end);

  if (_gtk_bitmask_is_allocated (mask) ||
      (end > GTK_BITMASK_N_DIRECT_BITS))
    return _gtk_allocated_bitmask_invert_range (mask, start, end);
  else
    {
      gsize invert = (((gsize) 1) << end) - (((gsize) 1) << start);
      
      return _gtk_bitmask_from_bits (_gtk_bitmask_to_bits (mask) ^ invert);
    }
}

static inline gboolean
_gtk_bitmask_is_empty (const GtkBitmask *mask)
{
  return mask == _gtk_bitmask_from_bits (0);
}

static inline gboolean
_gtk_bitmask_equals (const GtkBitmask *mask,
                     const GtkBitmask *other)
{
  if (mask == other)
    return TRUE;

  if (!_gtk_bitmask_is_allocated (mask) ||
      !_gtk_bitmask_is_allocated (other))
    return FALSE;

  return _gtk_allocated_bitmask_equals (mask, other);
}

static inline gboolean
_gtk_bitmask_intersects (const GtkBitmask *mask,
                         const GtkBitmask *other)
{
  if (_gtk_bitmask_is_allocated (mask) ||
      _gtk_bitmask_is_allocated (other))
    return _gtk_allocated_bitmask_intersects (mask, other);
  else
    return _gtk_bitmask_to_bits (mask) & _gtk_bitmask_to_bits (other) ? TRUE : FALSE;
}
