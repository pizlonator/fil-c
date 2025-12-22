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


#define PI 3.1415926

typedef struct
{
  float cos;
  float msin;
} dct_table_type;

static float dct_core_320[100];
static float dct_core_640[100];
static dct_table_type dct_table_5[5];
static dct_table_type dct_table_10[10];
static dct_table_type dct_table_20[20];
static dct_table_type dct_table_40[40];
static dct_table_type dct_table_80[80];
static dct_table_type dct_table_160[160];
static dct_table_type dct_table_320[320];
static dct_table_type dct_table_640[640];
static dct_table_type *dct_tables[8] = { dct_table_5,
  dct_table_10,
  dct_table_20,
  dct_table_40,
  dct_table_80,
  dct_table_160,
  dct_table_320,
  dct_table_640
};

static int dct4_initialized = 0;

void
siren_dct4_init (void)
{
  int i, j = 0;
  double scale_320 = (float) sqrt (2.0 / 320);
  double scale_640 = (float) sqrt (2.0 / 640);
  double angle;
  double scale;

  /* set up dct4 tables */
  for (i = 0; i < 10; i++) {
    angle = (float) ((i + 0.5) * PI);
    for (j = 0; j < 10; j++) {
      dct_core_320[(i * 10) + j] =
          (float) (scale_320 * cos ((j + 0.5) * angle / 10));
      dct_core_640[(i * 10) + j] =
          (float) (scale_640 * cos ((j + 0.5) * angle / 10));
    }
  }

  for (i = 0; i < 8; i++) {
    scale = (float) (PI / ((5 << i) * 4));
    for (j = 0; j < (5 << i); j++) {
      angle = (float) (j + 0.5) * scale;
      dct_tables[i][j].cos = (float) cos (angle);
      dct_tables[i][j].msin = (float) -sin (angle);
    }
  }

  dct4_initialized = 1;
}


void
siren_dct4 (float *Source, float *Destination, int dct_length)
{
  int log_length = 0;
  float *dct_core = NULL;
  dct_table_type **dct_table_ptr_ptr = NULL;
  dct_table_type *dct_table_ptr = NULL;
  float OutBuffer1[640];
  float OutBuffer2[640];
  float *Out_ptr;
  float *NextOut_ptr;
  float *In_Ptr = NULL;
  float *In_Ptr_low = NULL;
  float *In_Ptr_high = NULL;
  float In_val_low;
  float In_val_high;
  float *Out_ptr_low = NULL;
  float *Out_ptr_high = NULL;
  float mult1, mult2, mult3, mult4, mult5, mult6, mult7, mult8, mult9, mult10;
  int i, j;

  if (dct4_initialized == 0)
    siren_dct4_init ();

  if (dct_length == 640) {
    log_length = 5;
    dct_core = dct_core_640;
  } else {
    log_length = 4;
    dct_core = dct_core_320;
  }

  Out_ptr = OutBuffer1;
  NextOut_ptr = OutBuffer2;
  In_Ptr = Source;
  for (i = 0; i <= log_length; i++) {
    for (j = 0; j < (1 << i); j++) {
      Out_ptr_low = Out_ptr + (j * (dct_length >> i));
      Out_ptr_high = Out_ptr + ((j + 1) * (dct_length >> i));
      do {
        In_val_low = *In_Ptr++;
        In_val_high = *In_Ptr++;
        *Out_ptr_low++ = In_val_low + In_val_high;
        *--Out_ptr_high = In_val_low - In_val_high;
      } while (Out_ptr_low < Out_ptr_high);
    }

    In_Ptr = Out_ptr;
    Out_ptr = NextOut_ptr;
    NextOut_ptr = In_Ptr;
  }

  for (i = 0; i < (2 << log_length); i++) {
    for (j = 0; j < 10; j++) {
      mult1 = In_Ptr[(i * 10)] * dct_core[j * 10];
      mult2 = In_Ptr[(i * 10) + 1] * dct_core[(j * 10) + 1];
      mult3 = In_Ptr[(i * 10) + 2] * dct_core[(j * 10) + 2];
      mult4 = In_Ptr[(i * 10) + 3] * dct_core[(j * 10) + 3];
      mult5 = In_Ptr[(i * 10) + 4] * dct_core[(j * 10) + 4];
      mult6 = In_Ptr[(i * 10) + 5] * dct_core[(j * 10) + 5];
      mult7 = In_Ptr[(i * 10) + 6] * dct_core[(j * 10) + 6];
      mult8 = In_Ptr[(i * 10) + 7] * dct_core[(j * 10) + 7];
      mult9 = In_Ptr[(i * 10) + 8] * dct_core[(j * 10) + 8];
      mult10 = In_Ptr[(i * 10) + 9] * dct_core[(j * 10) + 9];
      Out_ptr[(i * 10) + j] = mult1 + mult2 + mult3 + mult4 +
          mult5 + mult6 + mult7 + mult8 + mult9 + mult10;
    }
  }


  In_Ptr = Out_ptr;
  Out_ptr = NextOut_ptr;
  NextOut_ptr = In_Ptr;
  dct_table_ptr_ptr = dct_tables;
  for (i = log_length; i >= 0; i--) {
    dct_table_ptr_ptr++;
    for (j = 0; j < (1 << i); j++) {
      dct_table_ptr = *dct_table_ptr_ptr;
      if (i == 0)
        Out_ptr_low = Destination + (j * (dct_length >> i));
      else
        Out_ptr_low = Out_ptr + (j * (dct_length >> i));

      Out_ptr_high = Out_ptr_low + (dct_length >> i);

      In_Ptr_low = In_Ptr + (j * (dct_length >> i));
      In_Ptr_high = In_Ptr_low + (dct_length >> (i + 1));
      do {
        *Out_ptr_low++ =
            (*In_Ptr_low * (*dct_table_ptr).cos) -
            (*In_Ptr_high * (*dct_table_ptr).msin);
        *--Out_ptr_high =
            (*In_Ptr_high++ * (*dct_table_ptr).cos) +
            (*In_Ptr_low++ * (*dct_table_ptr).msin);
        dct_table_ptr++;
        *Out_ptr_low++ =
            (*In_Ptr_low * (*dct_table_ptr).cos) +
            (*In_Ptr_high * (*dct_table_ptr).msin);
        *--Out_ptr_high =
            (*In_Ptr_low++ * (*dct_table_ptr).msin) -
            (*In_Ptr_high++ * (*dct_table_ptr).cos);
        dct_table_ptr++;
      } while (Out_ptr_low < Out_ptr_high);
    }

    In_Ptr = Out_ptr;
    Out_ptr = NextOut_ptr;
    NextOut_ptr = In_Ptr;
  }

}
