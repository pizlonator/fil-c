/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
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

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "parser.hh"

#include "parser-charset-tables.hh"

namespace vte {
namespace parser {

uint32_t
Parser::parse_charset_94(uint32_t raw,
                         unsigned int intermediates) noexcept
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_NONE:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94)))
                        return charset_graphic_94[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_SPACE:
                return VTE_CHARSET_DRCS;

        case VTE_SEQ_INTERMEDIATE_BANG:
                if (remaining_intermediates == 0 &&
                    raw >= 0x40 && (raw < 0x40 + G_N_ELEMENTS(charset_graphic_94_with_2_1)))
                        return charset_graphic_94_with_2_1[raw - 0x40];
                break;

        case VTE_SEQ_INTERMEDIATE_DQUOTE:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_with_2_2)))
                        return charset_graphic_94_with_2_2[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_HASH:
        case VTE_SEQ_INTERMEDIATE_CASH:
                break;

        case VTE_SEQ_INTERMEDIATE_PERCENT:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_with_2_5)))
                        return charset_graphic_94_with_2_5[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_AND:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_with_2_6)))
                        return charset_graphic_94_with_2_6[raw - 0x30];
                break;
        }

        return charset_empty_or_none(raw);
}

uint32_t
Parser::parse_charset_94_n(uint32_t raw,
                           unsigned int intermediates) noexcept
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_NONE:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_n)))
                        return charset_graphic_94_n[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_SPACE:
                return VTE_CHARSET_DRCS;

        case VTE_SEQ_INTERMEDIATE_BANG:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_n_with_2_1)))
                        return charset_graphic_94_n_with_2_1[raw - 0x30];
                break;
        }

        return charset_empty_or_none(raw);
}

uint32_t
Parser::parse_charset_96(uint32_t raw,
                         unsigned int intermediates) noexcept
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_NONE:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_96)))
                        return charset_graphic_96[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_SPACE:
                return VTE_CHARSET_DRCS;
        }

        return charset_empty_or_none(raw);
}

uint32_t
Parser::parse_charset_96_n(uint32_t raw,
                           unsigned int intermediates) noexcept
{
        if (VTE_SEQ_INTERMEDIATE(intermediates) == VTE_SEQ_INTERMEDIATE_SPACE)
                return VTE_CHARSET_DRCS;

        return charset_empty_or_none(raw);
}

uint32_t
Parser::parse_charset_ocs(uint32_t raw,
                          unsigned int intermediates) noexcept
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_NONE:  /* OCS with standard return */
                if (remaining_intermediates == 0 &&
                    raw >= 0x30 && raw < (0x30 + G_N_ELEMENTS(charset_ocs)))
                        return charset_ocs[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_SPACE: /* OCS with standard return */
                if (remaining_intermediates == 0 &&
                    raw >= 0x30 && raw < (0x30 + G_N_ELEMENTS(charset_ocs_with_2_0)))
                        return charset_ocs_with_2_0[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_BANG ... VTE_SEQ_INTERMEDIATE_DOT: /* OCS with standard return */
                break;

        case VTE_SEQ_INTERMEDIATE_SLASH: /* OCS without standard return */
                if (remaining_intermediates == 0 &&
                    raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_ocs_with_2_15)))
                        return charset_ocs_with_2_15[raw - 0x40];
                break;
        }

        return VTE_CHARSET_NONE;
}

uint32_t
Parser::parse_charset_control(uint32_t raw,
                              unsigned int intermediates) noexcept
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_BANG: /* C0 controls */
                if (remaining_intermediates == 0 &&
                    raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_control_c0)))
                        return charset_control_c0[raw - 0x40];
                break;

        case VTE_SEQ_INTERMEDIATE_DQUOTE: /* C1 controls */
                if (remaining_intermediates == 0 &&
                    raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_control_c1)))
                        return charset_control_c1[raw - 0x40];
                break;
        }

        return charset_empty_or_none(raw);
}

uint32_t
Parser::parse_host_escape(vte_seq_t const* seq,
                          unsigned int *cs_out) noexcept
{
        unsigned int intermediates = seq->intermediates;
        unsigned int intermediate0 = VTE_SEQ_INTERMEDIATE(intermediates);

        /* Switch on the first intermediate */
        switch (intermediate0) {
        case VTE_SEQ_INTERMEDIATE_NONE:
        case VTE_SEQ_INTERMEDIATE_HASH: {  /* Single control functions */
                switch (_VTE_SEQ_CODE_ESC(seq->terminator, intermediates)) {
#define _VTE_SEQ(cmd,type,f,p,ni,i,flags) \
                        case _VTE_SEQ_CODE_ESC(f, VTE_SEQ_INTERMEDIATE_##i): return VTE_CMD_##cmd;
#include "parser-esc.hh"
#undef _VTE_SEQ
                default: return VTE_CMD_NONE;
                }
                break;
        }

        case VTE_SEQ_INTERMEDIATE_SPACE:   /* Announce code structure */
                if (VTE_SEQ_REMOVE_INTERMEDIATE(intermediates) == 0)
                        return VTE_CMD_ACS;
                break;

        case VTE_SEQ_INTERMEDIATE_BANG:    /* C0-designate */
        case VTE_SEQ_INTERMEDIATE_DQUOTE:  /* C1-designate */
                *cs_out = VTE_MAKE_CHARSET(parse_charset_control(seq->terminator, intermediates),
                                           intermediate0 - VTE_SEQ_INTERMEDIATE_BANG);
                return VTE_CMD_CnD;

        case VTE_SEQ_INTERMEDIATE_CASH: {  /* Designate multi-byte character sets */
                unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);
                unsigned int intermediate1 = VTE_SEQ_INTERMEDIATE(remaining_intermediates);
                remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(remaining_intermediates);

                /* Check the 2nd intermediate */
                switch (intermediate1) {
                case VTE_SEQ_INTERMEDIATE_NONE:
                        /* For compatibility with an earlier version of ISO-2022,
                         * ESC 2/4 4/0, ESC 2/4 4/1 and ESC 2/4 4/2 designate G0
                         * sets (i.e., without the 2/8 as 2nd intermediate byte).
                         */
                        switch (seq->terminator) {
                        case '@':
                        case 'A':
                        case 'B': /* G0-designate multibyte charset */
                                *cs_out = VTE_MAKE_CHARSET(parse_charset_94_n(seq->terminator, remaining_intermediates),
                                                           0);
                                return VTE_CMD_GnDMm;
                        }
                        break;

                case VTE_SEQ_INTERMEDIATE_POPEN:  /* G0-designate 94^n-set */
                case VTE_SEQ_INTERMEDIATE_PCLOSE: /* G1-designate 94^n-set */
                case VTE_SEQ_INTERMEDIATE_MULT:   /* G2-designate 94^n-set */
                case VTE_SEQ_INTERMEDIATE_PLUS:   /* G3-designate 94^n-set */
                        *cs_out = VTE_MAKE_CHARSET(parse_charset_94_n(seq->terminator, remaining_intermediates),
                                                   intermediate1 - VTE_SEQ_INTERMEDIATE_POPEN);
                        return VTE_CMD_GnDMm;

                case VTE_SEQ_INTERMEDIATE_COMMA:  /* Reserved for future standardisation */
                        break;

                case VTE_SEQ_INTERMEDIATE_MINUS:  /* G1-designate 96^n-set */
                case VTE_SEQ_INTERMEDIATE_DOT:    /* G2-designate 96^n-set */
                case VTE_SEQ_INTERMEDIATE_SLASH:  /* G3-designate 96^n-set */
                        *cs_out = VTE_MAKE_CHARSET(parse_charset_96_n(seq->terminator, remaining_intermediates),
                                                   intermediate1 - VTE_SEQ_INTERMEDIATE_COMMA);
                        return VTE_CMD_GnDMm;
                }
                break;
        }

        case VTE_SEQ_INTERMEDIATE_PERCENT: /* Designate other coding system */
                *cs_out = parse_charset_ocs(seq->terminator, VTE_SEQ_REMOVE_INTERMEDIATE(intermediates));
                return VTE_CMD_DOCS;

        case VTE_SEQ_INTERMEDIATE_AND:     /* Identify revised registration */
                if (VTE_SEQ_REMOVE_INTERMEDIATE(intermediates) == 0)
                        return VTE_CMD_IRR;
                break;

        case VTE_SEQ_INTERMEDIATE_SQUOTE:  /* Reserved for future standardisation */
                break;

        case VTE_SEQ_INTERMEDIATE_POPEN:   /* G0-designate 94-set */
        case VTE_SEQ_INTERMEDIATE_PCLOSE:  /* G1-designate 94-set */
        case VTE_SEQ_INTERMEDIATE_MULT:    /* G2-designate 94-set */
        case VTE_SEQ_INTERMEDIATE_PLUS:    /* G3-designate 94-set */
                *cs_out = VTE_MAKE_CHARSET(parse_charset_94(seq->terminator,
                                                            VTE_SEQ_REMOVE_INTERMEDIATE(intermediates)),
                                           intermediate0 - VTE_SEQ_INTERMEDIATE_POPEN);
                return VTE_CMD_GnDm;

        case VTE_SEQ_INTERMEDIATE_COMMA:   /* Reserved for future standardisation */
                break;

        case VTE_SEQ_INTERMEDIATE_MINUS:   /* G1-designate 96-set */
        case VTE_SEQ_INTERMEDIATE_DOT:     /* G2-designate 96-set */
        case VTE_SEQ_INTERMEDIATE_SLASH:   /* G3-designate 96-set */
                *cs_out = VTE_MAKE_CHARSET(parse_charset_96(seq->terminator,
                                                            VTE_SEQ_REMOVE_INTERMEDIATE(intermediates)),
                                           intermediate0 - VTE_SEQ_INTERMEDIATE_COMMA);
                return VTE_CMD_GnDm;
        }

        return VTE_CMD_NONE;
}

uint32_t
Parser::parse_host_csi(vte_seq_t const* seq) noexcept
{
        switch (_VTE_SEQ_CODE(seq->terminator, seq->intermediates)) {
#define _VTE_SEQ(cmd,type,f,p,ni,i,flags) \
                case _VTE_SEQ_CODE(f, _VTE_SEQ_CODE_COMBINE(VTE_SEQ_PARAMETER_##p, VTE_SEQ_INTERMEDIATE_##i)): return VTE_CMD_##cmd;
#include "parser-csi.hh"
#undef _VTE_SEQ
                default: return VTE_CMD_NONE;
        }
}

uint32_t
Parser::parse_host_control(vte_seq_t const* seq) noexcept
{
        switch (seq->terminator) {
#define _VTE_SEQ(cmd,type,f,pi,ni,i0,flags) case f: return VTE_CMD_##cmd;
#include "parser-c01.hh"
#undef _VTE_SEQ
                default: return VTE_CMD_NONE;
        }
}

uint32_t
Parser::parse_host_dcs(vte_seq_t const* seq, unsigned int* flagsptr) noexcept
{
        switch (_VTE_SEQ_CODE(seq->terminator, seq->intermediates)) {
#define _VTE_SEQ(cmd,type,f,p,ni,i,flags) \
                case _VTE_SEQ_CODE(f, _VTE_SEQ_CODE_COMBINE(VTE_SEQ_PARAMETER_##p, VTE_SEQ_INTERMEDIATE_##i)): *flagsptr = flags; return VTE_CMD_##cmd;
#include "parser-dcs.hh"
#undef _VTE_SEQ
                default: return VTE_CMD_NONE;
        }
}

uint32_t
Parser::parse_host_sci(vte_seq_t const* seq) noexcept
{
        switch (_VTE_SEQ_CODE(seq->terminator, 0)) {
#define _VTE_SEQ(cmd,type,f,p,ni,i,flags) \
        case _VTE_SEQ_CODE(f, 0): return VTE_CMD_##cmd;
#include "parser-sci.hh"
#undef _VTE_SEQ
        default: return VTE_CMD_NONE;
        }
}

} // namespace parser
} // namespace vte
