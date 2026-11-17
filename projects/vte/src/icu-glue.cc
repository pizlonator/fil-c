/*
 * Copyright © 2019 Christian Persch
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

#include "config.h"

#include <glib.h>

#include <cstring>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <unicode/errorcode.h>
#include <unicode/ucnv.h>

#include "icu-glue.hh"

namespace vte::base {

/* VTE cannot use the converters for ECMA-35 (ISO-2022-*), since they
 * interpret escape sequences (for charset designation), and do not
 * (and *cannot*, without having a complete escape sequence parser)
 * let through the non-designation sequences.
 *
 * The user will need to use luit(1) instead.
 */

bool
get_icu_charset_is_ecma35(char const* charset)
{
        return strstr(charset, "2022") != nullptr;
}

char**
get_icu_charsets(bool aliases)
{
        auto count = ucnv_countAvailable();
        auto names = std::vector<std::string>{};
        names.reserve(count);

        for (auto i = 0; i < count; ++i) {
                auto name = ucnv_getAvailableName(i);
                if (get_icu_charset_is_ecma35(name))
                        continue;

                if (!aliases) {
                        names.push_back(name);
                        continue;
                }

                auto err = icu::ErrorCode{};
                auto acount = ucnv_countAliases(name, err);
                if (err.isFailure()) {
                        names.push_back(name);
                        continue;
                }

                /* The aliases will include @name */

                for (auto j = 0; j < acount; ++j) {
                        err.reset();
                        auto alias = ucnv_getAlias(name, j, err);
                        if (err.isFailure())
                                continue;
                        names.push_back(alias);
                }
        }

        /* Sort the charsets list */
        std::sort(names.begin(), names.end());

        auto r = g_new0(char*, names.size() + 1);
        auto n = r;
        for (auto const& name : names)
                *n++ = g_strdup(name.c_str());
        *n++ = nullptr;

        return r;
}

bool
get_icu_charset_supported(char const* charset)
{
        if (get_icu_charset_is_ecma35(charset))
                return false;

        auto err = icu::ErrorCode{};
        auto count = ucnv_countAliases(charset, err);
        return err.isSuccess() && count != 0;
}

static bool
set_icu_callbacks(UConverter* converter,
                  char const* charset,
                  GError** error)
{
        /* The unicode->target conversion is only used when converting
         * user input (keyboard, clipboard) to be sent to the PTY, and
         * we don't want the ucnv_fromUChars to substitute the SUB character
         * for illegal input, since SUB is U+001A which is Ctrl-Z, which
         * the default UCNV_FROM_U_CALLBACK_SUBSTITUTE callback does.
         * Use UCNV_FROM_U_CALLBACK_STOP to stop conversion when encountering
         * illegal input.
         */
        auto err = icu::ErrorCode{};
        ucnv_setFromUCallBack(converter,
                              UCNV_FROM_U_CALLBACK_STOP,
                              nullptr,
                              nullptr,
                              nullptr,
                              err);
        if (err.isFailure()) {
                g_set_error(error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION,
                            "Failed ucnv_setFromUCallBack for charset \"%s\": %s",
                            charset, err.errorName());
                return {};
        }

        return converter;
}

std::shared_ptr<UConverter>
make_icu_converter(char const* charset,
                   GError** error)
{
        auto err = icu::ErrorCode{};
        auto converter = std::shared_ptr<UConverter>{ucnv_open(charset, err), &ucnv_close};
        if (err.isFailure()) {
                g_set_error(error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION,
                            "Failed to open converter for charset \"%s\": %s",
                            charset, err.errorName());
                return {};
        }

        if (!set_icu_callbacks(converter.get(), charset, error))
                return {};

        return converter;
}


std::shared_ptr<UConverter>
clone_icu_converter(UConverter* other,
                    GError** error)
{
        auto err = icu::ErrorCode{};
        auto const charset = ucnv_getName(other, err);
        if (err.isFailure()) {
                g_set_error(error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION,
                            "Failed to get charset from converter: %s",
                            err.errorName());
        }

        err.reset();
        auto converter = std::shared_ptr<UConverter>{ucnv_clone(other, err), &ucnv_close};
        if (err.isFailure()) {
                g_set_error(error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION,
                            "Failed to clone converter for charset \"%s\": %s",
                            charset, err.errorName());
                return {};
        }

        if (!set_icu_callbacks(converter.get(), charset, error))
                return {};

        return converter;
}

} // namespace vte::base
