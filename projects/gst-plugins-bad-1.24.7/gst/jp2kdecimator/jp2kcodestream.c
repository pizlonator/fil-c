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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "jp2kcodestream.h"

GST_DEBUG_CATEGORY_EXTERN (gst_jp2k_decimator_debug);
#define GST_CAT_DEFAULT gst_jp2k_decimator_debug

/* Delimiting markers and marker segments */
#define MARKER_SOC 0xFF4F
#define MARKER_SOT 0xFF90
#define MARKER_SOD 0xFF93
#define MARKER_EOC 0xFFD9

/* Fixed information marker segments */
#define MARKER_SIZ 0xFF51

/* Functional marker segments */
#define MARKER_COD 0xFF52
#define MARKER_COC 0xFF53
#define MARKER_RGN 0xFF5E
#define MARKER_QCD 0xFF5C
#define MARKER_QCC 0xFF5D
#define MARKER_POC 0xFF5F

/* Pointer marker segments */
#define MARKER_PLM 0xFF57
#define MARKER_PLT 0xFF58
#define MARKER_PPM 0xFF60
#define MARKER_PPT 0xFF61
#define MARKER_TLM 0xFF55

/* In-bit-stream markers and marker segments */
#define MARKER_SOP 0xFF91
#define MARKER_EPH 0xFF92

/* Informational marker segments */
#define MARKER_CRG 0xFF63
#define MARKER_COM 0xFF64

static void
packet_iterator_changed_resolution_or_component (PacketIterator * it)
{
  gint tx0, tx1, ty0, ty1;
  gint tcx0, tcx1, tcy0, tcy1;
  gint trx0, trx1, try0, try1;
  gint tpx0, tpx1, tpy0, tpy1;
  gint two_nl_r;
  gint two_ppx, two_ppy;
  gint xr, yr;
  guint8 *PPx, *PPy;

  tx0 = it->tile->tx0;
  tx1 = it->tile->tx1;
  ty0 = it->tile->ty0;
  ty1 = it->tile->ty1;

  it->two_nl_r = two_nl_r = (1 << (it->n_resolutions - it->cur_resolution - 1));

  PPx = it->tile->cod ? it->tile->cod->PPx : it->header->cod.PPx;
  PPy = it->tile->cod ? it->tile->cod->PPy : it->header->cod.PPy;
  it->two_ppx = two_ppx = (1 << (PPx ? PPx[it->cur_resolution] : 15));
  it->two_ppy = two_ppy = (1 << (PPy ? PPy[it->cur_resolution] : 15));

  it->xr = xr = it->header->siz.components[it->cur_component].xr;
  it->yr = yr = it->header->siz.components[it->cur_component].yr;

  it->tcx0 = tcx0 = (tx0 + xr - 1) / xr;
  it->tcx1 = tcx1 = (tx1 + xr - 1) / xr;
  it->tcy0 = tcy0 = (ty0 + yr - 1) / yr;
  it->tcy1 = tcy1 = (ty1 + yr - 1) / yr;

  it->trx0 = trx0 = (tcx0 + two_nl_r - 1) / two_nl_r;
  it->trx1 = trx1 = (tcx1 + two_nl_r - 1) / two_nl_r;
  it->try0 = try0 = (tcy0 + two_nl_r - 1) / two_nl_r;
  it->try1 = try1 = (tcy1 + two_nl_r - 1) / two_nl_r;

  it->tpx0 = tpx0 = two_ppx * (trx0 / two_ppx);
  it->tpx1 = tpx1 = two_ppx * ((trx1 + two_ppx - 1) / two_ppx);
  it->tpy0 = tpy0 = two_ppy * (try0 / two_ppy);
  it->tpy1 = tpy1 = two_ppy * ((try1 + two_ppy - 1) / two_ppy);

  it->n_precincts_w = (trx0 == trx1) ? 0 : (tpx1 - tpx0) / two_ppx;
  it->n_precincts_h = (try0 == try1) ? 0 : (tpy1 - tpy0) / two_ppy;
  it->n_precincts = it->n_precincts_w * it->n_precincts_h;
}

static gboolean
packet_iterator_next_lrcp (PacketIterator * it)
{
  g_return_val_if_fail (it->cur_layer < it->n_layers, FALSE);

  if (it->first) {
    packet_iterator_changed_resolution_or_component (it);
    it->first = FALSE;
    return TRUE;
  }

  it->cur_precinct += 1;
  if (it->cur_precinct >= it->n_precincts) {
    it->cur_precinct = 0;

    it->cur_component += 1;
    if (it->cur_component >= it->n_components) {
      it->cur_component = 0;

      it->cur_resolution += 1;
      if (it->cur_resolution >= it->n_resolutions) {
        it->cur_resolution = 0;
        it->cur_layer += 1;
        if (it->cur_layer >= it->n_layers) {
          it->cur_packet++;
          return FALSE;
        }
      }
    }
    packet_iterator_changed_resolution_or_component (it);
  }

  it->cur_packet++;

  return TRUE;
}

static gboolean
packet_iterator_next_rlcp (PacketIterator * it)
{
  g_return_val_if_fail (it->cur_resolution < it->n_resolutions, FALSE);

  if (it->first) {
    packet_iterator_changed_resolution_or_component (it);
    it->first = FALSE;
    return TRUE;
  }

  it->cur_precinct += 1;
  if (it->cur_precinct >= it->n_precincts) {
    it->cur_precinct = 0;

    it->cur_component += 1;
    if (it->cur_component >= it->n_components) {
      it->cur_component = 0;

      it->cur_layer += 1;
      if (it->cur_layer >= it->n_layers) {
        it->cur_layer = 0;
        it->cur_resolution += 1;
        if (it->cur_resolution >= it->n_resolutions) {
          it->cur_packet++;
          return FALSE;
        }
      }
    }
    packet_iterator_changed_resolution_or_component (it);
  }

  it->cur_packet++;

  return TRUE;
}

static gboolean
packet_iterator_next_rpcl (PacketIterator * it)
{
  g_return_val_if_fail (it->cur_resolution < it->n_resolutions, FALSE);

  if (it->first) {
    packet_iterator_changed_resolution_or_component (it);
    it->first = FALSE;
    return TRUE;
  }

  it->cur_layer += 1;
  if (it->cur_layer >= it->n_layers) {
    it->cur_layer = 0;

    /* Loop and advance the position and resolution until
     * we find the next precinct
     */
    while (TRUE) {
      it->cur_component += 1;
      if (it->cur_component >= it->n_components) {
        it->cur_component = 0;

        it->cur_x += it->x_step - (it->cur_x % it->x_step);
        if (it->cur_x >= it->tx1) {
          it->cur_x = it->tx0;

          it->cur_y += it->y_step - (it->cur_y % it->y_step);
          if (it->cur_y >= it->ty1) {
            it->cur_y = it->ty0;

            it->cur_resolution += 1;

            if (it->cur_resolution >= it->n_resolutions) {
              it->cur_packet++;
              return FALSE;
            }
          }
        }
      }

      packet_iterator_changed_resolution_or_component (it);

      if (((it->cur_y % (it->yr * it->two_ppy * it->two_nl_r) == 0)
              || (it->cur_y == it->ty0
                  && ((it->try0 * it->two_nl_r) %
                      (it->two_ppy * it->two_nl_r) != 0)))
          && ((it->cur_x % (it->xr * it->two_ppx * it->two_nl_r) == 0)
              || (it->cur_x == it->tx0
                  && ((it->trx0 * it->two_nl_r) %
                      (it->two_ppx * it->two_nl_r) != 0)))) {
        gint k;

        k = (((it->cur_x + it->xr * it->two_nl_r - 1) /
                (it->xr * it->two_nl_r)) / it->two_ppx) -
            (it->trx0 / it->two_ppx) +
            it->n_precincts_w *
            (((it->cur_y + it->yr * it->two_nl_r - 1) /
                (it->yr * it->two_nl_r)) / it->two_ppy);

        g_assert (k < it->n_precincts);

        it->cur_precinct = k;
        break;
      }
    }
  }

  it->cur_packet++;

  return TRUE;
}

static gboolean
packet_iterator_next_pcrl (PacketIterator * it)
{
  g_return_val_if_fail (it->cur_resolution < it->n_resolutions, FALSE);

  if (it->first) {
    it->first = FALSE;
    return TRUE;
  }

  it->cur_layer += 1;
  if (it->cur_layer >= it->n_layers) {
    it->cur_layer = 0;

    /* Loop and advance the position and resolution until
     * we find the next precinct
     */
    while (TRUE) {
      it->cur_resolution += 1;
      if (it->cur_resolution >= it->n_resolutions) {
        it->cur_resolution = 0;

        it->cur_component += 1;
        if (it->cur_component >= it->n_components) {

          it->cur_x += it->x_step - (it->cur_x % it->x_step);
          if (it->cur_x >= it->tx1) {
            it->cur_x = it->tx0;

            it->cur_y += it->y_step - (it->cur_y % it->y_step);
            if (it->cur_y >= it->ty1) {
              it->cur_packet++;
              return FALSE;
            }
          }
        }
      }

      packet_iterator_changed_resolution_or_component (it);

      if (((it->cur_y % (it->yr * it->two_ppy * it->two_nl_r) == 0)
              || (it->cur_y == it->ty0
                  && ((it->try0 * it->two_nl_r) %
                      (it->two_ppy * it->two_nl_r) != 0)))
          && ((it->cur_x % (it->xr * it->two_ppx * it->two_nl_r) == 0)
              || (it->cur_x == it->tx0
                  && ((it->trx0 * it->two_nl_r) %
                      (it->two_ppx * it->two_nl_r) != 0)))) {
        gint k;

        k = (((it->cur_x + it->xr * it->two_nl_r - 1) /
                (it->xr * it->two_nl_r)) / it->two_ppx) -
            (it->trx0 / it->two_ppx) +
            it->n_precincts_w *
            (((it->cur_y + it->yr * it->two_nl_r - 1) /
                (it->yr * it->two_nl_r)) / it->two_ppy);

        g_assert (k < it->n_precincts);

        it->cur_precinct = k;
        break;
      }
    }
  }

  it->cur_packet++;

  return TRUE;
}

static gboolean
packet_iterator_next_cprl (PacketIterator * it)
{
  g_return_val_if_fail (it->cur_resolution < it->n_resolutions, FALSE);

  if (it->first) {
    packet_iterator_changed_resolution_or_component (it);
    it->first = FALSE;
    return TRUE;
  }

  it->cur_layer += 1;
  if (it->cur_layer >= it->n_layers) {
    it->cur_layer = 0;

    /* Loop and advance the position and resolution until
     * we find the next precinct
     */
    while (TRUE) {
      it->cur_resolution += 1;
      if (it->cur_resolution >= it->n_resolutions) {
        it->cur_resolution = 0;

        it->cur_x += it->x_step - (it->cur_x % it->x_step);
        if (it->cur_x >= it->tx1) {
          it->cur_x = it->tx0;

          it->cur_y += it->y_step - (it->cur_y % it->y_step);
          if (it->cur_y >= it->ty1) {
            it->cur_y = it->ty0;

            it->cur_component += 1;

            if (it->cur_component >= it->n_components) {
              it->cur_packet++;
              return FALSE;
            }
          }
        }
      }

      packet_iterator_changed_resolution_or_component (it);

      if (((it->cur_y % (it->yr * it->two_ppy * it->two_nl_r) == 0)
              || (it->cur_y == it->ty0
                  && ((it->try0 * it->two_nl_r) %
                      (it->two_ppy * it->two_nl_r) != 0)))
          && ((it->cur_x % (it->xr * it->two_ppx * it->two_nl_r) == 0)
              || (it->cur_x == it->tx0
                  && ((it->trx0 * it->two_nl_r) %
                      (it->two_ppx * it->two_nl_r) != 0)))) {
        gint k;

        k = (((it->cur_x + it->xr * it->two_nl_r - 1) /
                (it->xr * it->two_nl_r)) / it->two_ppx) -
            (it->trx0 / it->two_ppx) +
            it->n_precincts_w *
            (((it->cur_y + it->yr * it->two_nl_r - 1) /
                (it->yr * it->two_nl_r)) / it->two_ppy);

        g_assert (k < it->n_precincts);

        it->cur_precinct = k;
        break;
      }
    }
  }

  it->cur_packet++;

  return TRUE;
}

static GstFlowReturn
init_packet_iterator (GstJP2kDecimator * self, PacketIterator * it,
    const MainHeader * header, const Tile * tile)
{
  ProgressionOrder order;
  gint i, j;

  memset (it, 0, sizeof (PacketIterator));

  it->header = header;
  it->tile = tile;

  it->first = TRUE;

  it->n_layers = (tile->cod) ? tile->cod->n_layers : header->cod.n_layers;
  it->n_resolutions =
      1 +
      ((tile->cod) ? tile->cod->n_decompositions : header->cod.
      n_decompositions);
  it->n_components = header->siz.n_components;

  it->tx0 = tile->tx0;
  it->tx1 = tile->tx1;
  it->ty0 = tile->ty0;
  it->ty1 = tile->ty1;

  it->cur_x = it->tx0;
  it->cur_y = it->ty0;

  /* Calculate the step sizes for the position-dependent progression orders */
  it->x_step = it->y_step = 0;
  for (i = 0; i < it->n_components; i++) {
    gint xr, yr;

    xr = header->siz.components[i].xr;
    yr = header->siz.components[i].yr;


    for (j = 0; j < it->n_resolutions; j++) {
      gint xs, ys;
      guint8 PPx, PPy;

      if (tile->cod) {
        PPx = (tile->cod->PPx) ? tile->cod->PPx[j] : 15;
        PPy = (tile->cod->PPy) ? tile->cod->PPy[j] : 15;
      } else {
        PPx = (header->cod.PPx) ? header->cod.PPx[j] : 15;
        PPy = (header->cod.PPy) ? header->cod.PPy[j] : 15;
      }

      xs = xr * (1 << (PPx + it->n_resolutions - j - 1));
      ys = yr * (1 << (PPy + it->n_resolutions - j - 1));

      if (it->x_step == 0 || it->x_step > xs)
        it->x_step = xs;
      if (it->y_step == 0 || it->y_step > ys)
        it->y_step = ys;
    }
  }

  order =
      (tile->cod) ? tile->cod->progression_order : header->cod.
      progression_order;
  if (order == PROGRESSION_ORDER_LRCP) {
    it->next = packet_iterator_next_lrcp;
  } else if (order == PROGRESSION_ORDER_RLCP) {
    it->next = packet_iterator_next_rlcp;
  } else if (order == PROGRESSION_ORDER_RPCL) {
    it->next = packet_iterator_next_rpcl;
  } else if (order == PROGRESSION_ORDER_PCRL) {
    it->next = packet_iterator_next_pcrl;
  } else if (order == PROGRESSION_ORDER_CPRL) {
    it->next = packet_iterator_next_cprl;
  } else {
    GST_ERROR_OBJECT (self, "Progression order %d not supported", order);
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
parse_siz (GstJP2kDecimator * self, GstByteReader * reader,
    ImageSize * siz, guint16 length)
{
  gint i;

  if (length < 38) {
    GST_ERROR_OBJECT (self, "Invalid SIZ marker");
    return GST_FLOW_ERROR;
  }

  siz->caps = gst_byte_reader_get_uint16_be_unchecked (reader);
  siz->x = gst_byte_reader_get_uint32_be_unchecked (reader);
  siz->y = gst_byte_reader_get_uint32_be_unchecked (reader);
  siz->xo = gst_byte_reader_get_uint32_be_unchecked (reader);
  siz->yo = gst_byte_reader_get_uint32_be_unchecked (reader);
  siz->xt = gst_byte_reader_get_uint32_be_unchecked (reader);
  siz->yt = gst_byte_reader_get_uint32_be_unchecked (reader);
  siz->xto = gst_byte_reader_get_uint32_be_unchecked (reader);
  siz->yto = gst_byte_reader_get_uint32_be_unchecked (reader);
  siz->n_components = gst_byte_reader_get_uint16_be_unchecked (reader);

  if (length < 38 + 3 * siz->n_components) {
    GST_ERROR_OBJECT (self, "Invalid SIZ marker");
    return GST_FLOW_ERROR;
  }

  siz->components = g_new (ComponentSize, siz->n_components);
  for (i = 0; i < siz->n_components; i++) {
    siz->components[i].s = gst_byte_reader_get_uint8_unchecked (reader);
    siz->components[i].xr = gst_byte_reader_get_uint8_unchecked (reader);
    siz->components[i].yr = gst_byte_reader_get_uint8_unchecked (reader);
  }

  return GST_FLOW_OK;
}

static guint
sizeof_siz (GstJP2kDecimator * self, const ImageSize * siz)
{
  return 2 + 38 + 3 * siz->n_components;
}

static void
reset_siz (GstJP2kDecimator * self, ImageSize * siz)
{
  if (siz->components)
    g_free (siz->components);
  memset (siz, 0, sizeof (ImageSize));
}

static GstFlowReturn
write_siz (GstJP2kDecimator * self, GstByteWriter * writer,
    const ImageSize * siz)
{
  gint i;

  if (!gst_byte_writer_ensure_free_space (writer,
          2 + 38 + 3 * siz->n_components)) {
    GST_ERROR_OBJECT (self, "Could not ensure free space");
    return GST_FLOW_ERROR;
  }

  gst_byte_writer_put_uint16_be_unchecked (writer, MARKER_SIZ);
  gst_byte_writer_put_uint16_be_unchecked (writer, 38 + 3 * siz->n_components);
  gst_byte_writer_put_uint16_be_unchecked (writer, siz->caps);
  gst_byte_writer_put_uint32_be_unchecked (writer, siz->x);
  gst_byte_writer_put_uint32_be_unchecked (writer, siz->y);
  gst_byte_writer_put_uint32_be_unchecked (writer, siz->xo);
  gst_byte_writer_put_uint32_be_unchecked (writer, siz->yo);
  gst_byte_writer_put_uint32_be_unchecked (writer, siz->xt);
  gst_byte_writer_put_uint32_be_unchecked (writer, siz->yt);
  gst_byte_writer_put_uint32_be_unchecked (writer, siz->xto);
  gst_byte_writer_put_uint32_be_unchecked (writer, siz->yto);
  gst_byte_writer_put_uint16_be_unchecked (writer, siz->n_components);

  for (i = 0; i < siz->n_components; i++) {
    gst_byte_writer_put_uint8_unchecked (writer, siz->components[i].s);
    gst_byte_writer_put_uint8_unchecked (writer, siz->components[i].xr);
    gst_byte_writer_put_uint8_unchecked (writer, siz->components[i].yr);
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
parse_cod (GstJP2kDecimator * self, GstByteReader * reader,
    CodingStyleDefault * cod, guint16 length)
{
  guint8 Scod;

  if (length < 12) {
    GST_ERROR_OBJECT (self, "Invalid COD marker");
    return GST_FLOW_ERROR;
  }

  Scod = gst_byte_reader_get_uint8_unchecked (reader);
  cod->sop = !!(Scod & 0x02);
  cod->eph = !!(Scod & 0x04);

  /* SGcod */
  cod->progression_order = gst_byte_reader_get_uint8_unchecked (reader);
  cod->n_layers = gst_byte_reader_get_uint16_be_unchecked (reader);
  cod->multi_component_transform = gst_byte_reader_get_uint8_unchecked (reader);

  /* SPcod */
  cod->n_decompositions = gst_byte_reader_get_uint8_unchecked (reader);
  cod->xcb = gst_byte_reader_get_uint8_unchecked (reader) + 2;
  cod->ycb = gst_byte_reader_get_uint8_unchecked (reader) + 2;
  cod->code_block_style = gst_byte_reader_get_uint8_unchecked (reader);
  cod->transformation = gst_byte_reader_get_uint8_unchecked (reader);

  if ((Scod & 0x01)) {
    gint i;
    /* User defined precincts */

    if (length < 12 + (Scod & 0x01) * (cod->n_decompositions + 1)) {
      GST_ERROR_OBJECT (self, "Invalid COD marker");
      return GST_FLOW_ERROR;
    }

    cod->PPx = g_new (guint8, (cod->n_decompositions + 1));
    for (i = 0; i < cod->n_decompositions + 1; i++) {
      guint8 v = gst_byte_reader_get_uint8_unchecked (reader);
      cod->PPx[i] = (v & 0x0f);
      cod->PPy[i] = (v >> 4);
    }
  }

  return GST_FLOW_OK;
}

static guint
sizeof_cod (GstJP2kDecimator * self, const CodingStyleDefault * cod)
{
  return 2 + 12 + (cod->PPx ? (cod->n_decompositions + 1) : 0);
}

static void
reset_cod (GstJP2kDecimator * self, CodingStyleDefault * cod)
{
  g_free (cod->PPx);
  g_free (cod->PPy);
  memset (cod, 0, sizeof (CodingStyleDefault));
}

static GstFlowReturn
write_cod (GstJP2kDecimator * self, GstByteWriter * writer,
    const CodingStyleDefault * cod)
{
  guint tmp;

  tmp = 12 + (cod->PPx ? (1 + cod->n_decompositions) : 0);
  if (!gst_byte_writer_ensure_free_space (writer, tmp)) {
    GST_ERROR_OBJECT (self, "Could not ensure free space");
    return GST_FLOW_ERROR;
  }

  gst_byte_writer_put_uint16_be_unchecked (writer, MARKER_COD);
  gst_byte_writer_put_uint16_be_unchecked (writer, tmp);

  /* Scod */
  tmp =
      (cod->PPx ? 0x01 : 0x00) | (cod->sop ? 0x02 : 0x00) | (cod->
      eph ? 0x04 : 0x00);
  gst_byte_writer_put_uint8_unchecked (writer, tmp);

  /* SGcod */
  gst_byte_writer_put_uint8_unchecked (writer, cod->progression_order);
  gst_byte_writer_put_uint16_be_unchecked (writer, cod->n_layers);
  gst_byte_writer_put_uint8_unchecked (writer, cod->multi_component_transform);

  /* SPcod */
  gst_byte_writer_put_uint8_unchecked (writer, cod->n_decompositions);
  gst_byte_writer_put_uint8_unchecked (writer, cod->xcb - 2);
  gst_byte_writer_put_uint8_unchecked (writer, cod->ycb - 2);
  gst_byte_writer_put_uint8_unchecked (writer, cod->code_block_style);
  gst_byte_writer_put_uint8_unchecked (writer, cod->transformation);

  if (cod->PPx) {
    gint i;

    for (i = 0; i < cod->n_decompositions + 1; i++) {
      tmp = (cod->PPx[i]) | (cod->PPy[i] << 4);
      gst_byte_writer_put_uint8_unchecked (writer, tmp);
    }
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
parse_plt (GstJP2kDecimator * self, GstByteReader * reader,
    PacketLengthTilePart * plt, guint length)
{
  guint32 n;
  guint8 b = 0;
  gint i;

  if (length < 3) {
    GST_ERROR_OBJECT (self, "Invalid PLT");
    return GST_FLOW_ERROR;
  }

  plt->index = gst_byte_reader_get_uint8_unchecked (reader);
  plt->packet_lengths = g_array_new (FALSE, FALSE, sizeof (guint32));

  length -= 3;

  n = 0;
  for (i = 0; i < length; i++) {
    b = gst_byte_reader_get_uint8_unchecked (reader);

    if ((n & 0xfe000000)) {
      GST_ERROR_OBJECT (self, "PLT element overflow");
      return GST_FLOW_ERROR;
    }

    n = (n << 7) | (b & 0x7f);
    if ((b & 0x80) == 0x00) {
      g_array_append_val (plt->packet_lengths, n);
      n = 0;
    }
  }

  if ((b & 0x80) != 0x00) {
    GST_ERROR_OBJECT (self, "Truncated PLT");
    return GST_FLOW_ERROR;
  }
  return GST_FLOW_OK;
}

static guint
sizeof_plt (GstJP2kDecimator * self, const PacketLengthTilePart * plt)
{
  guint size = 2 + 3;
  gint i, n;

  n = plt->packet_lengths->len;
  for (i = 0; i < n; i++) {
    guint32 len = g_array_index (plt->packet_lengths, guint32, i);

    if (len < (1 << 7)) {
      size += 1;
    } else if (len < (1 << 14)) {
      size += 2;
    } else if (len < (1 << 21)) {
      size += 3;
    } else if (len < (1 << 28)) {
      size += 4;
    } else {
      size += 5;
    }
  }

  return size;
}

static void
reset_plt (GstJP2kDecimator * self, PacketLengthTilePart * plt)
{
  if (plt->packet_lengths)
    g_array_free (plt->packet_lengths, TRUE);
  memset (plt, 0, sizeof (PacketLengthTilePart));
}

static GstFlowReturn
write_plt (GstJP2kDecimator * self, GstByteWriter * writer,
    const PacketLengthTilePart * plt)
{
  gint i, n;
  guint plt_start_pos, plt_end_pos;

  if (!gst_byte_writer_ensure_free_space (writer, 2 + 2 + 1)) {
    GST_ERROR_OBJECT (self, "Could not ensure free space");
    return GST_FLOW_ERROR;
  }

  gst_byte_writer_put_uint16_be_unchecked (writer, MARKER_PLT);
  plt_start_pos = gst_byte_writer_get_pos (writer);
  gst_byte_writer_put_uint16_be_unchecked (writer, 0);

  gst_byte_writer_put_uint8_unchecked (writer, plt->index);

  n = plt->packet_lengths->len;
  for (i = 0; i < n; i++) {
    guint32 len = g_array_index (plt->packet_lengths, guint32, i);

    /* FIXME: Write multiple plt here */
    if (gst_byte_writer_get_pos (writer) - plt_start_pos > 65535 - 5) {
      GST_ERROR_OBJECT (self, "Too big PLT");
      return GST_FLOW_ERROR;
    }

    if (len < (1 << 7)) {
      if (!gst_byte_writer_ensure_free_space (writer, 1)) {
        GST_ERROR_OBJECT (self, "Could not ensure free space");
        return GST_FLOW_ERROR;
      }
      gst_byte_writer_put_uint8_unchecked (writer, (0x00 | (len & 0x7f)));
    } else if (len < (1 << 14)) {
      if (!gst_byte_writer_ensure_free_space (writer, 2)) {
        GST_ERROR_OBJECT (self, "Could not ensure free space");
        return GST_FLOW_ERROR;
      }
      gst_byte_writer_put_uint8_unchecked (writer,
          (0x80 | ((len >> 7) & 0x7f)));
      gst_byte_writer_put_uint8_unchecked (writer, (0x00 | (len & 0x7f)));
    } else if (len < (1 << 21)) {
      if (!gst_byte_writer_ensure_free_space (writer, 3)) {
        GST_ERROR_OBJECT (self, "Could not ensure free space");
        return GST_FLOW_ERROR;
      }
      gst_byte_writer_put_uint8_unchecked (writer,
          (0x80 | ((len >> 14) & 0x7f)));
      gst_byte_writer_put_uint8_unchecked (writer,
          (0x80 | ((len >> 7) & 0x7f)));
      gst_byte_writer_put_uint8_unchecked (writer, (0x00 | (len & 0x7f)));
    } else if (len < (1 << 28)) {
      if (!gst_byte_writer_ensure_free_space (writer, 4)) {
        GST_ERROR_OBJECT (self, "Could not ensure free space");
        return GST_FLOW_ERROR;
      }
      gst_byte_writer_put_uint8_unchecked (writer,
          (0x80 | ((len >> 21) & 0x7f)));
      gst_byte_writer_put_uint8_unchecked (writer,
          (0x80 | ((len >> 14) & 0x7f)));
      gst_byte_writer_put_uint8_unchecked (writer,
          (0x80 | ((len >> 7) & 0x7f)));
      gst_byte_writer_put_uint8_unchecked (writer, (0x00 | (len & 0x7f)));
    } else {
      if (!gst_byte_writer_ensure_free_space (writer, 5)) {
        GST_ERROR_OBJECT (self, "Could not ensure free space");
        return GST_FLOW_ERROR;
      }
      gst_byte_writer_put_uint8_unchecked (writer,
          (0x80 | ((len >> 28) & 0x7f)));
      gst_byte_writer_put_uint8_unchecked (writer,
          (0x80 | ((len >> 21) & 0x7f)));
      gst_byte_writer_put_uint8_unchecked (writer,
          (0x80 | ((len >> 14) & 0x7f)));
      gst_byte_writer_put_uint8_unchecked (writer,
          (0x80 | ((len >> 7) & 0x7f)));
      gst_byte_writer_put_uint8_unchecked (writer, (0x00 | (len & 0x7f)));
    }
  }

  plt_end_pos = gst_byte_writer_get_pos (writer);
  gst_byte_writer_set_pos (writer, plt_start_pos);
  if (!gst_byte_writer_put_uint16_be (writer, plt_end_pos - plt_start_pos)) {
    GST_ERROR_OBJECT (self, "Not enough space to write plt size");
    return GST_FLOW_ERROR;
  }

  gst_byte_writer_set_pos (writer, plt_end_pos);

  return GST_FLOW_OK;
}

static GstFlowReturn
parse_packet (GstJP2kDecimator * self, GstByteReader * reader,
    const MainHeader * header, Tile * tile, const PacketIterator * it)
{
  GstFlowReturn ret = GST_FLOW_OK;
  guint16 marker = 0, length;
  guint16 seqno = 0;
  guint packet_start_pos;
  const guint8 *packet_start_data;
  gboolean sop, eph;
  PacketLengthTilePart *plt = NULL;

  sop = (tile->cod) ? tile->cod->sop : header->cod.sop;
  eph = (tile->cod) ? tile->cod->eph : header->cod.eph;
  if (tile->plt) {
    if (g_list_length (tile->plt) > 1) {
      GST_ERROR_OBJECT (self,
          "Only a single PLT per tile is supported currently");
      ret = GST_FLOW_ERROR;
      goto done;
    }
    plt = tile->plt->data;
  }

  if (plt) {
    guint32 length;
    Packet *p;

    if (plt->packet_lengths->len <= it->cur_packet) {
      GST_ERROR_OBJECT (self, "Truncated PLT");
      ret = GST_FLOW_ERROR;
      goto done;
    }

    length = g_array_index (plt->packet_lengths, guint32, it->cur_packet);

    if (gst_byte_reader_get_remaining (reader) < length) {
      GST_ERROR_OBJECT (self, "Truncated file");
      ret = GST_FLOW_ERROR;
      goto done;
    }

    p = g_new0 (Packet, 1);

    /* If there is a SOP keep the seqno */
    if (sop && length > 6) {
      if (!gst_byte_reader_peek_uint16_be (reader, &marker)) {
        GST_ERROR_OBJECT (self, "Truncated file");
        ret = GST_FLOW_ERROR;
        g_free (p);
        goto done;
      }

      if (marker == MARKER_SOP) {
        guint16 dummy;

        gst_byte_reader_skip_unchecked (reader, 2);

        if (!gst_byte_reader_get_uint16_be (reader, &dummy)) {
          GST_ERROR_OBJECT (self, "Truncated file");
          ret = GST_FLOW_ERROR;
          g_free (p);
          goto done;
        }

        if (!gst_byte_reader_get_uint16_be (reader, &seqno)) {
          GST_ERROR_OBJECT (self, "Truncated file");
          ret = GST_FLOW_ERROR;
          g_free (p);
          goto done;
        }
        p->data = gst_byte_reader_peek_data_unchecked (reader);
        p->length = length - 6;
        p->sop = TRUE;
        p->eph = eph;
        p->seqno = seqno;
        gst_byte_reader_skip_unchecked (reader, length - 6);
      }
    }

    if (!p->data) {
      p->data = gst_byte_reader_peek_data_unchecked (reader);
      p->length = length;
      p->sop = FALSE;
      p->eph = eph;
      gst_byte_reader_skip_unchecked (reader, length);
    }

    tile->packets = g_list_prepend (tile->packets, p);
  } else if (sop) {
    if (!gst_byte_reader_peek_uint16_be (reader, &marker)) {
      GST_ERROR_OBJECT (self, "Truncated file");
      ret = GST_FLOW_ERROR;
      goto done;
    }

    if (marker != MARKER_SOP) {
      GST_ERROR_OBJECT (self, "No SOP marker");
      ret = GST_FLOW_EOS;
      goto done;
    }

    gst_byte_reader_skip_unchecked (reader, 2);

    if (!gst_byte_reader_get_uint16_be (reader, &length)) {
      GST_ERROR_OBJECT (self, "Truncated file");
      ret = GST_FLOW_ERROR;
      goto done;
    }

    if (!gst_byte_reader_get_uint16_be (reader, &seqno)) {
      GST_ERROR_OBJECT (self, "Truncated file");
      ret = GST_FLOW_ERROR;
      goto done;
    }

    packet_start_data = reader->data + reader->byte;
    packet_start_pos = gst_byte_reader_get_pos (reader);

    /* Find end of packet */
    while (TRUE) {
      if (!gst_byte_reader_peek_uint16_be (reader, &marker)) {
        GST_ERROR_OBJECT (self, "Truncated file");
        ret = GST_FLOW_ERROR;
        goto done;
      }

      if (marker == MARKER_SOP || marker == MARKER_EOC || marker == MARKER_SOT) {
        Packet *p = g_new (Packet, 1);

        p->sop = TRUE;
        p->eph = eph;
        p->seqno = seqno;
        p->data = packet_start_data;
        p->length = reader->byte - packet_start_pos;
        tile->packets = g_list_prepend (tile->packets, p);

        if (marker == MARKER_EOC || marker == MARKER_SOT)
          goto done;
        else
          break;
      }

      gst_byte_reader_skip_unchecked (reader, 1);
    }
  } else {
    GST_ERROR_OBJECT (self, "Either PLT or SOP are required");
    ret = GST_FLOW_ERROR;
    goto done;
  }

done:

  return ret;
}

static guint
sizeof_packet (GstJP2kDecimator * self, const Packet * packet)
{
  return packet->length + (packet->sop ? 6 : 0) + ((packet->eph
          && !packet->data) ? 2 : 0);
}

static GstFlowReturn
parse_packets (GstJP2kDecimator * self, GstByteReader * reader,
    const MainHeader * header, Tile * tile)
{
  guint16 marker = 0;
  GstFlowReturn ret = GST_FLOW_OK;
  PacketIterator it;

  /* Start of data here */
  if (!gst_byte_reader_get_uint16_be (reader, &marker)
      && marker != MARKER_SOD) {
    GST_ERROR_OBJECT (self, "No SOD in tile");
    return GST_FLOW_ERROR;
  }

  ret = init_packet_iterator (self, &it, header, tile);
  if (ret != GST_FLOW_OK)
    goto done;

  while ((it.next (&it))) {
    ret = parse_packet (self, reader, header, tile, &it);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  tile->packets = g_list_reverse (tile->packets);

done:

  return ret;
}

static GstFlowReturn
parse_tile (GstJP2kDecimator * self, GstByteReader * reader,
    const MainHeader * header, Tile * tile)
{
  GstFlowReturn ret = GST_FLOW_OK;
  guint16 marker = 0, length;

  if (!gst_byte_reader_peek_uint16_be (reader, &marker)) {
    GST_ERROR_OBJECT (self, "Could not read marker");
    ret = GST_FLOW_ERROR;
    goto done;
  }

  if (marker != MARKER_SOT) {
    GST_ERROR_OBJECT (self, "Unexpected marker 0x%04x", marker);
    ret = GST_FLOW_ERROR;
    goto done;
  }

  /* Skip marker */
  gst_byte_reader_skip_unchecked (reader, 2);

  if (gst_byte_reader_get_remaining (reader) < 10) {
    GST_ERROR_OBJECT (self, "Invalid SOT marker");
    ret = GST_FLOW_ERROR;
    goto done;
  }

  length = gst_byte_reader_get_uint16_be_unchecked (reader);
  if (length != 10) {
    GST_ERROR_OBJECT (self, "Invalid SOT length");
    ret = GST_FLOW_ERROR;
    goto done;
  }

  /* FIXME: handle multiple tile parts per tile */
  tile->sot.tile_index = gst_byte_reader_get_uint16_be_unchecked (reader);
  tile->sot.tile_part_size = gst_byte_reader_get_uint32_be_unchecked (reader);
  tile->sot.tile_part_index = gst_byte_reader_get_uint8_unchecked (reader);
  tile->sot.n_tile_parts = gst_byte_reader_get_uint8_unchecked (reader);

  if (tile->sot.tile_part_size >
      2 + 10 + gst_byte_reader_get_remaining (reader)) {
    GST_ERROR_OBJECT (self, "Truncated tile part");
    ret = GST_FLOW_ERROR;
    goto done;
  }

  tile->tile_x = tile->sot.tile_index % header->n_tiles_x;
  tile->tile_y = tile->sot.tile_index / header->n_tiles_x;

  tile->tx0 =
      MAX (header->siz.xto + tile->tile_x * header->siz.xt, header->siz.xo);
  tile->ty0 =
      MAX (header->siz.yto + tile->tile_y * header->siz.yt, header->siz.yo);
  tile->tx1 =
      MIN (header->siz.xto + (tile->tile_x + 1) * header->siz.xt,
      header->siz.x);
  tile->ty1 =
      MIN (header->siz.yto + (tile->tile_y + 1) * header->siz.yt,
      header->siz.y);

  /* tile part header */
  while (TRUE) {
    if (!gst_byte_reader_peek_uint16_be (reader, &marker)) {
      GST_ERROR_OBJECT (self, "Could not read marker");
      ret = GST_FLOW_ERROR;
      goto done;
    }

    /* SOD starts the data */
    if (marker == MARKER_SOD) {
      break;
    }

    if ((marker >> 8) != 0xff) {
      GST_ERROR_OBJECT (self, "Lost synchronization (0x%04x)", marker);
      ret = GST_FLOW_ERROR;
      goto done;
    }

    /* Skip the marker */
    gst_byte_reader_skip_unchecked (reader, 2);

    /* All markers here have a length */
    if (!gst_byte_reader_get_uint16_be (reader, &length)) {
      GST_ERROR_OBJECT (self, "Could not read marker length");
      ret = GST_FLOW_ERROR;
      goto done;
    }

    if (length < 2 || gst_byte_reader_get_remaining (reader) < length - 2) {
      GST_ERROR_OBJECT (self, "Invalid marker length %u (available %u)",
          length, gst_byte_reader_get_remaining (reader));
      ret = GST_FLOW_ERROR;
      goto done;
    }

    GST_LOG_OBJECT (self,
        "Tile header Marker 0x%04x at offset %u with length %u", marker,
        gst_byte_reader_get_pos (reader), length);

    switch (marker) {
      case MARKER_COD:
        if (tile->cod) {
          GST_ERROR_OBJECT (self, "Only one COD allowed");
          ret = GST_FLOW_ERROR;
          goto done;
        }

        tile->cod = g_new0 (CodingStyleDefault, 1);
        ret = parse_cod (self, reader, tile->cod, length);
        if (ret != GST_FLOW_OK)
          goto done;
        break;
      case MARKER_COC:
        GST_ERROR_OBJECT (self, "COC marker not supported yet");
        ret = GST_FLOW_ERROR;
        goto done;
      case MARKER_POC:
        GST_ERROR_OBJECT (self, "POC marker not supported yet");
        ret = GST_FLOW_ERROR;
        goto done;
      case MARKER_RGN:
        GST_ERROR_OBJECT (self, "RGN marker not supported yet");
        ret = GST_FLOW_ERROR;
        goto done;
      case MARKER_PPT:
        GST_ERROR_OBJECT (self, "PPT marker not supported yet");
        ret = GST_FLOW_ERROR;
        goto done;
      case MARKER_PLT:{
        PacketLengthTilePart *plt = g_new (PacketLengthTilePart, 1);

        ret = parse_plt (self, reader, plt, length);
        if (ret != GST_FLOW_OK) {
          g_free (plt);
          goto done;
        }

        tile->plt = g_list_append (tile->plt, plt);
        break;
      }
      case MARKER_QCD:
        if (tile->qcd != NULL) {
          GST_ERROR_OBJECT (self, "Multiple QCD markers");
          ret = GST_FLOW_ERROR;
          goto done;
        }
        tile->qcd = g_new (Buffer, 1);
        tile->qcd->data = gst_byte_reader_peek_data_unchecked (reader);
        tile->qcd->length = length - 2;
        gst_byte_reader_skip_unchecked (reader, length - 2);
        break;
      case MARKER_QCC:{
        Buffer *p = g_new (Buffer, 1);
        p->data = gst_byte_reader_peek_data_unchecked (reader);
        p->length = length - 2;
        tile->qcc = g_list_append (tile->qcc, p);
        gst_byte_reader_skip_unchecked (reader, length - 2);
        break;
      }
      case MARKER_COM:{
        Buffer *p = g_new (Buffer, 1);
        p->data = gst_byte_reader_peek_data_unchecked (reader);
        p->length = length - 2;
        tile->com = g_list_append (tile->com, p);
        gst_byte_reader_skip_unchecked (reader, length - 2);
        break;
      }
      default:
        GST_DEBUG_OBJECT (self, "Skipping unknown marker 0x%04x", marker);
        gst_byte_reader_skip_unchecked (reader, length - 2);
        break;
    }
  }

  ret = parse_packets (self, reader, header, tile);

done:

  return ret;
}

static guint
sizeof_tile (GstJP2kDecimator * self, const Tile * tile)
{
  guint size = 0;
  GList *l;

  /* SOT */
  size += 2 + 2 + 2 + 4 + 1 + 1;

  if (tile->cod)
    size += sizeof_cod (self, tile->cod);

  if (tile->qcd)
    size += 2 + 2 + tile->qcd->length;

  for (l = tile->qcc; l; l = l->next) {
    Buffer *b = l->data;
    size += 2 + 2 + b->length;
  }

  for (l = tile->plt; l; l = l->next) {
    PacketLengthTilePart *plt = l->data;
    size += sizeof_plt (self, plt);
  }

  for (l = tile->com; l; l = l->next) {
    Buffer *b = l->data;
    size += 2 + 2 + b->length;
  }

  /* SOD */
  size += 2;

  for (l = tile->packets; l; l = l->next) {
    Packet *p = l->data;
    size += sizeof_packet (self, p);
  }

  return size;
}

static void
reset_tile (GstJP2kDecimator * self, const MainHeader * header, Tile * tile)
{
  GList *l;

  if (tile->cod) {
    reset_cod (self, tile->cod);
    g_free (tile->cod);
  }

  for (l = tile->plt; l; l = l->next) {
    PacketLengthTilePart *plt = l->data;

    reset_plt (self, plt);

    g_free (plt);
  }
  g_list_free (tile->plt);

  if (tile->qcd)
    g_free (tile->qcd);

  for (l = tile->qcc; l; l = l->next) {
    g_free (l->data);
  }
  g_list_free (tile->qcc);

  for (l = tile->com; l; l = l->next) {
    g_free (l->data);
  }
  g_list_free (tile->com);

  for (l = tile->packets; l; l = l->next) {
    Packet *p = l->data;

    g_free (p);
  }
  g_list_free (tile->packets);

  memset (tile, 0, sizeof (Tile));
}

static GstFlowReturn
write_marker_buffer (GstJP2kDecimator * self, GstByteWriter * writer,
    guint16 marker, const Buffer * buffer)
{
  if (!gst_byte_writer_ensure_free_space (writer, 2 + 2 + buffer->length)) {
    GST_ERROR_OBJECT (self, "Could not ensure free space");
    return GST_FLOW_ERROR;
  }

  gst_byte_writer_put_uint16_be_unchecked (writer, marker);
  gst_byte_writer_put_uint16_be_unchecked (writer, buffer->length + 2);
  gst_byte_writer_put_data_unchecked (writer, buffer->data, buffer->length);

  return GST_FLOW_OK;
}

static GstFlowReturn
write_packet (GstJP2kDecimator * self, GstByteWriter * writer,
    const Packet * packet)
{
  guint size = packet->length;

  if (packet->sop)
    size += 6;
  if (packet->eph && !packet->data)
    size += 2;

  if (!gst_byte_writer_ensure_free_space (writer, size)) {
    GST_ERROR_OBJECT (self, "Could not ensure free space");
    return GST_FLOW_ERROR;
  }

  if (packet->sop) {
    gst_byte_writer_put_uint16_be_unchecked (writer, MARKER_SOP);
    gst_byte_writer_put_uint16_be_unchecked (writer, 4);
    gst_byte_writer_put_uint16_be_unchecked (writer, packet->seqno);
  }

  if (packet->data) {
    gst_byte_writer_put_data_unchecked (writer, packet->data, packet->length);
  } else {
    gst_byte_writer_put_uint8_unchecked (writer, 0);
    if (packet->eph) {
      gst_byte_writer_put_uint16_be_unchecked (writer, MARKER_EPH);
    }
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
write_tile (GstJP2kDecimator * self, GstByteWriter * writer,
    const MainHeader * header, Tile * tile)
{
  GList *l;
  GstFlowReturn ret = GST_FLOW_OK;

  if (!gst_byte_writer_ensure_free_space (writer, 12)) {
    GST_ERROR_OBJECT (self, "Could not ensure free space");
    return GST_FLOW_ERROR;
  }

  gst_byte_writer_put_uint16_be_unchecked (writer, MARKER_SOT);
  gst_byte_writer_put_uint16_be_unchecked (writer, 10);

  gst_byte_writer_put_uint16_be_unchecked (writer, tile->sot.tile_index);
  gst_byte_writer_put_uint32_be_unchecked (writer, tile->sot.tile_part_size);
  gst_byte_writer_put_uint8_unchecked (writer, tile->sot.tile_part_index);
  gst_byte_writer_put_uint8_unchecked (writer, tile->sot.n_tile_parts);

  if (tile->cod) {
    ret = write_cod (self, writer, tile->cod);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  if (tile->qcd) {
    ret = write_marker_buffer (self, writer, MARKER_QCD, tile->qcd);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  for (l = tile->qcc; l; l = l->next) {
    Buffer *p = l->data;

    ret = write_marker_buffer (self, writer, MARKER_QCC, p);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  for (l = tile->plt; l; l = l->next) {
    PacketLengthTilePart *plt = l->data;

    ret = write_plt (self, writer, plt);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  for (l = tile->com; l; l = l->next) {
    Buffer *p = l->data;

    ret = write_marker_buffer (self, writer, MARKER_COM, p);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  if (!gst_byte_writer_put_uint16_be (writer, MARKER_SOD)) {
    GST_ERROR_OBJECT (self, "Could not ensure free space");
    ret = GST_FLOW_ERROR;
    goto done;
  }

  for (l = tile->packets; l; l = l->next) {
    Packet *p = l->data;

    ret = write_packet (self, writer, p);
    if (ret != GST_FLOW_OK)
      goto done;
  }

done:

  return ret;
}

GstFlowReturn
parse_main_header (GstJP2kDecimator * self, GstByteReader * reader,
    MainHeader * header)
{
  GstFlowReturn ret = GST_FLOW_OK;
  guint16 marker = 0, length = 0;

  /* First SOC */
  if (!gst_byte_reader_get_uint16_be (reader, &marker)
      || marker != MARKER_SOC) {
    GST_ERROR_OBJECT (self, "Frame does not start with SOC");
    ret = GST_FLOW_ERROR;
    goto done;
  }

  while (TRUE) {
    if (!gst_byte_reader_peek_uint16_be (reader, &marker)) {
      GST_ERROR_OBJECT (self, "Could not read marker");
      ret = GST_FLOW_ERROR;
      goto done;
    }

    /* SOT starts the tiles */
    if (marker == MARKER_SOT) {
      ret = GST_FLOW_OK;
      break;
    } else if (marker == MARKER_EOC) {
      GST_WARNING_OBJECT (self, "EOC marker before SOT");
      ret = GST_FLOW_EOS;
      goto done;
    }

    if ((marker >> 8) != 0xff) {
      GST_ERROR_OBJECT (self, "Lost synchronization (0x%04x)", marker);
      ret = GST_FLOW_ERROR;
      goto done;
    }

    /* Now skip the marker */
    gst_byte_reader_skip_unchecked (reader, 2);

    /* All markers here have a length */
    if (!gst_byte_reader_get_uint16_be (reader, &length)) {
      GST_ERROR_OBJECT (self, "Could not read marker length");
      ret = GST_FLOW_ERROR;
      goto done;
    }

    if (length < 2 || gst_byte_reader_get_remaining (reader) < length - 2) {
      GST_ERROR_OBJECT (self, "Invalid marker length %u (available %u)",
          length, gst_byte_reader_get_remaining (reader));
      ret = GST_FLOW_ERROR;
      goto done;
    }

    GST_LOG_OBJECT (self, "Marker 0x%04x at offset %u with length %u", marker,
        gst_byte_reader_get_pos (reader), length);

    switch (marker) {
      case MARKER_SIZ:

        if (header->siz.n_components != 0) {
          GST_ERROR_OBJECT (self, "Multiple SIZ marker");
          ret = GST_FLOW_ERROR;
          goto done;
        }
        ret = parse_siz (self, reader, &header->siz, length);
        if (ret != GST_FLOW_OK)
          goto done;
        break;
      case MARKER_COD:
        if (header->siz.n_components == 0) {
          GST_ERROR_OBJECT (self, "Require SIZ before COD");
          ret = GST_FLOW_ERROR;
          goto done;
        }

        if (header->cod.n_layers != 0) {
          GST_ERROR_OBJECT (self, "Multiple COD");
          ret = GST_FLOW_ERROR;
          goto done;
        }

        ret = parse_cod (self, reader, &header->cod, length);
        if (ret != GST_FLOW_OK)
          goto done;

        break;
      case MARKER_POC:
        GST_ERROR_OBJECT (self, "POC marker not supported yet");
        ret = GST_FLOW_ERROR;
        goto done;
      case MARKER_COC:
        GST_ERROR_OBJECT (self, "COC marker not supported yet");
        ret = GST_FLOW_ERROR;
        goto done;
      case MARKER_RGN:
        GST_ERROR_OBJECT (self, "RGN marker not supported yet");
        ret = GST_FLOW_ERROR;
        goto done;
      case MARKER_TLM:
        GST_ERROR_OBJECT (self, "TLM marker not supported yet");
        ret = GST_FLOW_ERROR;
        goto done;
      case MARKER_PLM:
        GST_ERROR_OBJECT (self, "PLM marker not supported yet");
        ret = GST_FLOW_ERROR;
        goto done;
      case MARKER_PPM:
        GST_ERROR_OBJECT (self, "PPM marker not supported yet");
        ret = GST_FLOW_ERROR;
        goto done;
      case MARKER_QCD:
        if (header->qcd.data != NULL) {
          GST_ERROR_OBJECT (self, "Multiple QCD markers");
          ret = GST_FLOW_ERROR;
          goto done;
        }
        header->qcd.data = gst_byte_reader_peek_data_unchecked (reader);
        header->qcd.length = length - 2;
        gst_byte_reader_skip_unchecked (reader, length - 2);
        break;
      case MARKER_QCC:{
        Buffer *p = g_new (Buffer, 1);
        p->data = gst_byte_reader_peek_data_unchecked (reader);
        p->length = length - 2;
        header->qcc = g_list_append (header->qcc, p);
        gst_byte_reader_skip_unchecked (reader, length - 2);
        break;
      }
      case MARKER_COM:{
        Buffer *p = g_new (Buffer, 1);
        p->data = gst_byte_reader_peek_data_unchecked (reader);
        p->length = length - 2;
        header->com = g_list_append (header->com, p);
        gst_byte_reader_skip_unchecked (reader, length - 2);
        break;
      }
      case MARKER_CRG:{
        Buffer *p = g_new (Buffer, 1);
        p->data = gst_byte_reader_peek_data_unchecked (reader);
        p->length = length - 2;
        header->crg = g_list_append (header->crg, p);
        gst_byte_reader_skip_unchecked (reader, length - 2);
        break;
      }
      default:
        GST_DEBUG_OBJECT (self, "Skipping unknown marker 0x%04x", marker);
        gst_byte_reader_skip_unchecked (reader, length - 2);
        break;
    }
  }

  if (header->siz.n_components == 0 || header->cod.n_layers == 0) {
    GST_ERROR_OBJECT (self, "No SIZ or COD before SOT");
    return GST_FLOW_ERROR;
  }

  header->n_tiles_x =
      (header->siz.x - header->siz.xto + header->siz.xt - 1) / header->siz.xt;
  header->n_tiles_y =
      (header->siz.y - header->siz.yto + header->siz.yt - 1) / header->siz.yt;
  header->n_tiles = header->n_tiles_x * header->n_tiles_y;

  header->tiles = g_malloc0 (sizeof (Tile) * header->n_tiles);

  /* now at SOT marker, read the tiles */
  {
    gint i;

    for (i = 0; i < header->n_tiles; i++) {
      ret = parse_tile (self, reader, header, &header->tiles[i]);
      if (ret != GST_FLOW_OK)
        goto done;
    }
  }

  /* now there must be the EOC marker */
  if (!gst_byte_reader_get_uint16_be (reader, &marker)
      || marker != MARKER_EOC) {
    GST_ERROR_OBJECT (self, "Frame does not end with EOC");
    ret = GST_FLOW_ERROR;
    goto done;
  }

done:

  return ret;
}

guint
sizeof_main_header (GstJP2kDecimator * self, const MainHeader * header)
{
  guint size = 2;
  GList *l;
  gint i;

  size += sizeof_siz (self, &header->siz);
  size += sizeof_cod (self, &header->cod);
  size += 2 + 2 + header->qcd.length;

  for (l = header->qcc; l; l = l->next) {
    Buffer *b = l->data;
    size += 2 + 2 + b->length;
  }

  for (l = header->crg; l; l = l->next) {
    Buffer *b = l->data;
    size += 2 + 2 + b->length;
  }

  for (l = header->com; l; l = l->next) {
    Buffer *b = l->data;
    size += 2 + 2 + b->length;
  }

  for (i = 0; i < header->n_tiles; i++) {
    size += sizeof_tile (self, &header->tiles[i]);
  }

  /* EOC */
  size += 2;

  return size;
}

void
reset_main_header (GstJP2kDecimator * self, MainHeader * header)
{
  gint i;
  GList *l;

  if (header->tiles) {
    for (i = 0; i < header->n_tiles; i++) {
      reset_tile (self, header, &header->tiles[i]);
    }
    g_free (header->tiles);
  }

  for (l = header->qcc; l; l = l->next)
    g_free (l->data);
  g_list_free (header->qcc);

  for (l = header->com; l; l = l->next)
    g_free (l->data);
  g_list_free (header->com);

  for (l = header->crg; l; l = l->next)
    g_free (l->data);
  g_list_free (header->crg);

  reset_cod (self, &header->cod);
  reset_siz (self, &header->siz);

  memset (header, 0, sizeof (MainHeader));
}

GstFlowReturn
write_main_header (GstJP2kDecimator * self, GstByteWriter * writer,
    const MainHeader * header)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GList *l;
  gint i;

  if (!gst_byte_writer_ensure_free_space (writer, 2)) {
    GST_ERROR_OBJECT (self, "Could not ensure free space");
    return GST_FLOW_ERROR;
  }

  gst_byte_writer_put_uint16_be_unchecked (writer, MARKER_SOC);

  ret = write_siz (self, writer, &header->siz);
  if (ret != GST_FLOW_OK)
    goto done;

  ret = write_cod (self, writer, &header->cod);
  if (ret != GST_FLOW_OK)
    goto done;

  ret = write_marker_buffer (self, writer, MARKER_QCD, &header->qcd);
  if (ret != GST_FLOW_OK)
    goto done;

  for (l = header->qcc; l; l = l->next) {
    Buffer *p = l->data;

    ret = write_marker_buffer (self, writer, MARKER_QCC, p);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  for (l = header->crg; l; l = l->next) {
    Buffer *p = l->data;

    ret = write_marker_buffer (self, writer, MARKER_CRG, p);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  for (l = header->com; l; l = l->next) {
    Buffer *p = l->data;

    ret = write_marker_buffer (self, writer, MARKER_COM, p);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  for (i = 0; i < header->n_tiles; i++) {
    ret = write_tile (self, writer, header, &header->tiles[i]);
    if (ret != GST_FLOW_OK)
      goto done;
  }

  if (!gst_byte_writer_ensure_free_space (writer, 2)) {
    GST_ERROR_OBJECT (self, "Could not ensure free space");
    ret = GST_FLOW_ERROR;
    goto done;
  }
  gst_byte_writer_put_uint16_be_unchecked (writer, MARKER_EOC);

done:
  return ret;
}

GstFlowReturn
decimate_main_header (GstJP2kDecimator * self, MainHeader * header)
{
  GstFlowReturn ret = GST_FLOW_OK;
  gint i;

  for (i = 0; i < header->n_tiles; i++) {
    Tile *tile = &header->tiles[i];
    GList *l;
    PacketIterator it;
    PacketLengthTilePart *plt = NULL;

    if (tile->plt) {
      if (g_list_length (tile->plt) > 1) {
        GST_ERROR_OBJECT (self, "Multiple PLT per tile not supported yet");
        ret = GST_FLOW_ERROR;
        goto done;
      }
      plt = g_new (PacketLengthTilePart, 1);
      plt->index = 0;
      plt->packet_lengths = g_array_new (FALSE, FALSE, sizeof (guint32));
    }

    init_packet_iterator (self, &it, header, tile);

    l = tile->packets;
    while ((it.next (&it))) {
      Packet *p;

      if (l == NULL) {
        GST_ERROR_OBJECT (self, "Not enough packets");
        ret = GST_FLOW_ERROR;
        g_array_free (plt->packet_lengths, TRUE);
        g_free (plt);
        goto done;
      }

      p = l->data;

      if ((self->max_layers != 0 && it.cur_layer >= self->max_layers) ||
          (self->max_decomposition_levels != -1
              && it.cur_resolution > self->max_decomposition_levels)) {
        p->data = NULL;
        p->length = 1;
      }

      if (plt) {
        guint32 len = sizeof_packet (self, p);
        g_array_append_val (plt->packet_lengths, len);
      }

      l = l->next;
    }

    if (plt) {
      reset_plt (self, tile->plt->data);
      g_free (tile->plt->data);
      tile->plt->data = plt;
    }

    tile->sot.tile_part_size = sizeof_tile (self, tile);
  }

done:
  return ret;
}
