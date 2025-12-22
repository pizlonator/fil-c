/* GStreamer RIST plugin
 * Copyright (C) 2019 Net Insight AB
 *     Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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

#include "gstrist.h"
#include "gstroundrobin.h"

/*
 * rtp_ext_seq:
 * @extseq: (inout): a previous extended seqs
 * @seq: a new seq
 *
 * Update the @extseq field with the extended seq of @seq
 * For the first call of the method, @extseq should point to a location
 * with a value of -1.
 *
 * This function is able to handle both forward and backward seqs taking
 * into account:
 *   - seq wraparound making sure that the returned value is properly increased.
 *   - seq unwraparound making sure that the returned value is properly decreased.
 *
 * Returns: The extended seq of @seq or 0 if the result can't go anywhere backwards.
 *
 * NOTE: This is a calque of gst_rtp_buffer_ext_timestamp() but with
 * s/32/16/ and s/64/32/ and s/0xffffffff/0xffff/ and s/timestamp/seqnum/.
 */
guint32
gst_rist_rtp_ext_seq (guint32 * extseqnum, guint16 seqnum)
{
  guint32 result, ext;

  g_return_val_if_fail (extseqnum != NULL, -1);

  ext = *extseqnum;

  if (ext == -1) {
    result = seqnum;
  } else {
    /* pick wraparound counter from previous seqnum and add to new seqnum */
    result = seqnum + (ext & ~(0xffff));

    /* check for seqnum wraparound */
    if (result < ext) {
      guint32 diff = ext - result;

      if (diff > G_MAXINT16) {
        /* seqnum went backwards more than allowed, we wrap around and get
         * updated extended seqnum. */
        result += (1 << 16);
      }
    } else {
      guint32 diff = result - ext;

      if (diff > G_MAXINT16) {
        if (result < (1 << 16)) {
          GST_WARNING
              ("Cannot unwrap, any wrapping took place yet. Returning 0 without updating extended seqnum.");
          return 0;
        } else {
          /* seqnum went forwards more than allowed, we unwrap around and get
           * updated extended seqnum. */
          result -= (1 << 16);
          /* We don't want the extended seqnum storage to go back, ever */
          return result;
        }
      }
    }
  }

  *extseqnum = result;

  return result;
}
