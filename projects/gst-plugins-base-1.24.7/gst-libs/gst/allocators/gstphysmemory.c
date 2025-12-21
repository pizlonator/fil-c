/* GStreamer
 * Copyright (C) 2017 Sebastian Dröge <sebastian@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gstphysmemoryallocator
 * @title: GstPhysMemoryAllocator
 * @short_description: Interface for allocators that pass around physical memory addresses
 * @see_also: #GstMemory
 *
 * Since: 1.14
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstphysmemory.h"

G_DEFINE_INTERFACE (GstPhysMemoryAllocator, gst_phys_memory_allocator,
    GST_TYPE_ALLOCATOR);

static void
gst_phys_memory_allocator_default_init (GstPhysMemoryAllocatorInterface * iface)
{
}

/**
 * gst_is_phys_memory:
 * @mem: a #GstMemory
 *
 * Returns: whether the memory at @mem is backed by physical memory
 *
 * Since: 1.14
 */
gboolean
gst_is_phys_memory (GstMemory * mem)
{
  return mem != NULL && mem->allocator != NULL
      && g_type_is_a (G_OBJECT_TYPE (mem->allocator),
      GST_TYPE_PHYS_MEMORY_ALLOCATOR);
}

/**
 * gst_phys_memory_get_phys_addr:
 * @mem: a #GstMemory
 *
 * Returns: Physical memory address that is backing @mem, or 0 if none
 *
 * Since: 1.14
 */
guintptr
gst_phys_memory_get_phys_addr (GstMemory * mem)
{
  GstPhysMemoryAllocatorInterface *iface;

  g_return_val_if_fail (gst_is_phys_memory (mem), 0);

  iface = GST_PHYS_MEMORY_ALLOCATOR_GET_IFACE (mem->allocator);
  g_return_val_if_fail (iface->get_phys_addr != NULL, 0);

  return iface->get_phys_addr ((GstPhysMemoryAllocator *) mem->allocator, mem);
}
