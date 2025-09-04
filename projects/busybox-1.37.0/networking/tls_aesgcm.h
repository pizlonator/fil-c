/*
 * Copyright (C) 2018 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

void aesgcm_GHASH(uint8_t* h,
	const uint8_t* a, //unsigned aSz,
	const uint8_t* c, unsigned cSz,
	uint8_t* s //, unsigned sSz
) FAST_FUNC;
