/* vi: set sw=4 ts=4: */
/*
 * bb_perror_nomsg implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

void FAST_FUNC bb_perror_nomsg(void)
{
	bb_simple_perror_msg("");
}
