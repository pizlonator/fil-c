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


#ifndef _SIREN_COMMON_H
#define _SIREN_COMMON_H

typedef struct {
  unsigned int RiffId;
  unsigned int RiffSize;
} RiffHeader;

typedef struct  {
  unsigned short Format;
  unsigned short Channels;
  unsigned int SampleRate;
  unsigned int ByteRate;
  unsigned short BlockAlign;
  unsigned short BitsPerSample;
} FmtChunk;


typedef struct  {
  FmtChunk fmt;
  unsigned short ExtraSize;
  unsigned short DctLength;
} SirenFmtChunk;

typedef struct {
  RiffHeader riff;
  unsigned int WaveId;

  unsigned int FmtId;
  unsigned int FmtSize;

  SirenFmtChunk fmt;

  unsigned int FactId;
  unsigned int FactSize;
  unsigned int Samples;

  unsigned int DataId;
  unsigned int DataSize;
} SirenWavHeader;

typedef struct {
  RiffHeader riff;
  unsigned int WaveId;

  unsigned int FmtId;
  unsigned int FmtSize;

  FmtChunk fmt;

  unsigned int FactId;
  unsigned int FactSize;
  unsigned int Samples;

  unsigned int DataId;
  unsigned int DataSize;
} PCMWavHeader;

#define RIFF_ID 0x46464952
#define WAVE_ID 0x45564157
#define FMT__ID 0x20746d66
#define DATA_ID 0x61746164
#define FACT_ID 0x74636166


extern int region_size;
extern float region_size_inverse;
extern float standard_deviation[64];
extern float deviation_inverse[64];
extern float region_power_table_boundary[63];
extern int expected_bits_table[8];
extern int vector_dimension[8];
extern int number_of_vectors[8];
extern float dead_zone[8];
extern int max_bin[8];
extern float step_size[8];
extern float step_size_inverse[8];



extern void siren_init(void);
extern int categorize_regions(int number_of_regions, int number_of_available_bits, int *absolute_region_power_index, int *power_categories, int *category_balance);
extern int GetSirenCodecInfo(int flag, int sample_rate, int *number_of_coefs, int *sample_rate_bits, int *rate_control_bits, int *rate_control_possibilities, int *checksum_bits, int *esf_adjustment, int *scale_factor, int *number_of_regions, int *sample_rate_code, int *bits_per_frame );


#ifdef __BIG_ENDIAN__

#define POW_2_8 256
#define POW_2_16 65536
#define POW_2_24 16777216

#define IDX(val, i) ((unsigned int) ((unsigned char *) &val)[i])



#define ME_FROM_LE16(val) ( (unsigned short) ( IDX(val, 0) + IDX(val, 1) * 256 ))
#define ME_FROM_LE32(val) ( (unsigned int) (IDX(val, 0) + IDX(val, 1) * 256 + \
          IDX(val, 2) * 65536 + IDX(val, 3) * 16777216))


#define ME_TO_LE16(val) ( (unsigned short) (                    \
          (((unsigned short)val % 256) & 0xff) << 8 |           \
          ((((unsigned short)val / POW_2_8) % 256) & 0xff) ))

#define ME_TO_LE32(val) ( (unsigned int) (                              \
          ((((unsigned int) val           ) % 256)  & 0xff) << 24 |     \
          ((((unsigned int) val / POW_2_8 ) % 256) & 0xff) << 16|       \
          ((((unsigned int) val / POW_2_16) % 256) & 0xff) << 8 |       \
          ((((unsigned int) val / POW_2_24) % 256) & 0xff) ))

#else

#define ME_TO_LE16(val) ( (unsigned short) (val))
#define ME_TO_LE32(val) ( (unsigned int) (val))
#define ME_FROM_LE16(val) ( (unsigned short) (val))
#define ME_FROM_LE32(val) ( (unsigned int) (val))


#endif



#endif /* _SIREN_COMMON_H */

