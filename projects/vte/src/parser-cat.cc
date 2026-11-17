/*
 * Copyright © 2017, 2018 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>

#include <bitset>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include <string>

#include "debug.h"
#include "glib-glue.hh"
#include "libc-glue.hh"
#include "parser.hh"
#include "parser-glue.hh"
#include "std-glue.hh"
#include "utf8.hh"
#include "vtedefines.hh"

enum {
#define _VTE_SGR(...)
#define _VTE_NGR(name, value) VTE_SGR_##name = value,
#include "parser-sgr.hh"
#undef _VTE_SGR
#undef _VTE_NGR
};

using namespace std::literals;

enum class DataSyntax {
        ECMA48_UTF8,
        /* ECMA48_PCTERM, */
        /* ECMA48_ECMA35, */
};

char*
vte::parser::Sequence::ucs4_to_utf8(gunichar const* str,
                                    ssize_t len) const noexcept
{
        return g_ucs4_to_utf8(str, len, nullptr, nullptr, nullptr);
}

static constexpr char const*
seq_to_str(unsigned int type) noexcept
{
        switch (type) {
        case VTE_SEQ_NONE: return "NONE";
        case VTE_SEQ_IGNORE: return "IGNORE";
        case VTE_SEQ_GRAPHIC: return "GRAPHIC";
        case VTE_SEQ_CONTROL: return "CONTROL";
        case VTE_SEQ_ESCAPE: return "ESCAPE";
        case VTE_SEQ_CSI: return "CSI";
        case VTE_SEQ_DCS: return "DCS";
        case VTE_SEQ_OSC: return "OSC";
        case VTE_SEQ_SCI: return "SCI";
        case VTE_SEQ_APC: return "APC";
        case VTE_SEQ_PM: return "PM";
        case VTE_SEQ_SOS: return "SOS";
        default:
                assert(false);
        }
}

static constexpr char const*
cmd_to_str(unsigned int command) noexcept
{
        switch (command) {
#define _VTE_CMD(cmd) case VTE_CMD_##cmd: return #cmd;
#define _VTE_NOP(cmd) _VTE_CMD(cmd)
#include "parser-cmd.hh"
#undef _VTE_CMD
#undef _VTE_NOP
        default:
                return nullptr;
        }
}

#if 0
static constexepr char const*
charset_alias_to_str(unsigned int cs) noexcept
{
        switch (cs) {
#define _VTE_CHARSET_PASTE(name)
#define _VTE_CHARSET(name) _VTE_CHARSET_PASTE(name)
#define _VTE_CHARSET_ALIAS_PASTE(name1,name2) case VTE_CHARSET_##name1: return #name1 "(" ## #name2 ## ")";
#define _VTE_CHARSET_ALIAS(name1,name2)
#include "parser-charset.hh"
#undef _VTE_CHARSET_PASTE
#undef _VTE_CHARSET
#undef _VTE_CHARSET_ALIAS_PASTE
#undef _VTE_CHARSET_ALIAS
        default:
                return nullptr; /* not an alias */
        }
}

static constexpr char const*
charset_to_str(unsigned int cs) noexcept
{
        auto alias = charset_alias_to_str(cs);
        if (alias)
                return alias;

        switch (cs) {
#define _VTE_CHARSET_PASTE(name) case VTE_CHARSET_##name: return #name;
#define _VTE_CHARSET(name) _VTE_CHARSET_PASTE(name)
#define _VTE_CHARSET_ALIAS_PASTE(name1,name2)
#define _VTE_CHARSET_ALIAS(name1,name2)
#include "parser-charset.hh"
#undef _VTE_CHARSET_PASTE
#undef _VTE_CHARSET
#undef _VTE_CHARSET_ALIAS_PASTE
#undef _VTE_CHARSET_ALIAS
        default:
                static char buf[32];
                snprintf(buf, sizeof(buf), "UNKOWN(%u)", cs);
                return buf;
        }
}
#endif

class PrettyPrinter {
private:
        std::string m_str;
        bool m_plain;
        bool m_codepoints;

        inline constexpr bool plain() const noexcept { return m_plain; }

        class Attribute {
        public:
                Attribute(PrettyPrinter* printer,
                          std::string const& intro,
                          std::string const& outro) noexcept
                        : m_printer{printer}
                        , m_outro{outro} {
                        if (!m_printer->plain())
                                m_printer->m_str.append(intro);
                        }

                ~Attribute() noexcept
                {
                        if (!m_printer->plain())
                                m_printer->m_str.append(m_outro);
                }

        private:
                PrettyPrinter* m_printer;
                std::string m_outro;
        }; // class Attribute

        class ReverseAttr : private Attribute {
        public:
                ReverseAttr(PrettyPrinter* printer)
                        : Attribute(printer, "\e[7m"s, "\e[27m"s)
                { }
        };

        class RedAttr : private Attribute {
        public:
                RedAttr(PrettyPrinter* printer)
                        : Attribute(printer, "\e[7;31m"s, "\e[27;39m"s)
                { }
        };

        class GreenAttr : private Attribute {
        public:
                GreenAttr(PrettyPrinter* printer)
                        : Attribute(printer, "\e[7;32m"s, "\e[27;39m"s)
                { }
        };

        void
        print_params(vte::parser::Sequence const& seq) noexcept
        {
                auto const size = seq.size();
                if (size > 0)
                        m_str.push_back(' ');

                for (unsigned int i = 0; i < size; i++) {
                        if (!seq.param_default(i))
                                print_format("%d", seq.param(i));
                        if (i + 1 < size)
                                m_str.push_back(seq.param_nonfinal(i) ? ':' : ';');
                }
        }

        void
        print_pintro(vte::parser::Sequence const& seq) noexcept
        {
                auto const type = seq.type();
                if (type != VTE_SEQ_CSI &&
                    type != VTE_SEQ_DCS)
                        return;

                auto const p = seq.intermediates() & 0x7;
                if (p == 0)
                        return;

                m_str.push_back(' ');
                m_str.push_back(char(0x40 - p));
        }

        void
        print_intermediates(vte::parser::Sequence const& seq) noexcept
        {
                auto const type = seq.type();
                auto intermediates = seq.intermediates();
                if (type == VTE_SEQ_CSI ||
                    type == VTE_SEQ_DCS)
                        intermediates = intermediates >> 3; /* remove pintro */

                while (intermediates != 0) {
                        unsigned int i = intermediates & 0x1f;
                        char c = 0x20 + i - 1;

                        m_str.push_back(' ');
                        if (c == 0x20)
                                m_str.append("SP"s);
                        else
                                m_str.push_back(c);

                        intermediates = intermediates >> 5;
                }
        }

        void
        print_unichar(uint32_t c) noexcept
        {
                char buf[7];
                auto len = g_unichar_to_utf8(c, buf);
                m_str.append(buf, len);
        }

        void
        print_literal(char const* str) noexcept
        {
                m_str.append(str);
        }

        G_GNUC_PRINTF(2, 3)
        void
        print_format(char const* format,
                     ...)
        {
                char buf[256];
                va_list args;
                va_start(args, format);
                auto len = g_vsnprintf(buf, sizeof(buf), format, args);
                va_end(args);

                m_str.append(buf, len);
        }

        void
        print_string(vte::parser::Sequence const& seq) noexcept
        {
                auto u8str = seq.string_param();

                m_str.push_back('\"');
                m_str.append(u8str);
                m_str.push_back('\"');

                g_free(u8str);
        }

        void
        print_seq_and_params(vte::parser::Sequence const& seq) noexcept
        {
                ReverseAttr attr(this);

                if (seq.command() != VTE_CMD_NONE) {
                        m_str.push_back('{');
                        m_str.append(cmd_to_str(seq.command()));
                        print_params(seq);
                        m_str.push_back('}');
                } else {
                        m_str.push_back('{');
                        m_str.append(seq_to_str(seq.type()));
                        print_pintro(seq);
                        print_params(seq);
                        print_intermediates(seq);
                        m_str.push_back(' ');
                        m_str.push_back(seq.terminator());
                        m_str.push_back('}');
                }
        }

        void
        print_seq(vte::parser::Sequence const& seq) noexcept
        {
                switch (seq.type()) {
                case VTE_SEQ_NONE: {
                        RedAttr attr(this);
                        m_str.append("{NONE}"s);
                        break;
                }

                case VTE_SEQ_IGNORE: {
                        RedAttr attr(this);
                        m_str.append("{IGNORE}"s);
                        break;
                }

                case VTE_SEQ_GRAPHIC: {
                        auto const terminator = seq.terminator();
                        bool const printable = g_unichar_isprint(terminator);
                        if (m_codepoints || !printable) {
                                if (printable) {
                                        char ubuf[7];
                                        ubuf[g_unichar_to_utf8(terminator, ubuf)] = 0;
                                        print_format("[%04X %s]", terminator, ubuf);
                                } else {
                                        print_format("[%04X]", terminator);
                                }
                        } else {
                                print_unichar(terminator);
                        }
                        break;
                }

                case VTE_SEQ_CONTROL:
                case VTE_SEQ_ESCAPE: {
                        ReverseAttr attr(this);
                        print_format("{%s}", cmd_to_str(seq.command()));
                        break;
                }

                case VTE_SEQ_CSI:
                case VTE_SEQ_DCS: {
                        print_seq_and_params(seq);
                        break;
                }

                case VTE_SEQ_OSC: {
                        ReverseAttr attr(this);
                        m_str.append("{OSC "s);
                        print_string(seq);
                        m_str.push_back('}');
                        break;
                }

                case VTE_SEQ_SCI: {
                        auto const terminator = seq.terminator();
                        if (terminator <= 0x20)
                                print_format("{SCI %d/%d}",
                                             terminator / 16,
                                             terminator % 16);
                        else
                                print_format("{SCI %c}", terminator);
                        break;
                }

                default:
                        assert(false);
                }
        }

        void
        printout() noexcept
        {
                m_str.push_back('\n');
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
                write(STDOUT_FILENO, m_str.data(), m_str.size());
#pragma GCC diagnostic pop
                m_str.clear();
        }

public:

        PrettyPrinter(bool plain,
                      bool codepoints) noexcept
                : m_plain{plain}
                , m_codepoints{codepoints}
        {
        }

        ~PrettyPrinter() noexcept
        {
                printout();
        }

        void VT(vte::parser::Sequence const& seq) noexcept
        {
                print_seq(seq);
                if (seq.command() == VTE_CMD_LF)
                        printout();
        }

        void enter_data_syntax(DataSyntax syntax) noexcept
        {
                switch (syntax) {
                default:
                        break;
                }
        }

        void leave_data_syntax(DataSyntax syntax,
                               bool success) noexcept
        {
                switch (syntax) {
                default:
                        break;
                }
        }

        void reset() noexcept
        {
        }

}; // class PrettyPrinter

class Linter {
private:
        G_GNUC_PRINTF(2, 3)
        void
        warn(char const* format,
             ...) const noexcept
        {
                va_list args;
                va_start(args, format);
                char* str = g_strdup_vprintf(format, args);
                va_end(args);
                g_print("WARNING: %s\n", str);
                g_free(str);
        }

        void
        warn_deprecated(int cmd,
                        int replacement_cmd) const noexcept
        {
                warn("%s is deprecated; use %s instead",
                     cmd_to_str(cmd),
                     cmd_to_str(replacement_cmd));
        }

        void
        check_sgr_number(int sgr) noexcept
        {
                switch (sgr) {
                case -1:
#define _VTE_SGR(name, value) case value:
#define _VTE_NGR(...)
#include "parser-sgr.hh"
#undef _VTE_SGR
#undef _VTE_NGR
                case VTE_SGR_SET_FORE_LEGACY_START+1 ... VTE_SGR_SET_FORE_LEGACY_END-1:
                case VTE_SGR_SET_FORE_LEGACY_BRIGHT_START+1 ... VTE_SGR_SET_FORE_LEGACY_BRIGHT_END-1:
                case VTE_SGR_SET_BACK_LEGACY_START+1 ... VTE_SGR_SET_BACK_LEGACY_END-1:
                case VTE_SGR_SET_BACK_LEGACY_BRIGHT_START+1 ... VTE_SGR_SET_BACK_LEGACY_BRIGHT_END-1:
                        break;

#define _VTE_SGR(...)
#define _VTE_NGR(name, value) case value:
#include "parser-sgr.hh"
#undef _VTE_SGR
#undef _VTE_NGR
                case VTE_SGR_SET_FONT_FIRST+1 ... VTE_SGR_SET_FONT_LAST-1:
                        warn("SGR %d is unsupported", sgr);
                        break;

                default:
                        warn("SGR %d is unknown", sgr);
                        break;

                }
        }

        void
        check_sgr_color(vte::parser::Sequence const& seq,
                        unsigned int& idx) noexcept
        {
                auto const sgr = seq.param(idx);

                /* Simplified and adapted from sgr.hh */
                if (seq.param_nonfinal(idx)) {
                        /* Colon version */
                        auto const param = seq.param(++idx);
                        switch (param) {
                        case 2: {
                                auto const n = seq.next(idx) - idx;
                                if (n < 4)
                                        warn("SGR %d:2 not enough parameters", sgr);
                                else if (n == 4)
                                        warn("SGR %d:2:r:g:b is deprecated; use SGR %d:2::r:g:b instead",
                                             sgr, sgr);
                                break;
                        }
                        case 5: {
                                auto const n = seq.next(idx) - idx;
                                if (n < 2)
                                        warn("SGR %d:5 not enough parameters", sgr);
                                break;
                        }
                        case -1:
                                warn("SGR %d does not admit default parameters", sgr);
                                break;
                        case 0:
                        case 1:
                        case 3:
                        case 4:
                                warn("SGR %d:%d is unsupported", sgr, param);
                                break;
                        default:
                                warn("SGR %d:%d is unknown", sgr, param);
                        }
                } else {
                        /* Semicolon version */
                        idx = seq.next(idx);
                        auto const param = seq.param(idx);
                        switch (param) {
                        case 2:
                                /* Consume 3 more parameters */
                                idx = seq.next(idx);
                                idx = seq.next(idx);
                                idx = seq.next(idx);
                                warn("SGR %d;%d;r;g;b is deprecated; use SGR %d:%d::r:g:b instead",
                                     sgr, param, sgr, param);
                                break;
                        case 5:
                                /* Consume 1 more parameter */
                                idx = seq.next(idx);
                                warn("SGR %d;%d;index is deprecated; use SGR %d:%d:index instead",
                                     sgr, param, sgr, param);
                                break;
                        case -1:
                                warn("SGR %d does not admit default parameters", sgr);
                                break;
                        case 0:
                        case 1:
                        case 3:
                        case 4:
                                warn("SGR %d;%d;... is unsupported; use SGR %d:%d:... instead",
                                     sgr, param, sgr, param);
                                break;
                        default:
                                warn("SGR %d;%d is unknown", sgr, param);
                                break;
                        }
                }
        }

        void
        check_sgr_underline(vte::parser::Sequence const& seq,
                            unsigned int idx) noexcept
        {
                auto const sgr = seq.param(idx);

                int param = 1;
                /* If we have a subparameter, get it */
                if (seq.param_nonfinal(idx))
                        param = seq.param(idx + 1);

                switch (param) {
                case -1:
                case 0:
                case 1:
                case 2:
                case 3:
                        break;
                case 4:
                case 5:
                        warn("SGR %d:%d is unsupported", sgr, param);
                        break;
                default:
                        warn("SGR %d:%d is unknown", sgr, param);
                        break;
                }
        }

        void
        check_sgr(vte::parser::Sequence const& seq) noexcept
        {
                for (unsigned int i = 0; i < seq.size(); i = seq.next(i)) {
                        auto const param = seq.param(i, 0);

                        check_sgr_number(param);

                        switch (param) {
                        case VTE_SGR_SET_UNDERLINE:
                                check_sgr_underline(seq, i);
                                break;

                        case VTE_SGR_SET_FORE_SPEC:
                        case VTE_SGR_SET_BACK_SPEC:
                        case VTE_SGR_SET_DECO_SPEC:
                                check_sgr_color(seq, i);
                                break;

                        default:
                                if (seq.param_nonfinal(i))
                                        warn("SGR %d does not admit subparameters", param);
                                break;
                        }
                }
        }

public:
        Linter() noexcept = default;
        ~Linter() noexcept = default;

        void VT(vte::parser::Sequence const& seq) noexcept
        {
                auto cmd = seq.command();
                switch (cmd) {
                case VTE_CMD_OSC:
                        if (seq.st() == 7 /* BEL */)
                                warn("OSC terminated by BEL may be ignored; use ST (ESC \\) instead.");
                        break;

                case VTE_CMD_DECSLRM_OR_SCOSC:
                        cmd = VTE_CMD_SCOSC;
                        [[fallthrough]];
                case VTE_CMD_SCOSC:
                        warn_deprecated(cmd, VTE_CMD_DECSC);
                        break;

                case VTE_CMD_SCORC:
                        warn_deprecated(cmd, VTE_CMD_DECRC);
                        break;

                case VTE_CMD_SGR:
                        check_sgr(seq);
                        break;

                default:
                        if (cmd >= VTE_CMD_NOP_FIRST)
                                warn("%s is unimplemented", cmd_to_str(cmd));
                        break;
                }
        }

        void enter_data_syntax(DataSyntax syntax) noexcept
        {
        }

        void leave_data_syntax(DataSyntax syntax,
                               bool success) noexcept
        {
        }

        void reset() noexcept
        {
        }

}; // class Linter

class Sink {
public:
        void VT(vte::parser::Sequence const& seq) noexcept { }

        void enter_data_syntax(DataSyntax syntax) noexcept { }
        void leave_data_syntax(DataSyntax syntax,
                               bool success) noexcept { }

        void reset() noexcept { }

}; // class Sink

template<class D>
class Processor {
private:
        using Delegate = D;

        D& m_delegate;
        size_t m_buffer_size{0};
        bool m_statistics{false};
        bool m_benchmark{false};

        gsize m_seq_stats[VTE_SEQ_N];
        gsize m_cmd_stats[VTE_CMD_N];
        GArray* m_bench_times;

        static constexpr const size_t k_buf_overlap = 1u;

        vte::base::UTF8Decoder m_utf8_decoder{};
        vte::parser::Parser m_parser{};

        DataSyntax m_primary_data_syntax{DataSyntax::ECMA48_UTF8};
        DataSyntax m_current_data_syntax{DataSyntax::ECMA48_UTF8};

        void reset() noexcept
        {
                switch (m_current_data_syntax) {
                case DataSyntax::ECMA48_UTF8:
                        m_parser.reset();
                        m_utf8_decoder.reset();
                        break;

                default:
                        break;
                }

                if (m_current_data_syntax != m_primary_data_syntax) {
                        m_current_data_syntax = m_primary_data_syntax;
                        reset();
                }

                m_delegate.reset();
        }

        [[gnu::always_inline]]
        bool
        process_seq(vte::parser::Sequence const& seq) noexcept
        {
                m_delegate.VT(seq);
                return true;
        }

        uint8_t const*
        process_data_utf8(uint8_t const* const bufstart,
                          uint8_t const* const bufend,
                          bool eos) noexcept
        {
                auto seq = vte::parser::Sequence{m_parser};

                for (auto sptr = bufstart; sptr < bufend; ) {
                        switch (m_utf8_decoder.decode(*(sptr++))) {
                        case vte::base::UTF8Decoder::REJECT_REWIND:
                                /* Rewind the stream.
                                 * Note that this will never lead to a loop, since in the
                                 * next round this byte *will* be consumed.
                                 */
                                --sptr;
                                [[fallthrough]];
                        case vte::base::UTF8Decoder::REJECT:
                                m_utf8_decoder.reset();
                                /* Fall through to insert the U+FFFD replacement character. */
                                [[fallthrough]];
                        case vte::base::UTF8Decoder::ACCEPT: {
                                auto ret = m_parser.feed(m_utf8_decoder.codepoint());
                                if (G_UNLIKELY(ret < 0)) {
                                        g_printerr("Parser error!\n");
                                        return bufend;
                                }

                                m_seq_stats[ret]++;
                                if (ret != VTE_SEQ_NONE) {
                                        m_cmd_stats[seq.command()]++;
                                        if (!process_seq(seq))
                                                return sptr;
                                }
                                break;
                        }
                        default:
                                break;
                        }
                }

                if (eos &&
                    m_utf8_decoder.flush()) {
                        auto ret = m_parser.feed(m_utf8_decoder.codepoint());
                        if (G_UNLIKELY(ret < 0)) {
                                g_printerr("Parser error!\n");
                                return bufend;
                        }

                        m_seq_stats[ret]++;
                        if (ret != VTE_SEQ_NONE) {
                                m_cmd_stats[seq.command()]++;
                                if (!process_seq(seq))
                                        return bufend;
                        }
                }

                return bufend;
        }

        void
        process_fd(int fd)
        {
                auto buf = g_new0(uint8_t, m_buffer_size);

                auto start_time = g_get_monotonic_time();

                std::memset(buf, 0, k_buf_overlap);
                auto buf_start = k_buf_overlap;
                for (;;) {
                        auto len = read(fd, buf + buf_start, m_buffer_size - buf_start);
                        if (len == -1) {
                                if (errno == EAGAIN)
                                        continue;
                                break;
                        }

                        auto const eos = (len == 0);
                        uint8_t const* bufstart = buf + buf_start;
                        auto const bufend = bufstart + len;

                        for (auto sptr = bufstart; ; ) {
                                switch (m_current_data_syntax) {
                                case DataSyntax::ECMA48_UTF8:
                                        sptr = process_data_utf8(sptr, bufend, eos);
                                        break;

                                default:
                                        g_assert_not_reached();
                                        break;
                                }

                                if (sptr == bufend)
                                        break;
                        }

                        if (eos)
                                break;

                        /* Chain buffers by copying data from end of buf to the start */
                        std::memmove(buf, buf + buf_start + len - k_buf_overlap, k_buf_overlap);
                }

                int64_t time_spent = g_get_monotonic_time() - start_time;
                g_array_append_val(m_bench_times, time_spent);

                g_free(buf);
        }

        bool
        process_file(int fd,
                     int repeat)
        {
                if (fd == STDIN_FILENO && repeat != 1) {
                        g_printerr("Cannot consume STDIN more than once\n");
                        return false;
                }

                for (auto i = 0; i < repeat; ++i) {
                        if (i > 0 && lseek(fd, 0, SEEK_SET) != 0) {
                                auto errsv = vte::libc::ErrnoSaver{};
                                g_printerr("Failed to seek: %s\n", g_strerror(errsv));
                                return false;
                        }

                        reset();
                        process_fd(fd);
                }

                return true;
        }

public:

        Processor(Delegate& delegate,
                  size_t buffer_size,
                  bool statistics,
                  bool benchmark) noexcept
                : m_delegate{delegate},
                  m_buffer_size{std::max(buffer_size, k_buf_overlap + 1)},
                  m_statistics{statistics},
                  m_benchmark{benchmark}
        {
                memset(&m_seq_stats, 0, sizeof(m_seq_stats));
                memset(&m_cmd_stats, 0, sizeof(m_cmd_stats));
                m_bench_times = g_array_new(false, true, sizeof(int64_t));
        }

        ~Processor() noexcept
        {
                if (m_statistics)
                        print_statistics();
                if (m_benchmark)
                        print_benchmark();

                g_array_free(m_bench_times, true);
        }

        bool
        process_files(char const* const* filenames,
                      int repeat)
        {
                bool r = true;
                if (filenames != nullptr) {
                        for (auto i = 0; filenames[i] != nullptr; i++) {
                                char const* filename = filenames[i];

                                int fd = -1;
                                if (g_str_equal(filename, "-")) {
                                        fd = STDIN_FILENO;
                                } else {
                                        fd = open(filename, O_RDONLY);
                                        if (fd == -1) {
                                                auto errsv = vte::libc::ErrnoSaver{};
                                                g_printerr("Error opening file %s: %s\n",
                                                           filename, g_strerror(errsv));
                                        }
                                }
                                if (fd != -1) {
                                        r = process_file(fd, repeat);
                                        if (fd != STDIN_FILENO)
                                                close(fd);
                                        if (!r)
                                                break;
                                }
                        }
                } else {
                        r = process_file(STDIN_FILENO, repeat);
                }

                return r;
        }

        void print_statistics() const noexcept
        {
                for (unsigned int s = VTE_SEQ_NONE + 1; s < VTE_SEQ_N; s++) {
                        g_printerr("%\'16" G_GSIZE_FORMAT " %s\n",  m_seq_stats[s], seq_to_str(s));
                }

                g_printerr("\n");
                for (unsigned int s = 0; s < VTE_CMD_N; s++) {
                        if (m_cmd_stats[s] > 0) {
                                g_printerr("%\'16" G_GSIZE_FORMAT " %s%s\n",
                                           m_cmd_stats[s],
                                           cmd_to_str(s),
                                           s >= VTE_CMD_NOP_FIRST ? " [NOP]" : "");
                        }
                }
        }

        void print_benchmark() const noexcept
        {
                g_array_sort(m_bench_times,
                             [](void const* p1, void const* p2) -> int {
                                     int64_t const t1 = *(int64_t const*)p1;
                                     int64_t const t2 = *(int64_t const*)p2;
                                     return t1 == t2 ? 0 : (t1 < t2 ? -1 : 1);
                             });

                int64_t total_time = 0;
                for (unsigned int i = 0; i < m_bench_times->len; ++i)
                        total_time += g_array_index(m_bench_times, int64_t, i);

                g_printerr("\nTimes: best %\'" G_GINT64_FORMAT "µs "
                           "worst %\'" G_GINT64_FORMAT "µs "
                           "average %\'" G_GINT64_FORMAT "µs\n",
                           g_array_index(m_bench_times, int64_t, 0),
                           g_array_index(m_bench_times, int64_t, m_bench_times->len - 1),
                           total_time / (int64_t)m_bench_times->len);
                for (unsigned int i = 0; i < m_bench_times->len; ++i)
                        g_printerr("  %\'" G_GINT64_FORMAT "µs\n",
                                   g_array_index(m_bench_times, int64_t, i));
        }

}; // class Processor

class Options {
private:
        bool m_benchmark{false};
        bool m_codepoints{false};
        bool m_lint{false};
        bool m_plain{false};
        bool m_quiet{false};
        bool m_statistics{false};
        int m_buffer_size{16384};
        int m_repeat{1};
        vte::glib::StrvPtr m_filenames{};

public:

        Options() noexcept = default;
        Options(Options const&) = delete;
        Options(Options&&) = delete;

        ~Options() = default;

        inline constexpr bool   benchmark()   const noexcept { return m_benchmark;  }
        inline constexpr size_t buffer_size() const noexcept { return m_buffer_size; }
        inline constexpr bool   codepoints()  const noexcept { return m_codepoints; }
        inline constexpr bool   lint()        const noexcept { return m_lint;       }
        inline constexpr bool   plain()       const noexcept { return m_plain;      }
        inline constexpr bool   quiet()       const noexcept { return m_quiet;      }
        inline constexpr bool   statistics()  const noexcept { return m_statistics; }
        inline constexpr int    repeat()      const noexcept { return m_repeat;     }
        inline char const* const* filenames() const noexcept { return m_filenames.get(); }

        bool parse(int argc,
                   char* argv[],
                   GError** error) noexcept
        {
                using BoolOption = vte::ValueGetter<bool, gboolean>;
                using IntOption = vte::ValueGetter<int, int>;
                using StrvOption = vte::ValueGetter<vte::glib::StrvPtr, char**, nullptr>;

                auto benchmark = BoolOption{m_benchmark, false};
                auto codepoints = BoolOption{m_codepoints, false};
                auto lint = BoolOption{m_lint, false};
                auto plain = BoolOption{m_plain, false};
                auto quiet = BoolOption{m_quiet, false};
                auto statistics = BoolOption{m_statistics, false};
                auto buffer_size = IntOption{m_buffer_size, 16384};
                auto repeat = IntOption{m_repeat, 1};
                auto filenames = StrvOption{m_filenames, nullptr};

                GOptionEntry const entries[] = {
                        { "benchmark", 'b', 0, G_OPTION_ARG_NONE, &benchmark,
                          "Measure time spent parsing each file", nullptr },
                        { "buffer-size", 'B', 0, G_OPTION_ARG_INT, &buffer_size,
                          "Buffer size", "SIZE" },
                        { "codepoints", 'u', 0, G_OPTION_ARG_NONE, &codepoints,
                          "Output unicode code points by number", nullptr },
                        { "lint", 'l', 0, G_OPTION_ARG_NONE, &lint,
                          "Check input", nullptr },
                        { "plain", 'p', 0, G_OPTION_ARG_NONE, &plain,
                          "Output plain text without attributes", nullptr },
                        { "quiet", 'q', 0, G_OPTION_ARG_NONE, &quiet,
                          "Suppress output except for statistics and benchmark", nullptr },
                        { "repeat", 'r', 0, G_OPTION_ARG_INT, &repeat,
                          "Repeat each file COUNT times", "COUNT" },
                        { "statistics", 's', 0, G_OPTION_ARG_NONE, &statistics,
                          "Output statistics", nullptr },
                        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames,
                          nullptr, nullptr },
                        { nullptr },
                };

                auto context = vte::take_freeable(g_option_context_new("[FILE…] — parser cat"));
                g_option_context_set_help_enabled(context.get(), true);
                g_option_context_add_main_entries(context.get(), entries, nullptr);

                return g_option_context_parse(context.get(), &argc, &argv, error);
        }
}; // class Options

template<class D>
static bool
process(Options const& options,
        D delegate)
{
        auto proc = Processor{delegate,
                              options.buffer_size(),
                              options.statistics(),
                              options.benchmark()};

        return proc.process_files(options.filenames(), options.repeat());
}

int
main(int argc,
     char *argv[])
{
        setlocale(LC_ALL, "");
        _vte_debug_init();

        Options options{};
        auto error = vte::glib::Error{};
        if (!options.parse(argc, argv, error)) {
                g_printerr("Failed to parse arguments: %s\n", error.message());
                return EXIT_FAILURE;
        }

        auto rv = false;
        if (options.lint()) {
                if (options.repeat() != 1) {
                        g_printerr("Cannot use repeat option for linter\n");
                } else {
                        rv = process(options, Linter{});
                }
        } else if (options.quiet()) {
                rv = process(options, Sink{});
        } else {
                rv = process(options, PrettyPrinter{options.plain(), options.codepoints()});
        }

        return rv ? EXIT_SUCCESS : EXIT_FAILURE;
}
