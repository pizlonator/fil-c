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

int region_size;
float region_size_inverse;

float standard_deviation[64];
float deviation_inverse[64];
float region_power_table_boundary[63];

int expected_bits_table[8] = { 52, 47, 43, 37, 29, 22, 16, 0 };
int vector_dimension[8] = { 2, 2, 2, 4, 4, 5, 5, 1 };
int number_of_vectors[8] = { 10, 10, 10, 5, 5, 4, 4, 20 };
float dead_zone[8] = { 0.3f, 0.33f, 0.36f, 0.39f, 0.42f, 0.45f, 0.5f, 0.5f };

int max_bin[8] = {
  13,
  9,
  6,
  4,
  3,
  2,
  1,
  1
};

float step_size[8] = {
  0.3536f,
  0.5f,
  0.70709997f,
  1.0f,
  1.4141999f,
  2.0f,
  2.8283999f,
  2.8283999f
};

float step_size_inverse[8];

static int siren_initialized = 0;

/*
  STEPSIZE = 2.0 * log(sqrt(2));
*/
#define STEPSIZE 0.3010299957f

void
siren_init (void)
{
  int i;
  float region_power;

  if (siren_initialized == 1)
    return;

  region_size = 20;
  region_size_inverse = 1.0f / region_size;

  for (i = 0; i < 64; i++) {
    region_power = powf (10.0f, (i - 24) * STEPSIZE);
    standard_deviation[i] = sqrtf (region_power);
    deviation_inverse[i] = 1.0f / standard_deviation[i];
  }

  for (i = 0; i < 63; i++)
    region_power_table_boundary[i] =
        (float) pow (10, (i - 24 + 0.5) * STEPSIZE);

  for (i = 0; i < 8; i++)
    step_size_inverse[i] = (float) 1.0 / step_size[i];

  siren_dct4_init ();
  siren_rmlt_init ();

  siren_initialized = 1;
}


int
categorize_regions (int number_of_regions, int number_of_available_bits,
    int *absolute_region_power_index, int *power_categories,
    int *category_balance)
{
  int region, delta, i, temp;
  int expected_number_of_code_bits;
  int min, max;
  int offset,
      num_rate_control_possibilities,
      raw_value, raw_max_idx = 0, raw_min_idx = 0;
  int max_rate_categories[28];
  int min_rate_categories[28];
  int temp_category_balances[64];
  int *min_rate_ptr = NULL;
  int *max_rate_ptr = NULL;

  if (number_of_regions == 14) {
    num_rate_control_possibilities = 16;
    if (number_of_available_bits > 320)
      number_of_available_bits =
          ((number_of_available_bits - 320) * 5 / 8) + 320;
  } else {
    num_rate_control_possibilities = 32;
    if (number_of_regions == 28 && number_of_available_bits > 640)
      number_of_available_bits =
          ((number_of_available_bits - 640) * 5 / 8) + 640;
  }

  offset = -32;
  for (delta = 32; number_of_regions > 0 && delta > 0; delta /= 2) {
    expected_number_of_code_bits = 0;
    for (region = 0; region < number_of_regions; region++) {
      i = (delta + offset - absolute_region_power_index[region]) >> 1;
      if (i > 7)
        i = 7;
      else if (i < 0)
        i = 0;

      power_categories[region] = i;
      expected_number_of_code_bits += expected_bits_table[i];

    }
    if (expected_number_of_code_bits >= number_of_available_bits - 32)
      offset += delta;
  }

  expected_number_of_code_bits = 0;
  for (region = 0; region < number_of_regions; region++) {
    i = (offset - absolute_region_power_index[region]) >> 1;
    if (i > 7)
      i = 7;
    else if (i < 0)
      i = 0;
    max_rate_categories[region] = min_rate_categories[region] =
        power_categories[region] = i;
    expected_number_of_code_bits += expected_bits_table[i];
  }


  min = max = expected_number_of_code_bits;
  min_rate_ptr = max_rate_ptr =
      temp_category_balances + num_rate_control_possibilities;
  for (i = 0; i < num_rate_control_possibilities - 1; i++) {
    if (min + max > number_of_available_bits * 2) {
      raw_value = -99;
      for (region = number_of_regions - 1; region >= 0; region--) {
        if (min_rate_categories[region] < 7) {
          temp =
              offset - absolute_region_power_index[region] -
              2 * min_rate_categories[region];
          if (temp > raw_value) {
            raw_value = temp;
            raw_min_idx = region;
          }
        }
      }
      *min_rate_ptr++ = raw_min_idx;
      min +=
          expected_bits_table[min_rate_categories[raw_min_idx] + 1] -
          expected_bits_table[min_rate_categories[raw_min_idx]];
      min_rate_categories[raw_min_idx]++;
    } else {
      raw_value = 99;
      for (region = 0; region < number_of_regions; region++) {
        if (max_rate_categories[region] > 0) {
          temp =
              offset - absolute_region_power_index[region] -
              2 * max_rate_categories[region];
          if (temp < raw_value) {
            raw_value = temp;
            raw_max_idx = region;
          }
        }
      }

      *--max_rate_ptr = raw_max_idx;
      max +=
          expected_bits_table[max_rate_categories[raw_max_idx] - 1] -
          expected_bits_table[max_rate_categories[raw_max_idx]];
      max_rate_categories[raw_max_idx]--;
    }
  }

  for (region = 0; region < number_of_regions; region++)
    power_categories[region] = max_rate_categories[region];

  for (i = 0; i < num_rate_control_possibilities - 1; i++)
    category_balance[i] = *max_rate_ptr++;


  return 0;
}



/*
  Looks like the flag means what kind of encoding is used
  for now, it looks like :
  0 : the sample rate is not encoded in the frame
  1 - 2 : the sample rate is fixed in the frame
  3 : sample rate is variable and there is one for each frame
*/

int
GetSirenCodecInfo (int flag, int sample_rate, int *number_of_coefs,
    int *sample_rate_bits, int *rate_control_bits,
    int *rate_control_possibilities, int *checksum_bits, int *esf_adjustment,
    int *scale_factor, int *number_of_regions, int *sample_rate_code,
    int *bits_per_frame)
{
  switch (flag) {
    case 0:
      *number_of_coefs = 320;
      *sample_rate_bits = 0;
      *rate_control_bits = 4;
      *rate_control_possibilities = 16;
      *checksum_bits = 0;
      *esf_adjustment = 7;
      *number_of_regions = 14;
      *sample_rate_code = 0;
      *scale_factor = 22;
      break;
    case 1:
      *number_of_coefs = 320;
      *sample_rate_bits = 2;
      *rate_control_bits = 4;
      *rate_control_possibilities = 16;
      *checksum_bits = 4;
      *esf_adjustment = -2;
      *number_of_regions = 14;
      *scale_factor = 1;
      if (sample_rate == 16000)
        *sample_rate_code = 1;
      else if (sample_rate == 24000)
        *sample_rate_code = 2;
      else if (sample_rate == 32000)
        *sample_rate_code = 3;
      else
        return 3;
      break;
    case 2:
      *number_of_coefs = 640;
      *sample_rate_bits = 2;
      *rate_control_bits = 5;
      *rate_control_possibilities = 32;
      *checksum_bits = 4;
      *esf_adjustment = 7;
      *number_of_regions = 28;
      *scale_factor = 33;

      if (sample_rate == 24000)
        *sample_rate_code = 1;
      else if (sample_rate == 32000)
        *sample_rate_code = 2;
      else if (sample_rate == 48000)
        *sample_rate_code = 3;
      else
        return 3;

      break;
    case 3:
      *number_of_coefs = 640;
      *sample_rate_bits = 6;
      *rate_control_bits = 5;
      *rate_control_possibilities = 32;
      *checksum_bits = 4;
      *esf_adjustment = 7;
      *scale_factor = 33;

      switch (sample_rate) {
        case 8800:
          *number_of_regions = 12;
          *sample_rate_code = 59;
          break;
        case 9600:
          *number_of_regions = 12;
          *sample_rate_code = 1;
          break;
        case 10400:
          *number_of_regions = 12;
          *sample_rate_code = 13;
          break;
        case 10800:
          *number_of_regions = 12;
          *sample_rate_code = 14;
          break;
        case 11200:
          *number_of_regions = 12;
          *sample_rate_code = 15;
          break;
        case 11600:
          *number_of_regions = 12;
          *sample_rate_code = 16;
          break;
        case 12000:
          *number_of_regions = 12;
          *sample_rate_code = 2;
          break;
        case 12400:
          *number_of_regions = 12;
          *sample_rate_code = 17;
          break;
        case 12800:
          *number_of_regions = 12;
          *sample_rate_code = 18;
          break;
        case 13200:
          *number_of_regions = 12;
          *sample_rate_code = 19;
          break;
        case 13600:
          *number_of_regions = 12;
          *sample_rate_code = 20;
          break;
        case 14000:
          *number_of_regions = 12;
          *sample_rate_code = 21;
          break;
        case 14400:
          *number_of_regions = 16;
          *sample_rate_code = 3;
          break;
        case 14800:
          *number_of_regions = 16;
          *sample_rate_code = 22;
          break;
        case 15200:
          *number_of_regions = 16;
          *sample_rate_code = 23;
          break;
        case 15600:
          *number_of_regions = 16;
          *sample_rate_code = 24;
          break;
        case 16000:
          *number_of_regions = 16;
          *sample_rate_code = 25;
          break;
        case 16400:
          *number_of_regions = 16;
          *sample_rate_code = 26;
          break;
        case 16800:
          *number_of_regions = 18;
          *sample_rate_code = 4;
          break;
        case 17200:
          *number_of_regions = 18;
          *sample_rate_code = 27;
          break;
        case 17600:
          *number_of_regions = 18;
          *sample_rate_code = 28;
          break;
        case 18000:
          *number_of_regions = 18;
          *sample_rate_code = 29;
          break;
        case 18400:
          *number_of_regions = 18;
          *sample_rate_code = 30;
          break;
        case 18800:
          *number_of_regions = 18;
          *sample_rate_code = 31;
          break;
        case 19200:
          *number_of_regions = 20;
          *sample_rate_code = 5;
          break;
        case 19600:
          *number_of_regions = 20;
          *sample_rate_code = 32;
          break;
        case 20000:
          *number_of_regions = 20;
          *sample_rate_code = 33;
          break;
        case 20400:
          *number_of_regions = 20;
          *sample_rate_code = 34;
          break;
        case 20800:
          *number_of_regions = 20;
          *sample_rate_code = 35;
          break;
        case 21200:
          *number_of_regions = 20;
          *sample_rate_code = 36;
          break;
        case 21600:
          *number_of_regions = 22;
          *sample_rate_code = 6;
          break;
        case 22000:
          *number_of_regions = 22;
          *sample_rate_code = 37;
          break;
        case 22400:
          *number_of_regions = 22;
          *sample_rate_code = 38;
          break;
        case 22800:
          *number_of_regions = 22;
          *sample_rate_code = 39;
          break;
        case 23200:
          *number_of_regions = 22;
          *sample_rate_code = 40;
          break;
        case 23600:
          *number_of_regions = 22;
          *sample_rate_code = 41;
          break;
        case 24000:
          *number_of_regions = 24;
          *sample_rate_code = 7;
          break;
        case 24400:
          *number_of_regions = 24;
          *sample_rate_code = 42;
          break;
        case 24800:
          *number_of_regions = 24;
          *sample_rate_code = 43;
          break;
        case 25200:
          *number_of_regions = 24;
          *sample_rate_code = 44;
          break;
        case 25600:
          *number_of_regions = 24;
          *sample_rate_code = 45;
          break;
        case 26000:
          *number_of_regions = 24;
          *sample_rate_code = 46;
          break;
        case 26400:
          *number_of_regions = 26;
          *sample_rate_code = 8;
          break;
        case 26800:
          *number_of_regions = 26;
          *sample_rate_code = 47;
          break;
        case 27200:
          *number_of_regions = 26;
          *sample_rate_code = 48;
          break;
        case 27600:
          *number_of_regions = 26;
          *sample_rate_code = 49;
          break;
        case 28000:
          *number_of_regions = 26;
          *sample_rate_code = 50;
          break;
        case 28400:
          *number_of_regions = 26;
          *sample_rate_code = 51;
          break;
        case 28800:
          *number_of_regions = 28;
          *sample_rate_code = 9;
          break;
        case 29200:
          *number_of_regions = 28;
          *sample_rate_code = 52;
          break;
        case 29600:
          *number_of_regions = 28;
          *sample_rate_code = 53;
          break;
        case 30000:
          *number_of_regions = 28;
          *sample_rate_code = 54;
          break;
        case 30400:
          *number_of_regions = 28;
          *sample_rate_code = 55;
          break;
        case 30800:
          *number_of_regions = 28;
          *sample_rate_code = 56;
          break;
        case 31200:
          *number_of_regions = 28;
          *sample_rate_code = 10;
          break;
        case 31600:
          *number_of_regions = 28;
          *sample_rate_code = 57;
          break;
        case 32000:
          *number_of_regions = 28;
          *sample_rate_code = 58;
          break;
        default:
          return 3;
          break;
      }
      break;
    default:
      return 6;
  }

  *bits_per_frame = sample_rate / 50;
  return 0;
}
