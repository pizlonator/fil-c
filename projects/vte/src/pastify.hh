/*
 * Copyright © 2015, 2019, Egmont Koblinger
 * Copyright © 2015, 2018, 2019, 2020, 2021 Christian Persch
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

#include <string>
#include <string_view>

namespace vte::terminal {

std::string pastify_string(std::string_view str,
                           bool insert_brackets,
                           bool c1 = false);

void append_control_picture(std::string& str,
                            char32_t ctrl);

} // namespace vte::terminal
