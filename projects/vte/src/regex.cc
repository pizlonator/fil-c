/*
 * Copyright © 2015, 2019 Christian Persch
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

#include "regex.hh"
#include "vte/vteenums.h"
#include "vte/vteregex.h"

#include <cassert>

namespace vte {

namespace base {

static bool
set_gerror_from_pcre_error(int errcode,
                           GError **error)
{
        PCRE2_UCHAR8 buf[256];
        int n = pcre2_get_error_message_8(errcode, buf, sizeof(buf));
        assert(n >= 0);
        g_set_error_literal(error, VTE_REGEX_ERROR, errcode, (char const*)buf);
        return false;
}

Regex*
Regex::ref() noexcept
{
        g_atomic_int_inc(&m_refcount);
        return this;
}

void
Regex::unref() noexcept
{
        if (g_atomic_int_dec_and_test(&m_refcount))
                delete this;
}

bool
Regex::check_pcre_config_unicode(GError** error)
{
        /* Check library compatibility */
        guint32 v;
        int r = pcre2_config_8(PCRE2_CONFIG_UNICODE, &v);
        if (r != 0 || v != 1) {
                g_set_error(error, VTE_REGEX_ERROR, VTE_REGEX_ERROR_INCOMPATIBLE,
                            "PCRE2 library was built without unicode support");
                return false;
        }

        return true;
}

bool
Regex::check_pcre_config_jit(void)
{
        static bool warned = false;

        char s[256];
        int r = pcre2_config_8(PCRE2_CONFIG_JITTARGET, &s);
        if (r == PCRE2_ERROR_BADOPTION && !warned) {
                g_printerr("PCRE2 library was built without JIT support\n");
                warned = true;
        }

        return r >= 1;
}

Regex*
Regex::compile(Regex::Purpose purpose,
               std::string_view const& pattern,
               uint32_t flags,
               uint32_t extra_flags,
               size_t* error_offset,
               GError** error)
{
        assert(error == nullptr || *error == nullptr);

        if (!check_pcre_config_unicode(error))
                return nullptr;

        auto context = vte::Freeable<pcre2_compile_context_8>{};
        if (extra_flags) {
                context = vte::take_freeable(pcre2_compile_context_create_8(nullptr));
                pcre2_set_compile_extra_options_8(context.get(), extra_flags);
        }

        int errcode;
        PCRE2_SIZE erroffset;
        auto code = vte::take_freeable(pcre2_compile_8((PCRE2_SPTR8)pattern.data(),
                                                       pattern.size(),
                                                       (uint32_t)flags |
                                                       PCRE2_UTF |
                                                       (flags & PCRE2_UTF ? PCRE2_NO_UTF_CHECK : 0) |
                                                       PCRE2_NEVER_BACKSLASH_C |
                                                       PCRE2_USE_OFFSET_LIMIT,
                                                       &errcode, &erroffset,
                                                       context.get()));

        if (!code) {
                set_gerror_from_pcre_error(errcode, error);
                if (error_offset)
                        *error_offset = erroffset;

                g_prefix_error(error, "Failed to compile pattern to regex at offset %" G_GSIZE_FORMAT ":",
                               erroffset);
                return nullptr;
        }

        return new Regex{std::move(code), purpose};
}

/*
 * Regex::jit:
 * @flags: JIT flags
 *
 * If the platform supports JITing, JIT compiles the regex.
 *
 * Returns: %true if JITing succeeded (or PCRE2 was built without
 *   JIT support), or %false with @error filled in
 */
bool
Regex::jit(uint32_t flags,
           GError** error) noexcept
{
        if (!check_pcre_config_jit())
                return TRUE;

        int r = pcre2_jit_compile_8(code(), flags);
        if (r < 0)
                return set_gerror_from_pcre_error(r, error);

        return true;
}

/*
 * Regex::jited:
 *
 * Note: We can't tell if the regex has been JITed for a particular mode,
 * just if it has been JITed at all.
 *
 * Returns: %true iff the regex has been JITed
 */
bool
Regex::jited() const noexcept
{
        PCRE2_SIZE s;
        int r = pcre2_pattern_info_8(code(), PCRE2_INFO_JITSIZE, &s);

        return r == 0 && s != 0;
}

/*
 * Regex::has_compile_flags:
 * @flags:
 *
 * Returns: true if the compile flags include all of @flags.
 */
bool
Regex::has_compile_flags(uint32_t flags) const noexcept
{
        uint32_t v;
        int r = pcre2_pattern_info_8(code(), PCRE2_INFO_ARGOPTIONS, &v);

        return r == 0 ? ((v & flags) == flags) : false;
}

/*
 * Regex::substitute:
 * @subject: the subject string
 * @replacement: the replacement string
 * @flags: PCRE2 match flags
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * See man:pcre2api(3) on pcre2_substitute() for more information.
 *
 * Returns: the substituted string, or std::nullopt if an error occurred
 */
std::optional<std::string>
Regex::substitute(std::string_view const& subject,
                  std::string_view const& replacement,
                  uint32_t flags,
                  GError** error) const
{
        assert (!(flags & PCRE2_SUBSTITUTE_OVERFLOW_LENGTH));

        char outbuf[2048];
        PCRE2_SIZE outlen = sizeof(outbuf) - 1;
        auto r = pcre2_substitute_8(code(),
                                    (PCRE2_SPTR8)subject.data(), subject.size(),
                                    0 /* start offset */,
                                    flags | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH,
                                    nullptr /* match data */,
                                    nullptr /* match context */,
                                    (PCRE2_SPTR8)replacement.data(), replacement.size(),
                                    (PCRE2_UCHAR8*)outbuf, &outlen);
        /* Note that on success, outlen excludes the trailing NUL. */
        if (r >= 0)
                return std::string{outbuf, outlen};

        if (r == PCRE2_ERROR_NOMEMORY) {
                /* The buffer was not large enough; allocated a buffer of the
                 * required size and try again. Note that as opposed to the successful
                 * call to pcre2_substitute_8() above, in the error case outlen *includes*
                 * the trailing NUL.
                 */
                std::string outbuf2;
                outbuf2.resize(outlen);

                r = pcre2_substitute_8(code(),
                                       (PCRE2_SPTR8)subject.data(), subject.size(),
                                       0 /* start offset */,
                                       flags | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH,
                                       nullptr /* match data */,
                                       nullptr /* match context */,
                                       (PCRE2_SPTR8)replacement.data(), replacement.size(),
                                       (PCRE2_UCHAR8*)outbuf2.data(), &outlen);
                if (r >= 0) {
                        /* Note that on success, outlen excludes the trailing NUL. */
                        outbuf2.resize(outlen);
                        return outbuf2;
                }
       }

        set_gerror_from_pcre_error(r, error);
        return std::nullopt;
}

} // namespace base
} // namespace vte
