// SPDX-FileCopyrightText: 2021-2024, Alejandro Colomar <alx@kernel.org>
// SPDX-FileCopyrightText: 2024, Tobias Stoeckmann <tobias@stoeckmann.org>
// SPDX-License-Identifier: BSD-3-Clause


#include <config.h>

#include "time/day_to_str.h"


extern inline void day_to_str(size_t size, char buf[size], long day);
