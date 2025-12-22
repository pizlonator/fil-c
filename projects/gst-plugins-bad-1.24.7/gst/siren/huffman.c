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


#include "siren7.h"
#include "huffman_consts.h"


static short current_word = 0;
static int bit_idx = 0;
static int *bitstream_ptr = NULL;

int
next_bit (void)
{
  if (bitstream_ptr == NULL)
    return -1;

  if (bit_idx == 0) {
    current_word = *bitstream_ptr++;
    bit_idx = 16;
  }

  return (current_word >> --bit_idx) & 1;
}

void
set_bitstream (int *stream)
{
  bitstream_ptr = stream;
  current_word = *bitstream_ptr;
  bit_idx = 0;
}


int
compute_region_powers (int number_of_regions, float *coefs, int *drp_num_bits,
    int *drp_code_bits, int *absolute_region_power_index, int esf_adjustment)
{
  float region_power = 0;
  int num_bits;
  int idx;
  int max_idx, min_idx;
  int region, i;

  for (region = 0; region < number_of_regions; region++) {
    region_power = 0.0f;
    for (i = 0; i < region_size; i++) {
      region_power +=
          coefs[(region * region_size) + i] * coefs[(region * region_size) + i];
    }
    region_power *= region_size_inverse;

    min_idx = 0;
    max_idx = 64;
    for (i = 0; i < 6; i++) {
      idx = (min_idx + max_idx) / 2;
      if (region_power_table_boundary[idx - 1] <= region_power) {
        min_idx = idx;
      } else {
        max_idx = idx;
      }
    }
    absolute_region_power_index[region] = min_idx - 24;

  }

  for (region = number_of_regions - 2; region >= 0; region--) {
    if (absolute_region_power_index[region] <
        absolute_region_power_index[region + 1] - 11)
      absolute_region_power_index[region] =
          absolute_region_power_index[region + 1] - 11;
  }

  if (absolute_region_power_index[0] < (1 - esf_adjustment))
    absolute_region_power_index[0] = (1 - esf_adjustment);

  if (absolute_region_power_index[0] > (31 - esf_adjustment))
    absolute_region_power_index[0] = (31 - esf_adjustment);

  drp_num_bits[0] = 5;
  drp_code_bits[0] = absolute_region_power_index[0] + esf_adjustment;


  for (region = 1; region < number_of_regions; region++) {
    if (absolute_region_power_index[region] < (-8 - esf_adjustment))
      absolute_region_power_index[region] = (-8 - esf_adjustment);
    if (absolute_region_power_index[region] > (31 - esf_adjustment))
      absolute_region_power_index[region] = (31 - esf_adjustment);
  }

  num_bits = 5;

  for (region = 0; region < number_of_regions - 1; region++) {
    idx =
        absolute_region_power_index[region + 1] -
        absolute_region_power_index[region] + 12;
    if (idx < 0)
      idx = 0;

    absolute_region_power_index[region + 1] =
        absolute_region_power_index[region] + idx - 12;
    drp_num_bits[region + 1] = differential_region_power_bits[region][idx];
    drp_code_bits[region + 1] = differential_region_power_codes[region][idx];
    num_bits += drp_num_bits[region + 1];
  }

  return num_bits;
}


int
decode_envelope (int number_of_regions, float *decoder_standard_deviation,
    int *absolute_region_power_index, int esf_adjustment)
{
  int index;
  int i;
  int envelope_bits = 0;

  index = 0;
  for (i = 0; i < 5; i++)
    index = (index << 1) | next_bit ();
  envelope_bits = 5;

  absolute_region_power_index[0] = index - esf_adjustment;
  decoder_standard_deviation[0] =
      standard_deviation[absolute_region_power_index[0] + 24];

  for (i = 1; i < number_of_regions; i++) {
    index = 0;
    do {
      index = differential_decoder_tree[i - 1][index][next_bit ()];
      envelope_bits++;
    } while (index > 0);

    absolute_region_power_index[i] =
        absolute_region_power_index[i - 1] - index - 12;
    if (absolute_region_power_index[i] < -24)
      absolute_region_power_index[i] = -24;
    else if (absolute_region_power_index[i] > 39)
      absolute_region_power_index[i] = 39;
    decoder_standard_deviation[i] =
        standard_deviation[absolute_region_power_index[i] + 24];
  }

  return envelope_bits;
}



static int
huffman_vector (int category, int power_idx, float *mlts, int *out)
{
  int i, j;
  float temp_value = deviation_inverse[power_idx] * step_size_inverse[category];
  int sign_idx, idx, non_zeroes, max, bits_available;
  int current_word = 0;
  int region_bits = 0;

  bits_available = 32;
  for (i = 0; i < number_of_vectors[category]; i++) {
    sign_idx = idx = non_zeroes = 0;
    for (j = 0; j < vector_dimension[category]; j++) {
      max = (int) ((fabs (*mlts) * temp_value) + dead_zone[category]);
      if (max != 0) {
        sign_idx <<= 1;
        non_zeroes++;
        if (*mlts > 0)
          sign_idx++;
        if (max > max_bin[category] || max < 0)
          max = max_bin[category];

      }
      mlts++;
      idx = (idx * (max_bin[category] + 1)) + max;
    }

    region_bits += bitcount_tables[category][idx] + non_zeroes;
    bits_available -= bitcount_tables[category][idx] + non_zeroes;
    if (bits_available < 0) {
      *out++ =
          current_word + (((code_tables[category][idx] << non_zeroes) +
              sign_idx) >> -bits_available);
      bits_available += 32;
      current_word =
          ((code_tables[category][idx] << non_zeroes) +
          sign_idx) << bits_available;
    } else {
      current_word +=
          ((code_tables[category][idx] << non_zeroes) +
          sign_idx) << bits_available;
    }

  }

  *out = current_word;
  return region_bits;
}

int
quantize_mlt (int number_of_regions, int rate_control_possibilities,
    int number_of_available_bits, float *coefs,
    int *absolute_region_power_index, int *power_categories,
    int *category_balance, int *region_mlt_bit_counts, int *region_mlt_bits)
{
  int region;
  int mlt_bits = 0;
  int rate_control;

  for (rate_control = 0; rate_control < ((rate_control_possibilities >> 1) - 1);
      rate_control++)
    power_categories[category_balance[rate_control]]++;

  for (region = 0; region < number_of_regions; region++) {
    if (power_categories[region] > 6)
      region_mlt_bit_counts[region] = 0;
    else
      region_mlt_bit_counts[region] =
          huffman_vector (power_categories[region],
          absolute_region_power_index[region], coefs + (region_size * region),
          region_mlt_bits + (4 * region));
    mlt_bits += region_mlt_bit_counts[region];
  }

  while (mlt_bits < number_of_available_bits && rate_control > 0) {
    rate_control--;
    region = category_balance[rate_control];
    power_categories[region]--;

    if (power_categories[region] < 0)
      power_categories[region] = 0;

    mlt_bits -= region_mlt_bit_counts[region];

    if (power_categories[region] > 6)
      region_mlt_bit_counts[region] = 0;
    else
      region_mlt_bit_counts[region] =
          huffman_vector (power_categories[region],
          absolute_region_power_index[region], coefs + (region_size * region),
          region_mlt_bits + (4 * region));

    mlt_bits += region_mlt_bit_counts[region];
  }

  while (mlt_bits > number_of_available_bits
      && rate_control < rate_control_possibilities) {
    region = category_balance[rate_control];
    power_categories[region]++;
    mlt_bits -= region_mlt_bit_counts[region];

    if (power_categories[region] > 6)
      region_mlt_bit_counts[region] = 0;
    else
      region_mlt_bit_counts[region] =
          huffman_vector (power_categories[region],
          absolute_region_power_index[region], coefs + (region_size * region),
          region_mlt_bits + (4 * region));

    mlt_bits += region_mlt_bit_counts[region];

    rate_control++;
  }

  return rate_control;
}

static int
get_dw (SirenDecoder decoder)
{
  int ret = decoder->dw1 + decoder->dw4;

  if ((ret & 0x8000) != 0)
    ret++;

  decoder->dw1 = decoder->dw2;
  decoder->dw2 = decoder->dw3;
  decoder->dw3 = decoder->dw4;
  decoder->dw4 = ret;

  return ret;
}




int
decode_vector (SirenDecoder decoder, int number_of_regions,
    int number_of_available_bits, float *decoder_standard_deviation,
    int *power_categories, float *coefs, int scale_factor)
{
  float *coefs_ptr;
  float decoded_value;
  float noise;
  int *decoder_tree;

  int region;
  int category;
  int i, j;
  int index;
  int error;
  int dw1;
  int dw2;

  error = 0;
  for (region = 0; region < number_of_regions; region++) {
    category = power_categories[region];
    coefs_ptr = coefs + (region * region_size);

    if (category < 7) {
      decoder_tree = decoder_tables[category];

      for (i = 0; i < number_of_vectors[category]; i++) {
        index = 0;
        do {
          if (number_of_available_bits <= 0) {
            error = 1;
            break;
          }

          index = decoder_tree[index + next_bit ()];
          number_of_available_bits--;
        } while ((index & 1) == 0);

        index >>= 1;

        if (error == 0 && number_of_available_bits >= 0) {
          for (j = 0; j < vector_dimension[category]; j++) {
            decoded_value =
                mlt_quant[category][index & ((1 << index_table[category]) - 1)];
            index >>= index_table[category];

            if (decoded_value != 0) {
              if (next_bit () == 0)
                decoded_value *= -decoder_standard_deviation[region];
              else
                decoded_value *= decoder_standard_deviation[region];
              number_of_available_bits--;
            }

            *coefs_ptr++ = decoded_value * scale_factor;
          }
        } else {
          error = 1;
          break;
        }
      }

      if (error == 1) {
        for (j = region + 1; j < number_of_regions; j++)
          power_categories[j] = 7;
        category = 7;
      }
    }


    coefs_ptr = coefs + (region * region_size);

    if (category == 5) {
      i = 0;
      for (j = 0; j < region_size; j++) {
        if (*coefs_ptr != 0) {
          i++;
          if (fabs (*coefs_ptr) > 2.0 * decoder_standard_deviation[region]) {
            i += 3;
          }
        }
        coefs_ptr++;
      }

      noise = decoder_standard_deviation[region] * noise_category5[i];
    } else if (category == 6) {
      i = 0;
      for (j = 0; j < region_size; j++) {
        if (*coefs_ptr++ != 0)
          i++;
      }

      noise = decoder_standard_deviation[region] * noise_category6[i];
    } else if (category == 7) {
      noise = decoder_standard_deviation[region] * noise_category7;
    } else {
      noise = 0;
    }

    coefs_ptr = coefs + (region * region_size);

    if (category == 5 || category == 6 || category == 7) {
      dw1 = get_dw (decoder);
      dw2 = get_dw (decoder);

      for (j = 0; j < 10; j++) {
        if (category == 7 || *coefs_ptr == 0) {
          if ((dw1 & 1))
            *coefs_ptr = noise;
          else
            *coefs_ptr = -noise;
        }
        coefs_ptr++;
        dw1 >>= 1;

        if (category == 7 || *coefs_ptr == 0) {
          if ((dw2 & 1))
            *coefs_ptr = noise;
          else
            *coefs_ptr = -noise;
        }
        coefs_ptr++;
        dw2 >>= 1;
      }
    }
  }

  return error == 1 ? -1 : number_of_available_bits;
}
