/*
 * Copyright (C) 2009,2010 Red Hat, Inc.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Red Hat Author(s): Behdad Esfahbod
 */

#include <config.h>

#include "debug.h"
#include "vtestream.h"


/*
 * In the future it may be worth replacing these with gio.  Not sure about
 * the overhead though.
 */

#include "vtestream-base.h"
#include "vtestream-file.h"
