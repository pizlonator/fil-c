/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_QUIRK_H_
#define _NINE_QUIRK_H_

#include "util/compiler.h"

bool
_nine_get_quirk( unsigned quirk );

#define QUIRK(q) (_nine_get_quirk(QUIRK_##q))

#define QUIRK_FAKE_CAPS         0x00000001
#define QUIRK_LENIENT_SHADER    0x00000002

#endif /* _NINE_QUIRK_H_ */
