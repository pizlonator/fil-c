/*
 * Copyright © 2023 Christian Persch
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
 */

#pragma once

#include "fwd.hh"

#include <vte/vteuuid.h>

VteUuid* _vte_uuid_new_from_uuid(vte::uuid const& u) noexcept;

VteUuid* _vte_uuid_wrap(vte::uuid& u) noexcept;

vte::uuid& _vte_uuid_unwrap(VteUuid* u) noexcept;

vte::uuid const& _vte_uuid_unwrap(VteUuid const* u) noexcept;
