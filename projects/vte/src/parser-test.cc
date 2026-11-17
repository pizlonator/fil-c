/*
 * Copyright © 2017, 2018 Christian Persch
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

#include <string.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace std::literals;

#include <glib.h>

#include "parser.hh"
#include "parser-glue.hh"

#include "parser-charset-tables.hh"

#ifdef PARSER_INCLUDE_NOP
#define _VTE_NOP(...) _VTE_CMD(__VA_ARGS__)
#define _VTE_NOQ(...) _VTE_SEQ(__VA_ARGS__)
#else
#define _VTE_NOP(...)
#define _VTE_NOQ(...)
#endif

using namespace vte::parser;

Parser parser{};
Sequence seq{parser};

#if 0
static char const*
seq_to_str(unsigned int type)
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
        case VTE_SEQ_APC: return "APC";
        case VTE_SEQ_PM: return "PM";
        case VTE_SEQ_SOS: return "SOS";
        case VTE_SEQ_SCI: return "SCI";
        default:
                g_assert_not_reached();
        }
}

static char const*
cmd_to_str(unsigned int command)
{
        switch (command) {
#define _VTE_CMD(cmd) case VTE_CMD_##cmd: return #cmd;
#include "parser-cmd.hh"
#undef _VTE_CMD
        default:
                static char buf[32];
                snprintf(buf, sizeof(buf), "UNKOWN(%u)", command);
                return buf;
        }
}

static char const*
charset_to_str(unsigned int cs)
{
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

static const char c0str[][6] = {
        "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
        "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI",
        "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
        "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US",
        "SPACE"
};

static const char c1str[][5] = {
        "DEL",
        "0x80", "0x81", "BPH", "NBH", "0x84", "NEL", "SSA", "ESA",
        "HTS", "HTJ", "VTS", "PLD", "PLU", "RI", "SS2", "SS3",
        "DCS", "PU1", "PU2", "STS", "CCH", "MW", "SPA", "EPA",
        "SOS", "0x99", "SCI", "CSI", "ST", "OSC", "PM", "APC"
};

static void
print_escaped(std::u32string const& s)
{
        for (auto it : s) {
                uint32_t c = (char32_t)it;

                if (c <= 0x20)
                        g_print("%s ", c0str[c]);
                else if (c < 0x7f)
                        g_print("%c ", c);
                else if (c < 0xa0)
                        g_print("%s ", c1str[c - 0x7f]);
                else
                        g_print("U+%04X", c);
        }
        g_print("\n");
}

#if 0
static void
print_seq()
{
        auto c = seq.terminator();
        if (seq.command() == VTE_CMD_GRAPHIC) {
                char buf[7];
                buf[g_unichar_to_utf8(c, buf)] = 0;
                g_print("%s U+%04X [%s]\n", cmd_to_str(seq.command()),
                        c,
                        g_unichar_isprint(c) ? buf : "�");
        } else {
                g_print("%s", cmd_to_str(seq.command()));
                if (seq.size()) {
                        g_print(" ");
                        for (unsigned int i = 0; i < seq.size(); i++) {
                                if (i > 0)
                                        g_print(";");
                                g_print("%d", seq.param(i));
                        }
                }
                g_print("\n");
        }
}
#endif

class vte_seq_builder : public u32SequenceBuilder {
public:
        vte_seq_builder(unsigned int type,
                        uint32_t f)
                : u32SequenceBuilder(type, f)
        {
        }

        vte_seq_builder(unsigned int type,
                        u32SequenceBuilder::string_type const& str)
                : u32SequenceBuilder(type)
        {
                set_string(str);
        }

        void set_intermediates(uint32_t* i,
                               unsigned int ni) noexcept
        {
                for (unsigned int n = 0; n < ni; ++n)
                        append_intermediate(i[n]);
        }


        void set_params(int* params,
                        unsigned int n) noexcept
        {
                for (unsigned int i = 0; i < n; ++i)
                        append_param(params[i]);
        }

        void print(bool c1 = false) const noexcept
        {
                std::u32string s;
                to_string(s, c1);
                print_escaped(s);
        }
};

static int
feed_parser(std::u32string const& s)
{
        int rv = VTE_SEQ_NONE;
        for (auto it : s) {
                rv = parser.feed((uint32_t)(char32_t)it);
                if (rv < 0)
                        break;
        }
        return rv;
}

static int
feed_parser(vte_seq_builder& b,
            bool c1 = false)
{
        std::u32string s;
        b.to_string(s, c1);

        return feed_parser(s);
}

static void
test_seq_arg(void)
{
        /* Basic test */
        vte_seq_arg_t arg = VTE_SEQ_ARG_INIT_DEFAULT;
        g_assert_false(vte_seq_arg_started(arg));
        g_assert_true(vte_seq_arg_default(arg));

        vte_seq_arg_push(&arg, '1');
        vte_seq_arg_push(&arg, '2');
        vte_seq_arg_push(&arg, '3');
        vte_seq_arg_finish(&arg);

        g_assert_cmpint(vte_seq_arg_value(arg), ==, 123);
        g_assert_false(vte_seq_arg_default(arg));

        /* Test max value */
        arg = VTE_SEQ_ARG_INIT_DEFAULT;
        vte_seq_arg_push(&arg, '6');
        vte_seq_arg_push(&arg, '5');
        vte_seq_arg_push(&arg, '5');
        vte_seq_arg_push(&arg, '3');
        vte_seq_arg_push(&arg, '6');
        vte_seq_arg_finish(&arg);

        g_assert_cmpint(vte_seq_arg_value(arg), ==, 65535);
}

static void
test_seq_string(void)
{
        vte_seq_string_t str;
        vte_seq_string_init(&str);

        size_t len;
        auto buf = vte_seq_string_get(&str, &len);
        g_assert_cmpuint(len, ==, 0);

        for (unsigned int i = 0; i < VTE_SEQ_STRING_MAX_CAPACITY; ++i) {
                auto rv = vte_seq_string_push(&str, 0xfffdU);
                g_assert_true(rv);

                buf = vte_seq_string_get(&str, &len);
                g_assert_cmpuint(len, ==, i + 1);
        }

        /* Try one more */
        auto rv = vte_seq_string_push(&str, 0xfffdU);
        g_assert_false(rv);

        buf = vte_seq_string_get(&str, &len);
        for (unsigned int i = 0; i < len; i++)
                g_assert_cmpuint(buf[i], ==, 0xfffdU);

        vte_seq_string_reset(&str);
        buf = vte_seq_string_get(&str, &len);
        g_assert_cmpuint(len, ==, 0);

        vte_seq_string_free(&str);
}

static void
test_seq_control(void)
{
        static struct {
                uint32_t c;
                unsigned int cmd;
        } const controls [] = {
#define _VTE_SEQ(cmd,type,f,pi,ni,i0,flags) { f, VTE_CMD_##cmd },
#include "parser-c01.hh"
#undef _VTE_SEQ
        };

        for (unsigned int i = 0; i < G_N_ELEMENTS(controls); i++) {
                parser.reset();
                auto rv = parser.feed(controls[i].c);
                g_assert_cmpuint(rv, ==, VTE_SEQ_CONTROL);
                g_assert_cmpuint(controls[i].cmd, ==, seq.command());
        }
}

static void
test_seq_esc_invalid(void)
{
        /* Tests invalid ESC 0/n and ESC 1/n sequences, which should never result in
         * an VTE_SEQ_ESCAPE type sequence, but instead always in the C0 control.
         */
        for (uint32_t f = 0x0; f < 0x20; f++) {
                parser.reset();

                vte_seq_builder b{VTE_SEQ_ESCAPE, f};
                auto rv = feed_parser(b);
                g_assert_cmpint(rv, !=, VTE_SEQ_ESCAPE);
        }
}

static void
test_seq_esc(uint32_t f,
             uint32_t i[],
             unsigned int ni)
{
        vte_seq_builder b{VTE_SEQ_ESCAPE, f};
        b.set_intermediates(i, ni);

        parser.reset();
        auto rv = feed_parser(b);
        if (rv != VTE_SEQ_ESCAPE)
                return;

        b.assert_equal(seq);
}

static void
test_seq_esc_nF(void)
{
        /* Tests nF sequences, that is ESC 2/n [2/m..] F with F being 3/0..7/14.
         * They could have any number of itermediates, but we only test up to 4.
         */

        uint32_t i[4];
        for (uint32_t f = 0x30; f < 0x7f; f++) {
                test_seq_esc(f, i, 0);
                for (i[0] = 0x20; i[0] < 0x30; i[0]++) {
                        test_seq_esc(f, i, 1);
                        for (i[1] = 0x20; i[1] < 0x30; i[1]++) {
                                test_seq_esc(f, i, 2);
                                for (i[2] = 0x20; i[2] < 0x30; i[2]++) {
                                        test_seq_esc(f, i, 3);
                                        for (i[3] = 0x20; i[3] < 0x30; i[3]++) {
                                                test_seq_esc(f, i, 4);
                                        }
                                }
                        }
                }
        }
}

static void
test_seq_esc_charset(uint32_t f, /* final */
                     uint32_t i[], /* intermediates */
                     unsigned int ni, /* number of intermediates */
                     unsigned int cmd, /* expected command */
                     unsigned int cs /* expected charset */,
                     unsigned int slot /* expected slot */)
{
        vte_seq_builder b{VTE_SEQ_ESCAPE, f};
        b.set_intermediates(i, ni);

        parser.reset();
        auto rv = feed_parser(b);
        g_assert_cmpint(rv, ==, VTE_SEQ_ESCAPE);
        b.assert_equal(seq);

        g_assert_cmpint(seq.command(), ==, cmd);
        g_assert_cmpint(seq.charset(), ==, cs);
        g_assert_cmpint(seq.slot(), ==, slot);
}

static void
test_seq_esc_charset(uint32_t i[], /* intermediates */
                     unsigned int ni, /* number of intermediates */
                     uint8_t const* const table, /* table */
                     unsigned int ntable, /* number of table entries */
                     uint32_t ts, /* start of table */
                     unsigned int cmd, /* expected command */
                     unsigned int defaultcs /* default charset */,
                     unsigned int slot /* expected slot */)
{
        for (uint32_t f = 0x30; f < 0x7f; f++) {
                int cs;

                if (f >= ts && f < (ts + ntable))
                        cs = table[f - ts];
                else if (f == 0x7e &&
                         cmd != VTE_CMD_DOCS &&
                         defaultcs != VTE_CHARSET_DRCS)
                        cs = VTE_CHARSET_EMPTY;
                else
                        cs = defaultcs;

                test_seq_esc_charset(f, i, ni, cmd, cs, slot);
        }
}

static void
test_seq_esc_charset_94(void)
{
        uint32_t i[4];

        /* Single byte 94-sets */
        for (i[0] = 0x28; i[0] <= 0x2b; i[0]++) {
                int slot = i[0] - 0x28;

                test_seq_esc_charset(i, 1,
                                     charset_graphic_94,
                                     G_N_ELEMENTS(charset_graphic_94),
                                     0x30, VTE_CMD_GnDm, VTE_CHARSET_NONE, slot);

                i[1] = 0x20;
                test_seq_esc_charset(i, 2, nullptr, 0, 0,
                                     VTE_CMD_GnDm, VTE_CHARSET_DRCS, slot);

                i[1] = 0x21;
                test_seq_esc_charset(i, 2,
                                     charset_graphic_94_with_2_1,
                                     G_N_ELEMENTS(charset_graphic_94_with_2_1),
                                     0x40, VTE_CMD_GnDm, VTE_CHARSET_NONE, slot);

                i[1] = 0x22;
                test_seq_esc_charset(i, 2,
                                     charset_graphic_94_with_2_2,
                                     G_N_ELEMENTS(charset_graphic_94_with_2_2),
                                     0x30, VTE_CMD_GnDm, VTE_CHARSET_NONE, slot);

                i[1] = 0x23;
                test_seq_esc_charset(i, 2, nullptr, 0,
                                     0x30, VTE_CMD_GnDm, VTE_CHARSET_NONE, slot);

                /* 2/4 is multibyte charsets */

                i[1] = 0x25;
                test_seq_esc_charset(i, 2,
                                     charset_graphic_94_with_2_5,
                                     G_N_ELEMENTS(charset_graphic_94_with_2_5),
                                     0x30, VTE_CMD_GnDm, VTE_CHARSET_NONE, slot);

                i[1] = 0x26;
                test_seq_esc_charset(i, 2,
                                     charset_graphic_94_with_2_6,
                                     G_N_ELEMENTS(charset_graphic_94_with_2_6),
                                     0x30, VTE_CMD_GnDm, VTE_CHARSET_NONE, slot);

                i[1] = 0x27;
                test_seq_esc_charset(i, 2, nullptr, 0, 0,
                                     VTE_CMD_GnDm, VTE_CHARSET_NONE, slot);
        }
}

static void
test_seq_esc_charset_96(void)
{
        uint32_t i[4];

        /* Single byte 96-sets */
        for (i[0] = 0x2d; i[0] <= 0x2f; i[0]++) {
                int slot = i[0] - 0x2c;

                test_seq_esc_charset(i, 1,
                                     charset_graphic_96,
                                     G_N_ELEMENTS(charset_graphic_96),
                                     0x30, VTE_CMD_GnDm, VTE_CHARSET_NONE, slot);

                i[1] = 0x20;
                test_seq_esc_charset(i, 2, nullptr, 0, 0,
                                     VTE_CMD_GnDm, VTE_CHARSET_DRCS, slot);

                /* 2/4 is multibyte charsets, 2/5 is DOCS. Other indermediates may be present
                 * in Fp sequences, but none are actually in use.
                 */
                for (i[1] = 0x21; i[1] < 0x28; i[1]++) {
                        if (i[1] == 0x24 || i[1] == 0x25)
                                continue;

                        test_seq_esc_charset(i, 2, nullptr, 0, 0,
                                             VTE_CMD_GnDm, VTE_CHARSET_NONE, slot);
                }
        }
}

static void
test_seq_esc_charset_94_n(void)
{
        uint32_t i[4];

        /* Multibyte 94-sets */
        i[0] = 0x24;
        for (i[1] = 0x28; i[1] <= 0x2b; i[1]++) {
                int slot = i[1] - 0x28;

                test_seq_esc_charset(i, 2,
                                     charset_graphic_94_n,
                                     G_N_ELEMENTS(charset_graphic_94_n),
                                     0x30, VTE_CMD_GnDMm, VTE_CHARSET_NONE, slot);

                i[2] = 0x20;
                test_seq_esc_charset(i, 3, nullptr, 0, 0,
                                     VTE_CMD_GnDMm, VTE_CHARSET_DRCS, slot);

                i[2] = 0x21;
                test_seq_esc_charset(i, 3,
                                     charset_graphic_94_n_with_2_1,
                                     G_N_ELEMENTS(charset_graphic_94_n_with_2_1),
                                     0x30, VTE_CMD_GnDMm, VTE_CHARSET_NONE, slot);

                /* There could be one more intermediate byte. */
                for (i[2] = 0x22; i[2] < 0x28; i[2]++) {
                        if (i[2] == 0x24) /* TODO */
                                continue;

                        test_seq_esc_charset(i, 3, nullptr, 0, 0,
                                             VTE_CMD_GnDMm, VTE_CHARSET_NONE, slot);
                }
        }

        /* As a special exception, ESC 2/4 4/[012] are also possible */
        test_seq_esc_charset(0x40, i, 1, VTE_CMD_GnDMm, charset_graphic_94_n[0x40 - 0x30], 0);
        test_seq_esc_charset(0x41, i, 1, VTE_CMD_GnDMm, charset_graphic_94_n[0x41 - 0x30], 0);
        test_seq_esc_charset(0x42, i, 1, VTE_CMD_GnDMm, charset_graphic_94_n[0x42 - 0x30], 0);
}

static void
test_seq_esc_charset_96_n(void)
{
        uint32_t i[4];

        /* Multibyte 94-sets */
        i[0] = 0x24;
        for (i[1] = 0x2d; i[1] <= 0x2f; i[1]++) {
                int slot = i[1] - 0x2c;

                test_seq_esc_charset(i, 2, nullptr, 0, 0,
                                     VTE_CMD_GnDMm, VTE_CHARSET_NONE, slot);

                i[2] = 0x20;
                test_seq_esc_charset(i, 3, nullptr, 0, 0,
                                     VTE_CMD_GnDMm, VTE_CHARSET_DRCS, slot);

                /* There could be one more intermediate byte. */
                for (i[2] = 0x21; i[2] < 0x28; i[2]++) {
                        test_seq_esc_charset(i, 3, nullptr, 0, 0,
                                             VTE_CMD_GnDMm, VTE_CHARSET_NONE, slot);
                }
        }
}

static void
test_seq_esc_charset_control(void)
{
        uint32_t i[4];

        /* C0 controls: ESC 2/1 F */
        i[0] = 0x21;
        test_seq_esc_charset(i, 1,
                             charset_control_c0,
                             G_N_ELEMENTS(charset_control_c0),
                             0x40, VTE_CMD_CnD, VTE_CHARSET_NONE, 0);

        /* C1 controls: ESC 2/2 F */
        i[0] = 0x22;
        test_seq_esc_charset(i, 1,
                             charset_control_c1,
                             G_N_ELEMENTS(charset_control_c1),
                             0x40, VTE_CMD_CnD, VTE_CHARSET_NONE, 1);
}

static void
test_seq_esc_charset_other(void)
{
        uint32_t i[4];

        /* Other coding systems: ESC 2/5 F or ESC 2/5 I F */
        i[0] = 0x25;
        test_seq_esc_charset(i, 1,
                             charset_ocs,
                             G_N_ELEMENTS(charset_ocs),
                             0x30, VTE_CMD_DOCS, VTE_CHARSET_NONE, 0);

        i[1] = 0x20;
        test_seq_esc_charset(i, 2,
                             charset_ocs_with_2_0,
                             G_N_ELEMENTS(charset_ocs_with_2_0),
                             0x30, VTE_CMD_DOCS, VTE_CHARSET_NONE, 0);

        i[1] = 0x2f;
        test_seq_esc_charset(i, 2,
                             charset_ocs_with_2_15,
                             G_N_ELEMENTS(charset_ocs_with_2_15),
                             0x40, VTE_CMD_DOCS, VTE_CHARSET_NONE, 0);
}

static void
test_seq_esc_Fpes(void)
{
        /* Tests Fp, Fe and Ft sequences, that is ESC 3/n .. ESC 7/14 */

        for (uint32_t f = 0x30; f < 0x7f; f++) {
                parser.reset();

                vte_seq_builder b{VTE_SEQ_ESCAPE, f};

                auto rv = feed_parser(b);
                int expected_rv;
                switch (f) {
                case 'P': /* DCS */
                case 'X': /* SOS */
                case 'Z': /* SCI */
                case '_': /* APC */
                case '[': /* CSI */
                case ']': /* OSC */
                case '^': /* PM */
                        expected_rv = VTE_SEQ_NONE;
                        break;
                default:
                        expected_rv = VTE_SEQ_ESCAPE;
                        break;
                }
                g_assert_cmpint(rv, ==, expected_rv);
                if (rv != VTE_SEQ_NONE)
                        b.assert_equal(seq);
        }
}

static void
test_seq_esc_known(uint32_t f,
                   uint32_t i,
                   unsigned int cmd)
{
        vte_seq_builder b{VTE_SEQ_ESCAPE, f};
        if (i != 0)
                b.set_intermediates(&i, 1);

        auto rv = feed_parser(b);
        g_assert_cmpint(rv, ==, VTE_SEQ_ESCAPE);
        g_assert_cmpint(seq.command(), ==, cmd);
}

static void
test_seq_esc_known(void)
{
        parser.reset();

#define _VTE_SEQ(cmd,type,f,p,ni,i,flags) \
        test_seq_esc_known(f, VTE_SEQ_INTERMEDIATE_CHAR_##i, VTE_CMD_##cmd);
#include "parser-esc.hh"
#undef _VTE_SEQ
}

static void
test_seq_csi(uint32_t f,
             uint32_t p,
             vte_seq_arg_t params[16],
             uint32_t i[4],
             unsigned int ni)
{
        vte_seq_builder b{VTE_SEQ_CSI, f};
        b.set_intermediates(i, ni);
        b.set_param_intro(p);

        int expected_rv = (f & 0xF0) == 0x30 ? VTE_SEQ_NONE : VTE_SEQ_CSI;

        for (unsigned int n = 0; n <= 16; n++) {
                b.reset_params();
                b.set_params(params, n);

                parser.reset();
                /* First with C0 CSI */
                auto rv = feed_parser(b, false);
                g_assert_cmpint(rv, ==, expected_rv);
                if (rv != VTE_SEQ_NONE)
                        b.assert_equal_full(seq);

                /* Now with C1 CSI */
                rv = feed_parser(b, true);
                if (rv != VTE_SEQ_NONE)
                        b.assert_equal_full(seq);
        }
}

static void
test_seq_csi(uint32_t p,
             vte_seq_arg_t params[16])
{
        uint32_t i[4];
        for (uint32_t f = 0x30; f < 0x7f; f++) {
                test_seq_csi(f, p, params, i, 0);
                for (i[0] = 0x20; i[0] < 0x30; i[0]++) {
                        test_seq_csi(f, p, params, i, 1);
                        for (i[1] = 0x20; i[1] < 0x30; i[1]++) {
                                test_seq_csi(f, p, params, i, 2);
                        }
                }
        }
}

static void
test_seq_csi(vte_seq_arg_t params[16])
{
        test_seq_csi(0, params);
        for (uint32_t p = 0x3c; p <= 0x3f; p++)
                test_seq_csi(p, params);
}

static void
test_seq_csi(void)
{
        /* Tests CSI sequences, that is sequences of the form
         * CSI P...P I...I F
         * with parameter bytes P from 3/0..3/15, intermediate bytes I from 2/0..2/15 and
         * final byte F from 4/0..7/14.
         * There could be any number of intermediate bytes, but we only test up to 2.
         * There could be any number of extra params bytes, but we only test up to 1.
         * CSI can be either the C1 control itself, or ESC [
         */
        vte_seq_arg_t params1[16]{ -1, 0, 1, 9, 10, 99, 100, 999,
                        1000, 9999, 10000, 65534, 65535, 65536, -1, -1 };
        test_seq_csi(params1);

        vte_seq_arg_t params2[16]{ 1, -1, -1, -1, 1, -1, 1, 1,
                        1, -1, -1, -1, -1, 1, 1, 1 };
        test_seq_csi(params2);
}

static void
test_seq_sci(uint32_t f,
             bool valid)
{
        vte_seq_builder b{VTE_SEQ_SCI, f};

        /* First with C0 SCI */
        auto rv = feed_parser(b, false);
        if (valid) {
                g_assert_cmpint(rv, ==, VTE_SEQ_SCI);
                b.assert_equal_full(seq);
        } else
                g_assert_cmpint(rv, !=, VTE_SEQ_SCI);

        /* Now with C1 SCI */
        rv = feed_parser(b, true);
        if (valid) {
                g_assert_cmpint(rv, ==, VTE_SEQ_SCI);
                b.assert_equal_full(seq);
        } else
                g_assert_cmpint(rv, !=, VTE_SEQ_SCI);
}

static void
test_seq_sci(void)
{
        /* Tests SCI sequences, that is sequences of the form SCI F
         * with final byte 0/8..0/13 or 2/0..7/14
         * SCI can be either the C1 control itself, or ESC Z
         */
        parser.reset();

        for (uint32_t f = 0x8; f <= 0xd; ++f)
                test_seq_sci(f, true);
        for (uint32_t f = 0x20; f <= 0x7e; ++f)
                test_seq_sci(f, true);
        for (uint32_t f = 0x7f; f <= 0xff; ++f)
                test_seq_sci(f, false);
}

G_GNUC_UNUSED
static void
test_seq_sci_known(uint32_t f,
                   unsigned int cmd)
{
        vte_seq_builder b{VTE_SEQ_SCI, f};

        auto rv = feed_parser(b);
        g_assert_cmpint(rv, ==, VTE_SEQ_SCI);
        g_assert_cmpint(seq.command(), ==, cmd);
}

static void
test_seq_sci_known(void)
{
        parser.reset();

#define _VTE_SEQ(cmd,type,f,p,ni,i,flags) \
        test_seq_sci_known(f, VTE_CMD_##cmd);
#include "parser-sci.hh"
#undef _VTE_SEQ
}

static void
test_seq_csi_known(uint32_t f,
                   uint32_t p,
                   uint32_t i,
                   unsigned int cmd)
{
        vte_seq_builder b{VTE_SEQ_CSI, f};
        if (p != 0)
                b.set_param_intro(p);
        if (i != 0)
                b.set_intermediates(&i, 1);

        auto rv = feed_parser(b);
        g_assert_cmpint(rv, ==, VTE_SEQ_CSI);
        g_assert_cmpint(seq.command(), ==, cmd);
}

static void
test_seq_csi_known(void)
{
        parser.reset();

#define _VTE_SEQ(cmd,type,f,p,ni,i,flags) \
        test_seq_csi_known(f, VTE_SEQ_PARAMETER_CHAR_##p, VTE_SEQ_INTERMEDIATE_CHAR_##i, VTE_CMD_##cmd);
#include "parser-csi.hh"
#undef _VTE_SEQ
}

static void
test_seq_dcs(uint32_t f,
             uint32_t p,
             vte_seq_arg_t params[16],
             uint32_t i[4],
             unsigned int ni,
             std::u32string const& str,
             int expected_rv = VTE_SEQ_DCS)
{
        vte_seq_builder b{VTE_SEQ_DCS, f};
        b.set_intermediates(i, ni);
        b.set_param_intro(p);
        b.set_string(str);

        int expected_rv0 = (f & 0xF0) == 0x30 || expected_rv == VTE_SEQ_NONE ? VTE_SEQ_ESCAPE /* the C0 ST */ : expected_rv;
        int expected_rv1 = (f & 0xF0) == 0x30 ? VTE_SEQ_NONE : expected_rv;

        for (unsigned int n = 0; n <= 16; n++) {
                b.reset_params();
                b.set_params(params, n);

                parser.reset();

                /* First with C0 DCS */
                auto rv0 = feed_parser(b, false);
                g_assert_cmpint(rv0, ==, expected_rv0);
                if (rv0 != VTE_SEQ_ESCAPE && rv0 != VTE_SEQ_NONE)
                        b.assert_equal_full(seq);
                if (rv0 == VTE_SEQ_ESCAPE)
                        g_assert_cmpint(seq.command(), ==, VTE_CMD_ST);

                /* Now with C1 DCS */
                auto rv1 = feed_parser(b, true);
                g_assert_cmpint(rv1, ==, expected_rv1);
                if (rv1 != VTE_SEQ_NONE)
                        b.assert_equal_full(seq);
        }
}

static void
test_seq_dcs(uint32_t p,
             vte_seq_arg_t params[16],
             std::u32string const& str,
             int expected_rv = VTE_SEQ_DCS)
{
        uint32_t i[4];
        for (uint32_t f = 0x40; f < 0x7f; f++) {
                test_seq_dcs(f, p, params, i, 0, str, expected_rv);
        }

        for (uint32_t f = 0x30; f < 0x7f; f++) {
                for (i[0] = 0x20; i[0] < 0x30; i[0]++) {
                        test_seq_dcs(f, p, params, i, 1, str, expected_rv);
                        for (i[1] = 0x20; i[1] < 0x30; i[1]++) {
                                test_seq_dcs(f, p, params, i, 2, str, expected_rv);
                        }
                }
        }
}

static void
test_seq_dcs(vte_seq_arg_t params[16],
             std::u32string const& str,
             int expected_rv = VTE_SEQ_DCS)
{
        test_seq_dcs(0, params, str);
        for (uint32_t p = 0x3c; p <= 0x3f; p++)
                test_seq_dcs(p, params, str, expected_rv);
}

static void
test_seq_dcs(std::u32string const& str,
             int expected_rv = VTE_SEQ_DCS)
{
        /* Tests DCS sequences, that is sequences of the form
         * DCS P...P I...I F D...D ST
         * with parameter bytes P from 3/0..3/15, intermediate bytes I from 2/0..2/15 and
         * final byte F from 4/0..7/14.
         * There could be any number of intermediate bytes, but we only test up to 2.
         * There could be any number of extra params bytes, but we only test up to 1.
         * DCS can be either the C1 control itself, or ESC [; ST can be either the C1
         * control itself, or ESC \\
         */
        vte_seq_arg_t params1[16]{ -1, 0, 1, 9, 10, 99, 100, 999,
                        1000, 9999, 10000, 65534, 65535, 65536, -1, -1 };
        test_seq_dcs(params1, str, expected_rv);

        vte_seq_arg_t params2[16]{ 1, -1, -1, -1, 1, -1, 1, 1,
                        1, -1, -1, -1, -1, 1, 1, 1 };
        test_seq_dcs(params2, str, expected_rv);
}

static void
test_seq_dcs_simple(std::u32string const& str,
                    int expected_rv = VTE_SEQ_DCS)
{
        vte_seq_arg_t params[16]{ 1, -1, -1, -1, 1, -1, 1, 1,
                        1, -1, -1, -1, -1, 1, 1, 1 };
        uint32_t i[4];

        test_seq_dcs(0x40, 0, params, i, 0, str, expected_rv);
}

static void
test_seq_dcs(void)
{
        /* Length exceeded */
        test_seq_dcs_simple(std::u32string(VTE_SEQ_STRING_MAX_CAPACITY + 1, 0x100000), VTE_SEQ_NONE);

        test_seq_dcs(U""s);
        test_seq_dcs(U"123;TESTING"s);
}

static void
test_seq_dcs_known(uint32_t f,
                   uint32_t p,
                   uint32_t i,
                   unsigned int cmd)
{
        vte_seq_builder b{VTE_SEQ_DCS, f};
        if (p != 0)
                b.set_param_intro(p);
        if (i != 0)
                b.set_intermediates(&i, 1);

        auto rv = feed_parser(b);
        g_assert_cmpint(rv, ==, VTE_SEQ_DCS);
        g_assert_cmpint(seq.command(), ==, cmd);
}

static void
test_seq_dcs_known(void)
{
        parser.reset();

#define _VTE_SEQ(cmd,type,f,p,ni,i,flags) \
        test_seq_dcs_known(f, VTE_SEQ_PARAMETER_CHAR_##p, VTE_SEQ_INTERMEDIATE_CHAR_##i, VTE_CMD_##cmd);
#include "parser-dcs.hh"
#undef _VTE_SEQ
}

static void
test_seq_parse(char const* str)
{
        std::u32string s;
        s.push_back(0x9B); /* CSI */
        for (unsigned int i = 0; str[i]; i++)
                s.push_back(str[i]);
        s.push_back(0x6d); /* m = SGR */

        parser.reset();
        auto rv = feed_parser(s);
        g_assert_cmpint(rv, ==, VTE_SEQ_CSI);
}

static void
test_seq_csi_param(char const* str,
                   std::vector<int> args,
                   std::vector<bool> args_nonfinal)
{
        g_assert_cmpuint(args.size(), ==, args_nonfinal.size());

        test_seq_parse(str);

        if (seq.size() < VTE_PARSER_ARG_MAX)
                g_assert_cmpuint(seq.size(), ==, args.size());

        unsigned int n_final_args = 0;
        for (unsigned int i = 0; i < seq.size(); i++) {
                g_assert_cmpint(seq.param(i), ==, args[i]);

                auto is_nonfinal = args_nonfinal[i];
                if (!is_nonfinal)
                        n_final_args++;

                g_assert_cmpint(seq.param_nonfinal(i), ==, is_nonfinal);
        }

        g_assert_cmpuint(seq.size_final(), ==, n_final_args);
}

static void
test_seq_csi_param(void)
{
        /* Tests that CSI parameters and subparameters are parsed correctly. */

        test_seq_csi_param("", { }, { });
        test_seq_csi_param(";", { -1, -1 }, { false, false });
        test_seq_csi_param(":", { -1, -1 }, { true, false });
        test_seq_csi_param(";:", { -1, -1, -1 }, { false, true, false });
        test_seq_csi_param("::;;", { -1, -1, -1, -1, -1 }, { true, true, false, false, false });

        test_seq_csi_param("1;2:3:4:5:6;7:8;9:0",
                           { 1, 2, 3, 4, 5, 6, 7, 8, 9, 0 },
                           { false, true, true, true, true, false, true, false, true, false });

        test_seq_csi_param("1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1",
                           { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
                           { false, false, false, false, false, false, false, false,
                                           false, false, false, false, false, false, false, false });

        test_seq_csi_param("1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1",
                           { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
                           { true, true, true, true, true, true, true, true,
                                           true, true, true, true, true, true, true, false });

}

static void
test_seq_csi_clear(void)
{
        /* Check that parameters are cleared from when a sequence was aborted. */

        vte_seq_builder b0{VTE_SEQ_CSI, 'm'};
        b0.set_param_intro(VTE_SEQ_PARAMETER_CHAR_WHAT);
        for (unsigned int i = 0; i < VTE_PARSER_ARG_MAX; ++i)
                b0.append_param(127 * i + 17);

        std::u32string str0;
        b0.to_string(str0);

        parser.reset();
        for (size_t len0 = 1; len0 <= str0.size(); ++len0) {
                for (unsigned int n_args = 0; n_args < VTE_PARSER_ARG_MAX; ++n_args) {
                        feed_parser(str0.substr(0, len0));

                        vte_seq_builder b1{VTE_SEQ_CSI, 'n'};
                        b1.set_param_intro(VTE_SEQ_PARAMETER_CHAR_GT);
                        for (unsigned int i = 0; i < n_args; ++i)
                                b1.append_param(257 * i + 31);

                        std::u32string str1;
                        b1.to_string(str1);

                        auto rv = feed_parser(str1);
                        g_assert_cmpint(rv, ==, VTE_SEQ_CSI);
                        b1.assert_equal_full(seq);
                        for (unsigned int n = seq.size(); n < VTE_PARSER_ARG_MAX; n++)
                                g_assert_true(seq.param_default(n));
                }
        }
}

static void
test_seq_csi_max(std::u32string const& start,
                 std::u32string const& more,
                 int expected_rv = VTE_SEQ_NONE)
{
        parser.reset();
        feed_parser(start);
        feed_parser(more);
        auto rv = feed_parser(U"m"s); /* final character */
        g_assert_cmpint(rv, ==, expected_rv);
}

static void
test_seq_csi_max(void)
{
        /* Check that an excessive number of parameters causes the
         * sequence to be ignored.
         *
         * Since SequenceBuilder is limited in the same number of
         * parameters as the parser, can't use it directly to
         * produce a sequence with too may parameters.
         */

        vte_seq_builder b{VTE_SEQ_CSI, 'm'};
        b.set_param_intro(VTE_SEQ_PARAMETER_CHAR_WHAT);
        for (unsigned int i = 0; i < VTE_PARSER_ARG_MAX; ++i)
                b.append_param(i);

        std::u32string str;
        b.to_string(str);

        /* The sequence with VTE_PARSER_ARG_MAX args must be parsed */
        auto rv = feed_parser(str);
        g_assert_cmpint(rv, ==, VTE_SEQ_CSI);

        /* Now test that adding one more parameter (whether with an
         * explicit value, or default, causes the sequence to be ignored.
         */
        str.pop_back(); /* erase final character */
        test_seq_csi_max(str, U":"s);
        test_seq_csi_max(str, U";"s);
        test_seq_csi_max(str, U":12345"s);
        test_seq_csi_max(str, U";12345"s);
        test_seq_csi_max(str, U":12345;"s);
        test_seq_csi_max(str, U";12345:"s);
        test_seq_csi_max(str, U":12345;"s);
        test_seq_csi_max(str, U":12345:"s);
}

static void
test_seq_glue_arg(char const* str,
                  unsigned int n_args,
                  unsigned int n_final_args)
{
        test_seq_parse(str);

        auto raw_seq = *seq.seq_ptr();
        g_assert_cmpuint(seq.size(), ==, n_args);
        g_assert_cmpuint(raw_seq->n_args, ==, n_args);
        g_assert_cmpuint(seq.size_final(), ==, n_final_args);
        g_assert_cmpuint(raw_seq->n_final_args, ==, n_final_args);

        g_assert_cmpuint(seq.type(), ==, raw_seq->type);
        g_assert_cmpuint(seq.command(), ==, raw_seq->command);
        g_assert_cmpuint(seq.terminator(), ==, raw_seq->terminator);

        for (unsigned int i = 0; i < raw_seq->n_args; i++)
                g_assert_cmpuint(seq.param(i), ==, vte_seq_arg_value(raw_seq->args[i]));
}

static void
test_seq_glue_arg(void)
{
        test_seq_glue_arg(":0:1000;2;3;4;:;", 9, 6);
        g_assert_cmpuint(seq.cbegin(), ==, 0);
        g_assert_cmpuint(seq.cend(), ==, 9);

        auto it = seq.cbegin();
        g_assert_cmpuint(it, ==, 0);
        it = seq.next(it);
        g_assert_cmpuint(it, ==,  3);
        it = seq.next(it);
        g_assert_cmpuint(it, ==, 4);
        it = seq.next(it);
        g_assert_cmpuint(it, ==, 5);
        it = seq.next(it);
        g_assert_cmpuint(it, ==, 6);
        it = seq.next(it);
        g_assert_cmpuint(it, ==, 8);
        it = seq.next(it);
        g_assert_cmpuint(it, ==, 9);

        it = seq.cbegin();
        g_assert_cmpint(seq.param(it++), ==, -1);
        g_assert_cmpint(seq.param(it++), ==, 0);
        g_assert_cmpint(seq.param(it++), ==, 1000);
        g_assert_cmpint(seq.param(it++), ==, 2);
        g_assert_cmpint(seq.param(it++), ==, 3);
        g_assert_cmpint(seq.param(it++), ==, 4);
        g_assert_cmpint(seq.param(it++), ==, -1);
        g_assert_cmpint(seq.param(it++), ==, -1);
        g_assert_cmpint(seq.param(it++), ==, -1);
        g_assert_cmpint(it, ==, seq.cend());

        it = seq.cbegin();
        g_assert_cmpint(seq.param(it, -2), ==, -2);
        g_assert_cmpint(seq.param(it, -2, 0, 100), ==, 0);
        it++; it++;
        g_assert_cmpint(seq.param(it, -2), ==, seq.param(it));
        g_assert_cmpint(seq.param(it, -2, 20, 100), ==, 100);
        g_assert_cmpint(seq.param(it, -2, 200, 2000), ==, 1000);
        g_assert_cmpint(seq.param(it, -2, 2000, 4000), ==, 2000);

        int a, b, c,d ;
        it = seq.cbegin();
        g_assert_false(seq.collect(it, {&a, &b, &c}));
        g_assert_true(seq.collect_subparams(it, {&a}));
        g_assert_true(seq.collect_subparams(it, {&a, &b}));
        g_assert_true(seq.collect_subparams(it, {&a, &b, &c}));
        g_assert_cmpint(a, ==, -1);
        g_assert_cmpint(b, ==, 0);
        g_assert_cmpint(c, ==, 1000);
        g_assert_false(seq.collect_subparams(it, {&a, &b, &c, &d}));

        it = seq.next(it);
        g_assert_true(seq.collect(it, {&a}));
        g_assert_true(seq.collect(it, {&a, &b}));
        g_assert_true(seq.collect(it, {&a, &b, &c}));
        g_assert_cmpint(a, ==, 2);
        g_assert_cmpint(b, ==, 3);
        g_assert_cmpint(c, ==, 4);
        g_assert_false(seq.collect(it, {&a, &b, &c, &d}));

        it = seq.next(it);
        it = seq.next(it);
        it = seq.next(it);
        g_assert_false(seq.collect(it, {&a}));
        g_assert_true(seq.collect_subparams(it, {&a}));
        g_assert_true(seq.collect_subparams(it, {&a, &b}));
        g_assert_cmpint(a, ==, -1);
        g_assert_cmpint(b, ==, -1);
        g_assert_false(seq.collect_subparams(it, {&a, &b, &c}));
        it = seq.next(it);
        g_assert_true(seq.collect(it, {&a}));
        g_assert_cmpint(a, ==, -1);
        g_assert_true(seq.collect(it, {&a, &b})); /* past-the-end params are final and default */
        g_assert_cmpint(a, ==, -1);
        g_assert_cmpint(b, ==, -1);
        g_assert_true(seq.collect(it, {&a, &b, &c})); /* past-the-end params are final and default */
        g_assert_cmpint(a, ==, -1);
        g_assert_cmpint(b, ==, -1);
        g_assert_cmpint(c, ==, -1);

        it = seq.cbegin();
        g_assert_cmpint(seq.collect1(it, -2), ==, -2);
        it = seq.next(it);
        g_assert_cmpint(seq.collect1(it), ==, 2);
        g_assert_cmpint(seq.collect1(it), ==, 2);
        it = seq.next(it);
        g_assert_cmpint(seq.collect1(it), ==, 3);
        it = seq.next(it);
        g_assert_cmpint(seq.collect1(it), ==, 4);
        it = seq.next(it);
        g_assert_cmpint(seq.collect1(it, -3), ==, -3);
        it = seq.next(it);
        g_assert_cmpint(seq.collect1(it), ==, -1);
        g_assert_cmpint(seq.collect1(it, 42), ==, 42);
        g_assert_cmpint(seq.collect1(it, -1, 0, 100), ==, 0);
        g_assert_cmpint(seq.collect1(it, 42, 0, 100), ==, 42);
        g_assert_cmpint(seq.collect1(it, 42, 0, 10), ==, 10);
        g_assert_cmpint(seq.collect1(it, 42, 100, 200), ==, 100);
}

static void
test_seq_glue_bignum(void)
{
        parser.reset();

        // Since this test a convenience function that operates
        // only on the vte_seq_arg_t's params, we can speed up
        // these tests by setting them directly instead of
        // building a string, parsing it and then testing the
        // params.
        auto raw_seq = *seq.seq_ptr();
        auto& n_args = raw_seq->n_args;
        auto& args = raw_seq->args;
        raw_seq->n_final_args = 1;

        auto test = [&](std::initializer_list<int> params,
                        bool ok = true) noexcept -> void
        {
                n_args = 0;
                for (auto p : params) {
                        args[n_args] = vte_seq_arg_init(p);
                        vte_seq_arg_finish(&args[n_args], (n_args + 1) < params.size());
                        ++n_args;
                }

                auto const idx = 0;
                auto v = seq.collect_number(idx);

                if (ok) {
                        g_assert_true(v);

                        auto expected_v = uint64_t{0};
                        for (auto ev : params) {
                                expected_v <<= 16;
                                expected_v += ev != -1 ? ev : 0;
                        }

                        g_assert_cmphex(*v, ==, expected_v);
                } else {
                        g_assert_false(v);
                }
        };

        test({}); // ""
        test({0}); // "0"
        test({11}); // "11"

        test({1, 0}); // "1:0"
        test({31, 0}); // "31:0"
        test({1, 65535}); // "1:65535"
        test({65535, 0}); // "65535:0"
        test({65535, 65535}); // "65535:65535"

        test({2, -1}); // "2:"
        test({3, -1, -1}); // "3::"
        test({5, -1, -1, -1}); // "5:::"
        test({1, -1, 1, -1}); // "1::1:"
        test({2, 3, 5, 7}); // "2:3:5:7"
        test({65535, 65535, 65535, 65535}); // "65535:65535:65535:65535", max

        test({1, -1, -1, -1, -1}, false); // "1::::", too many components
        test({-1, 1}, false); // ":1", leading default param
        test({0, 1}); // "0:1" // however this is ok
}

static void
test_seq_glue_uchar(void)
{
        parser.reset();

        // Since this test a convenience function that operates
        // only on the vte_seq_arg_t's params, we can speed up
        // these tests by setting them directly instead of
        // building a string, parsing it and then testing the
        // params.
        auto raw_seq = *seq.seq_ptr();
        auto& n_args = raw_seq->n_args;
        auto& args = raw_seq->args;
        raw_seq->n_final_args = 1;

        auto test_zero = [&](int32_t c,
                             int zero_v,
                             bool valid,
                             int default_v = 0x20) noexcept -> void
        {
                if (c == -1) {
                        n_args = 0;
                } else {
                        n_args = 1;
                        args[0] = vte_seq_arg_init(c);
                        vte_seq_arg_finish(&args[0]); // final
                }

                auto const rc = seq.collect_char(0, default_v, zero_v);
                if (valid) {
                        g_assert_true(rc);
                        g_assert_cmphex(*rc, ==, default_v);
                } else {
                        g_assert_false(rc);
                }
        };

        auto test = [&](uint32_t c,
                        bool valid = true) noexcept -> void
        {
                if (c < 0x10000u) {
                        n_args = 1;
                        args[0] = vte_seq_arg_init(c);
                        vte_seq_arg_finish(&args[0]); // final
                } else {
                        n_args = 2;
                        args[0] = vte_seq_arg_init(c >> 16);
                        vte_seq_arg_finish(&args[0], true); // nonfinal
                        args[1] = vte_seq_arg_init(c & 0xffffu);
                        vte_seq_arg_finish(&args[1]); // final
                }

                auto const rc = seq.collect_char(0);
                if (valid) {
                        g_assert_true(rc);
                        g_assert_cmphex(*rc, ==, c);
                } else {
                        g_assert_false(rc);
                }
        };

        auto test_surrogates = [&](char32_t c) noexcept -> void
        {
                auto const sc = c - 0x10000u;

                n_args = 2;
                args[0] = vte_seq_arg_init((sc >> 10) + 0xd800u);
                vte_seq_arg_finish(&args[0], true); // nonfinal
                args[1] = vte_seq_arg_init((sc & 0x3ffu) + 0xdc00u);
                vte_seq_arg_finish(&args[1]); // final

                auto const rc = seq.collect_char(0);
                g_assert_true(rc);
                g_assert_cmphex(*rc, ==, c);
        };

        test_zero(-1, -1, true); // default arg returns default value (0x20)
        test_zero(-1, -1, false, 0); // default arg but default value NUL is C0
        test_zero(0, -1, true); // zero arg treated as default returns default value (0x20)
        test_zero(0, -1, false, 0); // zero arg treated as default fails because NUL is C0
        test_zero(0, 0, false); // zero arg treated as zero fails because NUL is C0
        test_zero(0, 0x20, true); // zero arg treated as default value (0x20)
        for (auto c = 1u; c < 0x20u; ++c)
                test(c, false); // C0
        for (auto c = 0x20u; c < 0x7fu; ++c)
                test(c);
        for (auto c = 0x7fu; c < 0xa0u; ++c)
                test(c, false); // C1
        for (auto c = 0xa0u; c < 0xd800u; ++c)
                test(c);
        for (auto c = 0xd800; c < 0xe000; ++c)
                test(c, false); // surrogate
        for (auto c = 0xe000; c < 0x10000; ++c)
                test(c);

        for (auto c = 0x10000u; c < 0x110000u; ++c) {
                test(c);
                test_surrogates(c);
        }

        test(0x110000, false);

        // Test default value
        {
                n_args = 1;
                args[0] = vte_seq_arg_init(-1);
                vte_seq_arg_finish(&args[0]); // final

                auto const rc = seq.collect_char(0);
                g_assert_true(rc);
                g_assert_cmphex(*rc, ==, 0x20); // ' '
        }
}

static int
feed_parser_st(vte_seq_builder& b,
               bool c1 = false,
               ssize_t max_arg_str_len = -1,
               u32SequenceBuilder::Introducer introducer = u32SequenceBuilder::Introducer::DEFAULT,
               u32SequenceBuilder::ST st = u32SequenceBuilder::ST::DEFAULT)
{
        std::u32string s;
        b.to_string(s, c1, max_arg_str_len, introducer, st);

        auto rv = feed_parser(s);
        if (rv != VTE_SEQ_OSC)
                return rv;

        switch (st) {
        case u32SequenceBuilder::ST::NONE:
                g_assert_cmpuint(seq.st(), ==, 0);
                break;
        case u32SequenceBuilder::ST::DEFAULT:
                g_assert_cmpuint(seq.st(), ==, c1 ? 0x9c /* ST */ : 0x5c /* BACKSLASH */);
                break;
        case u32SequenceBuilder::ST::C0:
                g_assert_cmpuint(seq.st(), ==, 0x5c /* BACKSLASH */);
                break;
        case u32SequenceBuilder::ST::C1:
                g_assert_cmpuint(seq.st(), ==, 0x9c /* ST */);
                break;
        case u32SequenceBuilder::ST::BEL:
                g_assert_cmpuint(seq.st(), ==, 0x7 /* BEL */);
                break;
        }

        return rv;
}

static void
test_seq_osc(std::u32string const& str,
             int expected_rv = VTE_SEQ_OSC,
             bool c1 = true,
             ssize_t max_arg_str_len = -1,
             u32SequenceBuilder::Introducer introducer = u32SequenceBuilder::Introducer::DEFAULT,
             u32SequenceBuilder::ST st = u32SequenceBuilder::ST::DEFAULT)
{
        vte_seq_builder b{VTE_SEQ_OSC, str};

        parser.reset();
        auto rv = feed_parser_st(b, c1, max_arg_str_len, introducer, st);
        g_assert_cmpint(rv, ==, expected_rv);
        #if 0
        if (rv != VTE_SEQ_NONE)
                b.assert_equal(seq);
        #endif

        if (expected_rv != VTE_SEQ_OSC)
                return;

        if (max_arg_str_len < 0 || size_t(max_arg_str_len) == str.size())
                g_assert_true(seq.string() == str);
        else
                g_assert_true(seq.string() == str.substr(0, max_arg_str_len));
}

static int
controls_match(bool c1,
               u32SequenceBuilder::Introducer introducer,
               u32SequenceBuilder::ST st,
               bool allow_bel,
               int expected_rv)
{
        if (introducer == u32SequenceBuilder::Introducer::DEFAULT)
                introducer = c1 ? u32SequenceBuilder::Introducer::C1 : u32SequenceBuilder::Introducer::C0;
        if (st == u32SequenceBuilder::ST::DEFAULT)
                st = c1 ? u32SequenceBuilder::ST::C1 : u32SequenceBuilder::ST::C0;
        if ((introducer == u32SequenceBuilder::Introducer::C0 &&
             (st == u32SequenceBuilder::ST::C0 || (allow_bel && st == u32SequenceBuilder::ST::BEL))) ||
            (introducer == u32SequenceBuilder::Introducer::C1 &&
             st == u32SequenceBuilder::ST::C1))
                return expected_rv;
        return VTE_SEQ_IGNORE;
}

static void
test_seq_osc(void)
{
        /* Simple */
        test_seq_osc(U""s);
        test_seq_osc(U"TEST"s);

        /* String of any supported length */
        for (unsigned int len = 0; len < VTE_SEQ_STRING_MAX_CAPACITY; ++len)
                test_seq_osc(std::u32string(len, 0x10000+len));

        /* Length exceeded */
        test_seq_osc(std::u32string(VTE_SEQ_STRING_MAX_CAPACITY + 1, 0x100000), VTE_SEQ_IGNORE);

        /* Test all introducer/ST combinations */
        for (auto introducer : { u32SequenceBuilder::Introducer::DEFAULT,
                                u32SequenceBuilder::Introducer::C0,
                                u32SequenceBuilder::Introducer::C1 }) {
                for (auto st : {u32SequenceBuilder::ST::DEFAULT,
                                        u32SequenceBuilder::ST::C0,
                                        u32SequenceBuilder::ST::C1,
                                        u32SequenceBuilder::ST::BEL }) {
                        for (auto c1 : { false, true }) {
                                int expected_rv = controls_match(c1, introducer, st, true, VTE_SEQ_OSC);
                                test_seq_osc(U"TEST"s, expected_rv, c1, -1, introducer, st);
                        }
                }
        }
}

static void
test_seq_glue_string(void)
{
        std::u32string str{U"TEST"s};
        test_seq_osc(str);

        g_assert_true(seq.string() == str);
}

static void
test_seq_glue_string_tokeniser(void)
{
        std::string str{"a;1b:17:test::b:;3;5;def;17 a;ghi;"s};

        StringTokeniser tokeniser{str, ';'};

        auto start = tokeniser.cbegin();
        auto end = tokeniser.cend();

        auto pit = start;
        for (auto it : {"a"s, "1b:17:test::b:"s, "3"s, "5"s, "def"s, "17 a"s, "ghi"s, ""s}) {
                g_assert_true(it == *pit);

                /* Use std::find to see if the InputIterator implementation
                 * is complete and correct.
                 */
                auto fit = std::find(start, end, it);
                g_assert_true(fit == pit);

                ++pit;
        }
        g_assert_true(pit == end);

        auto len = str.size();
        size_t pos = 0;
        pit = start;
        for (auto it : {1, 14, 1, 1, 3, 4, 3, 0}) {
                g_assert_cmpuint(it, ==, pit.size());
                g_assert_cmpuint(len, ==, pit.size_remaining());

                g_assert_true(pit.string_remaining() == str.substr(pos, std::string::npos));

                len -= it + 1;
                pos += it + 1;

                ++pit;
        }
        g_assert_cmpuint(len + 1, ==, 0);
        g_assert_cmpuint(pos, ==, str.size() + 1);

        pit = start;
        for (auto it : {-2, -2, 3, 5, -2, -2, -2, -1}) {
                int num;
                bool v = pit.number(num);
                if (it == -2)
                        g_assert_false(v);
                else
                        g_assert_cmpint(it, ==, num);

                ++pit;
        }

        /* Test range for */
        for (auto it : tokeniser)
                ;

        /* Test different separator */
        pit = start;
        ++pit;

        auto substr = *pit;
        StringTokeniser subtokeniser{substr, ':'};

        auto subpit = subtokeniser.cbegin();
        for (auto it : {"1b"s, "17"s, "test"s, ""s, "b"s, ""s}) {
                g_assert_true(it == *subpit);

                ++subpit;
        }
        g_assert_true(subpit == subtokeniser.cend());

        /* Test another string, one that doesn't end with an empty token */
        std::string str2{"abc;defghi"s};
        StringTokeniser tokeniser2{str2, ';'};

        g_assert_cmpint(std::distance(tokeniser2.cbegin(), tokeniser2.cend()), ==, 2);
        auto pit2 = tokeniser2.cbegin();
        g_assert_true(*pit2 == "abc"s);
        ++pit2;
        g_assert_true(*pit2 == "defghi"s);
        ++pit2;
        g_assert_true(pit2 == tokeniser2.cend());

        /* Test another string, one that starts with an empty token */
        std::string str3{";abc"s};
        StringTokeniser tokeniser3{str3, ';'};

        g_assert_cmpint(std::distance(tokeniser3.cbegin(), tokeniser3.cend()), ==, 2);
        auto pit3 = tokeniser3.cbegin();
        g_assert_true(*pit3 == ""s);
        ++pit3;
        g_assert_true(*pit3 == "abc"s);
        ++pit3;
        g_assert_true(pit3 == tokeniser3.cend());

        /* And try an empty string, which should split into one empty token */
        std::string str4{""s};
        StringTokeniser tokeniser4{str4, ';'};

        g_assert_cmpint(std::distance(tokeniser4.cbegin(), tokeniser4.cend()), ==, 1);
        auto pit4 = tokeniser4.cbegin();
        g_assert_true(*pit4 == ""s);
        ++pit4;
        g_assert_true(pit4 == tokeniser4.cend());
}

static void
test_seq_glue_sequence_builder(void)
{
        /* This is sufficiently tested by being used in all the other tests,
         * but if there's anything remaining to be tested, do it here.
         */

        vte_seq_builder b{VTE_SEQ_CSI, 'm'};
        b.append_param(-1);
        b.append_param(1);
        b.append_param(-1);
        b.append_params({2, -2, -1, 3});
        b.append_subparams({4, -1, -2, 5, -1, 6});
        b.append_param(7);
        b.append_param(-1);
        b.append_param(8);

        auto str = std::u32string{};
        b.to_string(str);

        g_assert_true(str == U"\e[;1;;2;;3;4::5::6;7;;8m"s);

}

static void
test_seq_glue_reply_builder(void)
{
        /* Nothing to test here; ReplyBuilder is just a constructor for
         * SequenceBuilder.
         */
}

int
main(int argc,
     char* argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/parser/sequences/arg", test_seq_arg);
        g_test_add_func("/vte/parser/sequences/string", test_seq_string);
        g_test_add_func("/vte/parser/sequences/glue/arg", test_seq_glue_arg);
        g_test_add_func("/vte/parser/sequences/glue/bignum", test_seq_glue_bignum);
        g_test_add_func("/vte/parser/sequences/glue/uchar", test_seq_glue_uchar);
        g_test_add_func("/vte/parser/sequences/glue/string", test_seq_glue_string);
        g_test_add_func("/vte/parser/sequences/glue/string-tokeniser", test_seq_glue_string_tokeniser);
        g_test_add_func("/vte/parser/sequences/glue/sequence-builder", test_seq_glue_sequence_builder);
        g_test_add_func("/vte/parser/sequences/glue/reply-builder", test_seq_glue_reply_builder);
        g_test_add_func("/vte/parser/sequences/control", test_seq_control);
        g_test_add_func("/vte/parser/sequences/escape/invalid", test_seq_esc_invalid);
        g_test_add_func("/vte/parser/sequences/escape/charset/94", test_seq_esc_charset_94);
        g_test_add_func("/vte/parser/sequences/escape/charset/96", test_seq_esc_charset_96);
        g_test_add_func("/vte/parser/sequences/escape/charset/94^n", test_seq_esc_charset_94_n);
        g_test_add_func("/vte/parser/sequences/escape/charset/96^n", test_seq_esc_charset_96_n);
        g_test_add_func("/vte/parser/sequences/escape/charset/control", test_seq_esc_charset_control);
        g_test_add_func("/vte/parser/sequences/escape/charset/other", test_seq_esc_charset_other);
        g_test_add_func("/vte/parser/sequences/escape/nF", test_seq_esc_nF);
        g_test_add_func("/vte/parser/sequences/escape/F[pes]", test_seq_esc_Fpes);
        g_test_add_func("/vte/parser/sequences/escape/known", test_seq_esc_known);
        g_test_add_func("/vte/parser/sequences/csi", test_seq_csi);
        g_test_add_func("/vte/parser/sequences/csi/known", test_seq_csi_known);
        g_test_add_func("/vte/parser/sequences/csi/parameters", test_seq_csi_param);
        g_test_add_func("/vte/parser/sequences/csi/clear", test_seq_csi_clear);
        g_test_add_func("/vte/parser/sequences/csi/max", test_seq_csi_max);
        g_test_add_func("/vte/parser/sequences/sci", test_seq_sci);
        g_test_add_func("/vte/parser/sequences/sci/known", test_seq_sci_known);
        g_test_add_func("/vte/parser/sequences/dcs", test_seq_dcs);
        g_test_add_func("/vte/parser/sequences/dcs/known", test_seq_dcs_known);
        g_test_add_func("/vte/parser/sequences/osc", test_seq_osc);

        return g_test_run();
}
