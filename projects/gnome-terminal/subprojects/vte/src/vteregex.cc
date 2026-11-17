/*
 * Copyright © 2015 Christian Persch
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

/**
 * SECTION: vte-regex
 * @short_description: Regex for matching and searching. Uses PCRE2 internally.
 *
 * Since: 0.46
 */

#include "config.h"

#include <exception>

#include "vtemacros.h"
#include "vteenums.h"
#include "vteregex.h"

#include "glib-glue.hh"
#include "pcre2-glue.hh"
#include "regex.hh"
#include "vteregexinternal.hh"

#define IMPL(wrapper) (regex_from_wrapper(wrapper))

/* Type registration */

#pragma GCC diagnostic push
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
G_DEFINE_BOXED_TYPE(VteRegex, vte_regex,
                    vte_regex_ref, (GBoxedFreeFunc)vte_regex_unref)
#pragma GCC diagnostic pop

G_DEFINE_QUARK(vte-regex-error, vte_regex_error)

/**
 * vte_regex_ref:
 * @regex: (transfer none): a #VteRegex
 *
 * Increases the reference count of @regex by one.
 *
 * Returns: @regex
 */
VteRegex *
vte_regex_ref(VteRegex *regex) noexcept
{
        g_return_val_if_fail(regex != nullptr, nullptr);

        return wrapper_from_regex(IMPL(regex)->ref());
}

/**
 * vte_regex_unref:
 * @regex: (transfer full): a #VteRegex
 *
 * Decreases the reference count of @regex by one, and frees @regex
 * if the refcount reaches zero.
 *
 * Returns: %NULL
 */
VteRegex *
vte_regex_unref(VteRegex* regex) noexcept
{
        g_return_val_if_fail(regex != nullptr, nullptr);

        IMPL(regex)->unref();
        return nullptr;
}

static VteRegex*
vte_regex_new(vte::base::Regex::Purpose purpose,
              std::string_view const& pattern,
              uint32_t flags,
              uint32_t extra_flags,
              gsize* error_offset,
              GError** error) noexcept
try
{
        return wrapper_from_regex(vte::base::Regex::compile(purpose,
                                                            pattern,
                                                            flags,
                                                            extra_flags,
                                                            error_offset,
                                                            error));
}
catch (...)
{
        vte::glib::set_error_from_exception(error);
        return nullptr;
}

/**
 * vte_regex_new_for_match_full:
 * @pattern: a regex pattern string
 * @pattern_length: the length of @pattern in bytes, or -1 if the
 *  string is NUL-terminated and the length is unknown
 * @flags: PCRE2 compile flags
 * @extra_flags: PCRE2 extra compile flags
 * @error_offset: (out) (optional): return location to store the error offset
 * @error: (nullable): return location for a #GError
 *
 * Compiles @pattern into a regex for use as a match regex
 * with vte_terminal_match_add_regex() or
 * vte_terminal_event_check_regex_simple().
 *
 * See man:pcre2pattern(3) for information
 * about the supported regex language, and man:pcre2api(3) for
 * information about the supported @flags and @extra_flags.
 *
 * The regex will be compiled using <literal>PCRE2_UTF</literal> and
 * possibly other flags, in addition to the flags supplied in @flags.
 *
 * If regex compilation fails, @error will be set and @error_offset point
 * to error as an offset into @pattern.
 *
 * Returns: (transfer full): a newly created #VteRegex, or %NULL
 *
 * Since: 0.76
 */
VteRegex *
vte_regex_new_for_match_full(char const* pattern,
                             gssize pattern_length,
                             uint32_t flags,
                             uint32_t extra_flags,
                             gsize* error_offset,
                             GError** error) noexcept
try
{
        auto const len = size_t{pattern_length == -1 ? strlen(pattern) : size_t(pattern_length)};
        return vte_regex_new(vte::base::Regex::Purpose::eMatch,
                             {pattern, len},
                             flags,
                             extra_flags,
                             error_offset,
                             error);
}
catch (...)
{
        vte::glib::set_error_from_exception(error);
        return nullptr;
}

/**
 * vte_regex_new_for_match:
 * @pattern: a regex pattern string
 * @pattern_length: the length of @pattern in bytes, or -1 if the
 *  string is NUL-terminated and the length is unknown
 * @flags: PCRE2 compile flags
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Compiles @pattern into a regex for use as a match regex
 * with vte_terminal_match_add_regex() or
 * vte_terminal_event_check_regex_simple().
 *
 * See man:pcre2pattern(3) for information
 * about the supported regex language, and man:pcre2api(3) for
 * information about the supported @flags.
 *
 * The regex will be compiled using <literal>PCRE2_UTF</literal> and
 * possibly other flags, in addition to the flags supplied in @flags.
 *
 * Returns: (transfer full): a newly created #VteRegex, or %NULL with @error filled in
 */
VteRegex *
vte_regex_new_for_match(const char *pattern,
                        gssize      pattern_length,
                        guint32     flags,
                        GError    **error) noexcept
{
        return vte_regex_new_for_match_full(pattern, pattern_length, flags, 0, nullptr, error);
}

/**
 * vte_regex_new_for_search_full:
 * @pattern: a regex pattern string
 * @pattern_length: the length of @pattern in bytes, or -1 if the
 *  string is NUL-terminated and the length is unknown
 * @flags: PCRE2 compile flags
 * @error_offset: (out) (optional): return location to store the error offset
 * @error: (nullable): return location for a #GError
 *
 * Compiles @pattern into a regex for use as a search regex
 * with vte_terminal_search_set_regex().
 *
 * See man:pcre2pattern(3) for information
 * about the supported regex language, and man:pcre2api(3) for
 * information about the supported @flags and @extra_flags.
 *
 * The regex will be compiled using <literal>PCRE2_UTF</literal> and
 * possibly other flags, in addition to the flags supplied in @flags.
 *
 * If regex compilation fails, @error will be set and @error_offset point
 * to error as an offset into @pattern.
 *
 * Returns: (transfer full): a newly created #VteRegex, or %NULL
 *
 * Since: 0.76
 */
VteRegex*
vte_regex_new_for_search_full(char const* pattern,
                              gssize pattern_length,
                              uint32_t flags,
                              uint32_t extra_flags,
                              gsize* error_offset,
                              GError** error) noexcept
try
{
        auto const len = size_t{pattern_length == -1 ? strlen(pattern) : size_t(pattern_length)};
        return vte_regex_new(vte::base::Regex::Purpose::eSearch,
                             {pattern, len},
                             flags,
                             extra_flags,
                             error_offset,
                             error);
}
catch (...)
{
        vte::glib::set_error_from_exception(error);
        return nullptr;
}

/**
 * vte_regex_new_for_search:
 * @pattern: a regex pattern string
 * @pattern_length: the length of @pattern in bytes, or -1 if the
 *  string is NUL-terminated and the length is unknown
 * @flags: PCRE2 compile flags
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Compiles @pattern into a regex for use as a search regex
 * with vte_terminal_search_set_regex().
 *
 * See man:pcre2pattern(3) for information
 * about the supported regex language, and man:pcre2api(3) for
 * information about the supported @flags.
 *
 * The regex will be compiled using <literal>PCRE2_UTF</literal> and
 * possibly other flags, in addition to the flags supplied in @flags.
 *
 * Returns: (transfer full): a newly created #VteRegex, or %NULL with @error filled in
 */
VteRegex *
vte_regex_new_for_search(const char *pattern,
                         gssize      pattern_length,
                         guint32     flags,
                         GError    **error) noexcept
{
        return vte_regex_new_for_search_full(pattern, pattern_length, flags, 0, nullptr, error);
}

/**
 * vte_regex_jit:
 * @regex: a #VteRegex
 * @flags: PCRE2 JIT flags, or 0
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * If the platform supports JITing, JIT compiles @regex.
 *
 * Returns: %TRUE if JITing succeeded (or PCRE2 was built without
 *   JIT support), or %FALSE with @error filled in
 */
gboolean
vte_regex_jit(VteRegex *regex,
              guint     flags,
              GError  **error) noexcept
try
{
        g_return_val_if_fail(regex != nullptr, false);

        return IMPL(regex)->jit(flags, error);
}
catch (...)
{
        return vte::glib::set_error_from_exception(error);
}

bool
_vte_regex_has_purpose(VteRegex *regex,
                       vte::base::Regex::Purpose purpose)
{
        g_return_val_if_fail(regex != nullptr, false);

        return IMPL(regex)->has_purpose(purpose);
}

bool
_vte_regex_has_multiline_compile_flag(VteRegex *regex)
{
        g_return_val_if_fail(regex != nullptr, 0);

        return IMPL(regex)->has_compile_flags(PCRE2_MULTILINE);
}

/**
 * vte_regex_substitute:
 * @regex: a #VteRegex
 * @subject: the subject string
 * @replacement: the replacement string
 * @flags: PCRE2 match flags
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * See man:pcre2api(3) and man:pcre2_substitute(3) for more information.
 *
 * Returns: (transfer full): the substituted string, or %NULL
 *   if an error occurred
 *
 * Since: 0.56
 */
char *
vte_regex_substitute(VteRegex *regex,
                     const char *subject,
                     const char *replacement,
                     guint32 flags,
                     GError **error) noexcept
try
{
        g_return_val_if_fail(regex != nullptr, nullptr);
        g_return_val_if_fail(subject != nullptr, nullptr);
        g_return_val_if_fail(replacement != nullptr, nullptr);
        g_return_val_if_fail (!(flags & PCRE2_SUBSTITUTE_OVERFLOW_LENGTH), nullptr);

        auto const r = IMPL(regex)->substitute(subject, replacement, flags, error);
        return r ? g_strndup(r->c_str(), r->size()) : nullptr;
}
catch (...)
{
        vte::glib::set_error_from_exception(error);
        return nullptr;
}
