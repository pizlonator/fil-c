/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include "win32ipcutils.h"
#include <cctype>
#include <string>
#include <locale>
#include <codecvt>
#include <algorithm>

static inline void rtrim(std::string &s) {
  s.erase (std::find_if (s.rbegin(), s.rend(),
      [](unsigned char ch) {
        return !std::isspace (ch);
      }).base (), s.end ());
}

std::string
win32_ipc_error_message (DWORD error_code)
{
  WCHAR buffer[1024];

  if (!FormatMessageW (FORMAT_MESSAGE_IGNORE_INSERTS |
      FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error_code, 0, buffer,
      1024, nullptr)) {
    return std::string ("");
  }

  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
  std::string ret = converter.to_bytes (buffer);
  rtrim (ret);

  return ret;
}
