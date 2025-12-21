/* GStreamer
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_CD_PARANOIA_SRC_H__
#define __GST_CD_PARANOIA_SRC_H__

#include <gst/audio/gstaudiocdsrc.h>

G_BEGIN_DECLS

/* on OSX the cdparanoia headers include IOKit framework headers (in particular
 * SCSICmds_INQUIRY_Definitions.h) which define a structure that has a member
 * named VERSION, so we must #undef VERSION here for things to compile on OSX */
static const char GST_PLUGINS_BASE_VERSION[] = VERSION;
#undef VERSION

#ifdef CDPARANOIA_HEADERS_IN_DIR
  #include <cdda/cdda_interface.h>
  #include <cdda/cdda_paranoia.h>
#else
  #include <cdda_interface.h>
  #include <cdda_paranoia.h>
#endif

#define GST_TYPE_CD_PARANOIA_SRC            (gst_cd_paranoia_src_get_type())
#define GST_CD_PARANOIA_SRC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CD_PARANOIA_SRC,GstCdParanoiaSrc))
#define GST_CD_PARANOIA_SRC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CD_PARANOIA_SRC,GstCdParanoiaSrcClass))
#define GST_IS_CD_PARANOIA_SRC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CD_PARANOIA_SRC))
#define GST_IS_CD_PARANOIA_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CD_PARANOIA_SRC))

typedef struct _GstCdParanoiaSrc GstCdParanoiaSrc;
typedef struct _GstCdParanoiaSrcClass GstCdParanoiaSrcClass;

/**
 * GstCdParanoiaSrc:
 *
 * The cdparanoia object structure.
 */
struct _GstCdParanoiaSrc {
  GstAudioCdSrc   audiocdsrc;

  /*< private >*/
  cdrom_drive     *d;
  cdrom_paranoia  *p;

  gint             next_sector; /* -1 or next sector we expect to
                                 * read, so we know when to do a seek */

  gint             paranoia_mode;
  gint             read_speed;
  gint             search_overlap;
  gint             cache_size;

  gchar           *generic_device;
};

struct _GstCdParanoiaSrcClass {
  GstAudioCdSrcClass parent_class;

  /* signal callbacks */
  /**
   * GstCdParanoiaSrcClass::transport-error:
   * @src: the GstAudioCdSrc source element object
   * @sector: the sector at which the error happened
   *
   * This signal is emitted when a sector could not be read
   * because of a transport error.
   */
  void (*transport_error)	(GstCdParanoiaSrc * src, gint sector);
  /**
   * GstCdParanoiaSrcClass::uncorrected-error:
   * @src: the GstAudioCdSrc source element object
   * @sector: the sector at which the error happened
   *
   * This signal is emitted when a sector could not be read
   * because of a transport error.
   */
  void (*uncorrected_error)	(GstCdParanoiaSrc * src, gint sector);
};

GST_ELEMENT_REGISTER_DECLARE (cdparanoiasrc);

GType    gst_cd_paranoia_src_get_type (void);

G_END_DECLS

#endif /* __GST_CD_PARANOIA_SRC_H__ */

