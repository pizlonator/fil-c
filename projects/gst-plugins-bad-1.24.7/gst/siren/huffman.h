/*
 * Siren Encoder/Decoder library
 *
 *   @author: Youness Alaoui <kakaroto@kakaroto.homelinux.net>
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

#ifndef _SIREN7_HUFFMAN_H_
#define _SIREN7_HUFFMAN_H_

#include "decoder.h"

extern int compute_region_powers(int number_of_regions, float *coefs, int *drp_num_bits, int *drp_code_bits, int *absolute_region_power_index, int esf_adjustment);
extern int quantize_mlt(int number_of_regions, int rate_control_possibilities, int number_of_available_bits, float *coefs, int *absolute_region_power_index, int *power_categories, int *category_balance, int *region_mlt_bit_counts, int *region_mlt_bits);
extern int decode_envelope(int number_of_regions, float *decoder_standard_deviation, int *absolute_region_power_index, int esf_adjustment);
extern int decode_vector(SirenDecoder decoder, int number_of_regions, int number_of_available_bits, float *decoder_standard_deviation, int *power_categories, float *coefs, int scale_factor);

extern void set_bitstream(int *stream);
extern int next_bit(void);

#endif /* _SIREN7_HUFFMAN_H_ */
