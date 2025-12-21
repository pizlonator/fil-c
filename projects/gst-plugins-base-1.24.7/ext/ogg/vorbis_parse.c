/*
   This file borrowed from liboggz
 */
/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * oggz_auto.c
 *
 * Conrad Parker <conrad@annodex.net>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "gstoggstream.h"
#include "vorbis_parse.h"

/*
 * Vorbis packets can be short or long, and each packet overlaps the previous
 * and next packets.  The granulepos of a packet is always the last sample
 * that is completely decoded at the end of decoding that packet - i.e. the
 * last packet before the first overlapping packet.  If the sizes of packets
 * are 's' and 'l', then the increment will depend on the previous and next
 * packet types:
 *  v                             prev<<1 | next
 * lll:           l/2             3
 * lls:           3l/4 - s/4      2
 * lsl:           s/2
 * lss:           s/2
 * sll:           l/4 + s/4       1
 * sls:           l/2             0
 * ssl:           s/2
 * sss:           s/2
 *
 * The previous and next packet types can be inferred from the current packet
 * (additional information is not required)
 *
 * The two blocksizes can be determined from the first header packet, by reading
 * byte 28.  1 << (packet[28] >> 4) == long_size.
 * 1 << (packet[28] & 0xF) == short_size.
 *
 * (see http://xiph.org/vorbis/doc/Vorbis_I_spec.html for specification)
 */


void
gst_parse_vorbis_header_packet (GstOggStream * pad, ogg_packet * packet)
{
  /*
   * on the first (b_o_s) packet, determine the long and short sizes,
   * and then calculate l/2, l/4 - s/4, 3 * l/4 - s/4, l/2 - s/2 and s/2
   */
  int short_size;
  int long_size;

  long_size = 1 << (packet->packet[28] >> 4);
  short_size = 1 << (packet->packet[28] & 0xF);

  pad->nln_increments[3] = long_size >> 1;
  pad->nln_increments[2] = 3 * (long_size >> 2) - (short_size >> 2);
  pad->nln_increments[1] = (long_size >> 2) + (short_size >> 2);
  pad->nln_increments[0] = pad->nln_increments[3];
  pad->short_size = short_size;
  pad->long_size = long_size;
  pad->nsn_increment = short_size >> 1;
}

int
gst_parse_vorbis_setup_packet (GstOggStream * pad, ogg_packet * op)
{
  /*
   * the code pages, a whole bunch of other fairly useless stuff, AND,
   * RIGHT AT THE END (of a bunch of variable-length compressed rubbish that
   * basically has only one actual set of values that everyone uses BUT YOU
   * CAN'T BE SURE OF THAT, OH NO YOU CAN'T) is the only piece of data that's
   * actually useful to us - the packet modes (because it's inconceivable to
   * think people might want _just that_ and nothing else, you know, for
   * seeking and stuff).
   *
   * Fortunately, because of the mandate that non-used bits must be zero
   * at the end of the packet, we might be able to sneakily work backwards
   * and find out the information we need (namely a mapping of modes to
   * packet sizes)
   */
  unsigned char *current_pos = &op->packet[op->bytes - 1];
  int offset;
  int size;
  int size_check;
  int *mode_size_ptr;
  int i;
  int ii;

  /*
   * This is the format of the mode data at the end of the packet for all
   * Vorbis Version 1 :
   *
   * [ 6:number_of_modes ]
   * [ 1:size | 16:window_type(0) | 16:transform_type(0) | 8:mapping ]
   * [ 1:size | 16:window_type(0) | 16:transform_type(0) | 8:mapping ]
   * [ 1:size | 16:window_type(0) | 16:transform_type(0) | 8:mapping ]
   * [ 1:framing(1) ]
   *
   * e.g.:
   *
   *              <-
   * 0 0 0 0 0 1 0 0
   * 0 0 1 0 0 0 0 0
   * 0 0 1 0 0 0 0 0
   * 0 0 1|0 0 0 0 0
   * 0 0 0 0|0|0 0 0
   * 0 0 0 0 0 0 0 0
   * 0 0 0 0|0 0 0 0
   * 0 0 0 0 0 0 0 0
   * 0 0 0 0|0 0 0 0
   * 0 0 0|1|0 0 0 0 |
   * 0 0 0 0 0 0 0 0 V
   * 0 0 0|0 0 0 0 0
   * 0 0 0 0 0 0 0 0
   * 0 0 1|0 0 0 0 0
   * 0 0|1|0 0 0 0 0
   *
   *
   * i.e. each entry is an important bit, 32 bits of 0, 8 bits of blah, a
   * bit of 1.
   * Let's find our last 1 bit first.
   *
   */

  size = 0;

  offset = 8;
  while (!((1 << --offset) & *current_pos)) {
    if (offset == 0) {
      offset = 8;
      current_pos -= 1;
    }
  }

  while (1) {

    /*
     * from current_pos-5:(offset+1) to current_pos-1:(offset+1) should
     * be zero
     */
    offset = (offset + 7) % 8;
    if (offset == 7)
      current_pos -= 1;

    if (((current_pos[-5] & ~((1 << (offset + 1)) - 1)) != 0)
        ||
        current_pos[-4] != 0
        ||
        current_pos[-3] != 0
        ||
        current_pos[-2] != 0
        || ((current_pos[-1] & ((1 << (offset + 1)) - 1)) != 0)
        ) {
      break;
    }

    size += 1;

    current_pos -= 5;

  }

  /* Give ourselves a chance to recover if we went back too far by using
   * the size check. */
  for (ii = 0; ii < 2; ii++) {
    if (offset > 4) {
      size_check = (current_pos[0] >> (offset - 5)) & 0x3F;
    } else {
      /* mask part of byte from current_pos */
      size_check = (current_pos[0] & ((1 << (offset + 1)) - 1));
      /* shift to appropriate position */
      size_check <<= (5 - offset);
      /* or in part of byte from current_pos - 1 */
      size_check |= (current_pos[-1] & ~((1 << (offset + 3)) - 1)) >>
          (offset + 3);
    }

    size_check += 1;
    if (size_check == size) {
      break;
    }
    offset = (offset + 1) % 8;
    if (offset == 0)
      current_pos += 1;
    current_pos += 5;
    size -= 1;

    /* have we overrun? */
    if (current_pos >= op->packet + op->bytes)
      return -1;
  }

  /* Store mode size information in our info struct */
  i = -1;
  while ((1 << (++i)) < size);
  pad->vorbis_log2_num_modes = i;

  mode_size_ptr = pad->vorbis_mode_sizes;

  for (i = 0; i < size; i++) {
    offset = (offset + 1) % 8;
    if (offset == 0)
      current_pos += 1;
    *mode_size_ptr++ = (current_pos[0] >> offset) & 0x1;
    current_pos += 5;

    /* have we overrun? */
    if (current_pos >= op->packet + op->bytes)
      return -1;
  }

  return 0;
}
