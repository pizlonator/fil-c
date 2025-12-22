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


SirenEncoder
Siren7_NewEncoder (int sample_rate)
{
  SirenEncoder encoder = (SirenEncoder) malloc (sizeof (struct stSirenEncoder));
  encoder->sample_rate = sample_rate;

  encoder->WavHeader.riff.RiffId = ME_TO_LE32 (RIFF_ID);
  encoder->WavHeader.riff.RiffSize = sizeof (SirenWavHeader) - 2 * sizeof (int);
  encoder->WavHeader.riff.RiffSize =
      ME_TO_LE32 (encoder->WavHeader.riff.RiffSize);
  encoder->WavHeader.WaveId = ME_TO_LE32 (WAVE_ID);

  encoder->WavHeader.FmtId = ME_TO_LE32 (FMT__ID);
  encoder->WavHeader.FmtSize = ME_TO_LE32 (sizeof (SirenFmtChunk));

  encoder->WavHeader.fmt.fmt.Format = ME_TO_LE16 (0x028E);
  encoder->WavHeader.fmt.fmt.Channels = ME_TO_LE16 (1);
  encoder->WavHeader.fmt.fmt.SampleRate = ME_TO_LE32 (16000);
  encoder->WavHeader.fmt.fmt.ByteRate = ME_TO_LE32 (2000);
  encoder->WavHeader.fmt.fmt.BlockAlign = ME_TO_LE16 (40);
  encoder->WavHeader.fmt.fmt.BitsPerSample = ME_TO_LE16 (0);
  encoder->WavHeader.fmt.ExtraSize = ME_TO_LE16 (2);
  encoder->WavHeader.fmt.DctLength = ME_TO_LE16 (320);

  encoder->WavHeader.FactId = ME_TO_LE32 (FACT_ID);
  encoder->WavHeader.FactSize = ME_TO_LE32 (sizeof (int));
  encoder->WavHeader.Samples = ME_TO_LE32 (0);

  encoder->WavHeader.DataId = ME_TO_LE32 (DATA_ID);
  encoder->WavHeader.DataSize = ME_TO_LE32 (0);

  memset (encoder->context, 0, sizeof (encoder->context));

  siren_init ();
  return encoder;
}

void
Siren7_CloseEncoder (SirenEncoder encoder)
{
  free (encoder);
}



int
Siren7_EncodeFrame (SirenEncoder encoder, unsigned char *DataIn,
    unsigned char *DataOut)
{
  int number_of_coefs,
      sample_rate_bits,
      rate_control_bits,
      rate_control_possibilities,
      checksum_bits,
      esf_adjustment,
      scale_factor, number_of_regions, sample_rate_code, bits_per_frame;
  int sample_rate = encoder->sample_rate;

  int absolute_region_power_index[28] = { 0 };
  int power_categories[28] = { 0 };
  int category_balance[28] = { 0 };
  int drp_num_bits[30] = { 0 };
  int drp_code_bits[30] = { 0 };
  int region_mlt_bit_counts[28] = { 0 };
  int region_mlt_bits[112] = { 0 };
  int ChecksumTable[4] = { 0x7F80, 0x7878, 0x6666, 0x5555 };
  int i, j;

  int dwRes = 0;
  short out_word;
  int bits_left;
  int current_word_bits_left;
  int region_bit_count;
  unsigned int current_word;
  unsigned int sum;
  unsigned int checksum;
  int temp1 = 0;
  int temp2 = 0;
  int region;
  int idx = 0;
  int envelope_bits = 0;
  int rate_control;
  int number_of_available_bits;

  float coefs[320];
  float In[320];
  short BufferOut[20];
  float *context = encoder->context;

  for (i = 0; i < 320; i++)
    In[i] = (float) ((short) ME_FROM_LE16 (((short *) DataIn)[i]));

  dwRes = siren_rmlt_encode_samples (In, context, 320, coefs);


  if (dwRes != 0)
    return dwRes;

  dwRes =
      GetSirenCodecInfo (1, sample_rate, &number_of_coefs, &sample_rate_bits,
      &rate_control_bits, &rate_control_possibilities, &checksum_bits,
      &esf_adjustment, &scale_factor, &number_of_regions, &sample_rate_code,
      &bits_per_frame);

  if (dwRes != 0)
    return dwRes;

  envelope_bits =
      compute_region_powers (number_of_regions, coefs, drp_num_bits,
      drp_code_bits, absolute_region_power_index, esf_adjustment);

  number_of_available_bits =
      bits_per_frame - rate_control_bits - envelope_bits - sample_rate_bits -
      checksum_bits;

  categorize_regions (number_of_regions, number_of_available_bits,
      absolute_region_power_index, power_categories, category_balance);

  for (region = 0; region < number_of_regions; region++) {
    absolute_region_power_index[region] += 24;
    region_mlt_bit_counts[region] = 0;
  }

  rate_control =
      quantize_mlt (number_of_regions, rate_control_possibilities,
      number_of_available_bits, coefs, absolute_region_power_index,
      power_categories, category_balance, region_mlt_bit_counts,
      region_mlt_bits);

  idx = 0;
  bits_left = 16 - sample_rate_bits;
  out_word = sample_rate_code << (16 - sample_rate_bits);
  drp_num_bits[number_of_regions] = rate_control_bits;
  drp_code_bits[number_of_regions] = rate_control;
  for (region = 0; region <= number_of_regions; region++) {
    i = drp_num_bits[region] - bits_left;
    if (i < 0) {
      out_word += drp_code_bits[region] << -i;
      bits_left -= drp_num_bits[region];
    } else {
      BufferOut[idx++] = out_word + (drp_code_bits[region] >> i);
      bits_left += 16 - drp_num_bits[region];
      out_word = drp_code_bits[region] << bits_left;
    }
  }

  for (region = 0; region < number_of_regions && (16 * idx) < bits_per_frame;
      region++) {
    current_word_bits_left = region_bit_count = region_mlt_bit_counts[region];
    if (current_word_bits_left > 32)
      current_word_bits_left = 32;

    current_word = region_mlt_bits[region * 4];
    i = 1;
    while (region_bit_count > 0 && (16 * idx) < bits_per_frame) {
      if (current_word_bits_left < bits_left) {
        bits_left -= current_word_bits_left;
        out_word +=
            (current_word >> (32 - current_word_bits_left)) << bits_left;
        current_word_bits_left = 0;
      } else {
        BufferOut[idx++] =
            (short) (out_word + (current_word >> (32 - bits_left)));
        current_word_bits_left -= bits_left;
        current_word <<= bits_left;
        bits_left = 16;
        out_word = 0;
      }
      if (current_word_bits_left == 0) {
        region_bit_count -= 32;
        current_word = region_mlt_bits[(region * 4) + i++];
        current_word_bits_left = region_bit_count;
        if (current_word_bits_left > 32)
          current_word_bits_left = 32;
      }
    }
  }


  while ((16 * idx) < bits_per_frame) {
    BufferOut[idx++] = (short) ((0xFFFF >> (16 - bits_left)) + out_word);
    bits_left = 16;
    out_word = 0;
  }

  if (checksum_bits > 0) {
    BufferOut[idx - 1] &= (-1 << checksum_bits);
    sum = 0;
    idx = 0;
    do {
      sum ^= (BufferOut[idx] & 0xFFFF) << (idx % 15);
    } while ((16 * ++idx) < bits_per_frame);

    sum = (sum >> 15) ^ (sum & 0x7FFF);
    checksum = 0;
    for (i = 0; i < 4; i++) {
      temp1 = ChecksumTable[i] & sum;
      for (j = 8; j > 0; j >>= 1) {
        temp2 = temp1 >> j;
        temp1 ^= temp2;
      }
      checksum <<= 1;
      checksum |= temp1 & 1;
    }
    BufferOut[idx - 1] |= ((1 << checksum_bits) - 1) & checksum;
  }


  for (i = 0; i < 20; i++)
#ifdef __BIG_ENDIAN__
    ((short *) DataOut)[i] = BufferOut[i];
#else
    ((short *) DataOut)[i] =
        ((BufferOut[i] << 8) & 0xFF00) | ((BufferOut[i] >> 8) & 0x00FF);
#endif

  encoder->WavHeader.Samples = ME_FROM_LE32 (encoder->WavHeader.Samples);
  encoder->WavHeader.Samples += 320;
  encoder->WavHeader.Samples = ME_TO_LE32 (encoder->WavHeader.Samples);
  encoder->WavHeader.DataSize = ME_FROM_LE32 (encoder->WavHeader.DataSize);
  encoder->WavHeader.DataSize += 40;
  encoder->WavHeader.DataSize = ME_TO_LE32 (encoder->WavHeader.DataSize);
  encoder->WavHeader.riff.RiffSize =
      ME_FROM_LE32 (encoder->WavHeader.riff.RiffSize);
  encoder->WavHeader.riff.RiffSize += 40;
  encoder->WavHeader.riff.RiffSize =
      ME_TO_LE32 (encoder->WavHeader.riff.RiffSize);


  return 0;
}
