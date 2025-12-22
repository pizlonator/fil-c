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


#ifndef _SIREN_ENCODER_H
#define _SIREN_ENCODER_H

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "dct4.h"
#include "rmlt.h"
#include "huffman.h"
#include "common.h"


typedef struct stSirenEncoder {
	int sample_rate;
	SirenWavHeader WavHeader;
	float context[320];
} * SirenEncoder;

/* sample_rate MUST be 16000 to be compatible with MSN Voice clips (I think) */
extern SirenEncoder Siren7_NewEncoder(int sample_rate);
extern void Siren7_CloseEncoder(SirenEncoder encoder);
extern int Siren7_EncodeFrame(SirenEncoder encoder, unsigned char *DataIn, unsigned char *DataOut);


#endif /* _SIREN_ENCODER_H */
