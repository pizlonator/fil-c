/* GStreamer
 * Copyright (C) 2010 Oblong Industries, Inc.
 * Copyright (C) 2010 Collabora Multimedia
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

#ifndef __JP2K_CODESTREAM_H__
#define __JP2K_CODESTREAM_H__

#include <gst/gst.h>
#include <gst/base/gstbytereader.h>
#include <gst/base/gstbitreader.h>
#include <gst/base/gstbytewriter.h>

#include "gstjp2kdecimator.h"

/* Used to represent codestream packets */
typedef struct
{
  gboolean sop;
  gboolean eph;
  guint16 seqno;

  const guint8 *data;
  guint length;
} Packet;

/* Used to represent unparsed markers for passthrough */
typedef struct
{
  const guint8 *data;
  guint length;
} Buffer;

typedef struct
{
  guint8 s;                     /* sample precision */
  guint8 xr, yr;                /* resolution */
} ComponentSize;

/* SIZ */
typedef struct
{
  guint16 caps;                 /* capabilities */
  guint32 x, y;                 /* reference grid size */
  guint32 xo, yo;               /* origin */
  ComponentSize *components;
  guint16 n_components;
  guint32 xt, yt;               /* tile sizes */
  guint32 xto, yto;             /* tile origin */
} ImageSize;

/* Progression orders
 * L - layer
 * R - resolution/decomposition level
 * C - component
 * P - position/precinct
 */
typedef enum
{
  PROGRESSION_ORDER_LRCP = 0,
  PROGRESSION_ORDER_RLCP,
  PROGRESSION_ORDER_RPCL,
  PROGRESSION_ORDER_PCRL,
  PROGRESSION_ORDER_CPRL,
  PROGRESSION_ORDER_MAX
} ProgressionOrder;

/* COD */
typedef struct
{
  /* Scod */
  gboolean sop, eph;
  /* SGcod */
  ProgressionOrder progression_order;
  guint16 n_layers;
  guint8 multi_component_transform;
  /* SPcod */
  guint8 n_decompositions;
  guint8 xcb, ycb;              /* code block dimensions */
  guint8 code_block_style;
  guint8 transformation;
  guint8 *PPx, *PPy;            /* precinct sizes (default:15,
                                 * otherwise n_decompositions+1 elements) */
} CodingStyleDefault;

/* SOT */
typedef struct
{
  guint16 tile_index;
  guint32 tile_part_size;
  guint8 tile_part_index, n_tile_parts;
} StartOfTile;

/* PLT */
typedef struct
{
  guint8 index;
  GArray *packet_lengths;       /* array of guint32 */
} PacketLengthTilePart;

typedef struct
{
  StartOfTile sot;
  CodingStyleDefault *cod;

  Buffer *qcd;
  GList *qcc;                   /* list of Buffer */

  GList *plt;                   /* list of PacketLengthTilePart */

  GList *com;                   /* list of Buffer */

  GList *packets;               /* list of Packet, codestream */

  /* TODO: COC, PPT */

  /* Calculated value */
  gint tile_x, tile_y;
  gint tx0, tx1, ty0, ty1;      /* tile dimensions */
} Tile;

typedef struct
{
  /* Parsed values */
  ImageSize siz;
  CodingStyleDefault cod;

  Buffer qcd;
  GList *qcc;                   /* list of Buffer */
  GList *crg, *com;             /* lists of Buffer */

  /* TODO: COC, PPM, TLM, PLM */

  guint n_tiles_x, n_tiles_y, n_tiles;  /* calculated */
  Tile *tiles;
} MainHeader;

typedef struct _PacketIterator PacketIterator;
struct _PacketIterator
{
  gboolean (*next) (PacketIterator * it);
  const MainHeader *header;
  const Tile *tile;

  gboolean first;

  gint cur_layer;
  gint cur_resolution;
  gint cur_component;
  gint cur_precinct;
  gint cur_x, cur_y;

  gint n_layers;
  gint n_resolutions;
  gint n_components;
  gint n_precincts, n_precincts_w, n_precincts_h;

  gint tx0, tx1, ty0, ty1;
  gint x_step, y_step;

  /* cached calculated values */
  /* depends on resolution and component */
  gint tcx0, tcx1, tcy0, tcy1;
  gint trx0, trx1, try0, try1;
  gint tpx0, tpx1, tpy0, tpy1;
  gint yr, xr;
  gint two_nl_r, two_ppx, two_ppy;

  gint cur_packet;
};

GstFlowReturn parse_main_header (GstJP2kDecimator * self, GstByteReader * reader, MainHeader * header);
guint sizeof_main_header (GstJP2kDecimator * self, const MainHeader * header);
void reset_main_header (GstJP2kDecimator * self, MainHeader * header);
GstFlowReturn write_main_header (GstJP2kDecimator * self, GstByteWriter * writer, const MainHeader * header);
GstFlowReturn decimate_main_header (GstJP2kDecimator * self, MainHeader * header);

#endif /* __JP2K_CODESTREAM_H__ */
