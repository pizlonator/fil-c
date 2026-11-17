/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
 * Copyright © 2018 Christian Persch
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

#include <cstdint>
#include <cstdio>
#include <optional>

#include "debug.h"

#include "parser-arg.hh"
#include "parser-string.hh"

struct vte_seq_t;

/*
 * Parsers
 * The vte_parser object parses control-sequences for both host and terminal
 * side. Based on this parser, there is a set of command-parsers that take a
 * vte_seq sequence and returns the command it represents. This is different
 * for host and terminal side, and so far we only provide the terminal side, as
 * host side is not used by anyone.
 */

#define VTE_PARSER_ARG_MAX (32)

enum {
        VTE_SEQ_NONE,        /* placeholder, no sequence parsed */

        VTE_SEQ_IGNORE,      /* no-op character */
        VTE_SEQ_GRAPHIC,     /* graphic character */
        VTE_SEQ_CONTROL,     /* control character */
        VTE_SEQ_ESCAPE,      /* escape sequence */
        VTE_SEQ_CSI,         /* control sequence function */
        VTE_SEQ_DCS,         /* device control string */
        VTE_SEQ_OSC,         /* operating system control */
        VTE_SEQ_SCI,         /* single character control function */
        VTE_SEQ_APC,         /* application program command */
        VTE_SEQ_PM,          /* privacy message */
        VTE_SEQ_SOS,         /* start of string */

        VTE_SEQ_N,
};

enum {
        VTE_SEQ_INTERMEDIATE_CHAR_NONE    = 0,

        VTE_SEQ_INTERMEDIATE_CHAR_SPACE   = ' ',  /* 02/00 */
        VTE_SEQ_INTERMEDIATE_CHAR_BANG    = '!',  /* 02/01 */
        VTE_SEQ_INTERMEDIATE_CHAR_DQUOTE  = '"',  /* 02/02 */
        VTE_SEQ_INTERMEDIATE_CHAR_HASH    = '#',  /* 02/03 */
        VTE_SEQ_INTERMEDIATE_CHAR_CASH    = '$',  /* 02/04 */
        VTE_SEQ_INTERMEDIATE_CHAR_PERCENT = '%',  /* 02/05 */
        VTE_SEQ_INTERMEDIATE_CHAR_AND     = '&',  /* 02/06 */
        VTE_SEQ_INTERMEDIATE_CHAR_SQUOTE  = '\'', /* 02/07 */
        VTE_SEQ_INTERMEDIATE_CHAR_POPEN   = '(',  /* 02/08 */
        VTE_SEQ_INTERMEDIATE_CHAR_PCLOSE  = ')',  /* 02/09 */
        VTE_SEQ_INTERMEDIATE_CHAR_MULT    = '*',  /* 02/10 */
        VTE_SEQ_INTERMEDIATE_CHAR_PLUS    = '+',  /* 02/11 */
        VTE_SEQ_INTERMEDIATE_CHAR_COMMA   = ',',  /* 02/12 */
        VTE_SEQ_INTERMEDIATE_CHAR_MINUS   = '-',  /* 02/13 */
        VTE_SEQ_INTERMEDIATE_CHAR_DOT     = '.',  /* 02/14 */
        VTE_SEQ_INTERMEDIATE_CHAR_SLASH   = '/',  /* 02/15 */
};

enum {
        VTE_SEQ_PARAMETER_CHAR_NONE  = 0,

        /* Numbers; not used         *  03/00..03/09 */
        /* COLON is reserved         = ':'   * 03/10 */
        /* SEMICOLON is reserved     = ';'   * 03/11 */
        VTE_SEQ_PARAMETER_CHAR_LT    = '<', /* 03/12 */
        VTE_SEQ_PARAMETER_CHAR_EQUAL = '=', /* 03/13 */
        VTE_SEQ_PARAMETER_CHAR_GT    = '>', /* 03/14 */
        VTE_SEQ_PARAMETER_CHAR_WHAT  = '?'  /* 03/15 */
};

#define VTE_SEQ_MAKE_INTERMEDIATE(c) ((c) - ' ' + 1)

enum {
        VTE_SEQ_INTERMEDIATE_NONE      = 0,

        VTE_SEQ_INTERMEDIATE_SPACE     = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_SPACE  ),
        VTE_SEQ_INTERMEDIATE_BANG      = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_BANG   ),
        VTE_SEQ_INTERMEDIATE_DQUOTE    = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_DQUOTE ),
        VTE_SEQ_INTERMEDIATE_HASH      = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_HASH   ),
        VTE_SEQ_INTERMEDIATE_CASH      = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_CASH   ),
        VTE_SEQ_INTERMEDIATE_PERCENT   = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_PERCENT),
        VTE_SEQ_INTERMEDIATE_AND       = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_AND    ),
        VTE_SEQ_INTERMEDIATE_SQUOTE    = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_SQUOTE ),
        VTE_SEQ_INTERMEDIATE_POPEN     = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_POPEN  ),
        VTE_SEQ_INTERMEDIATE_PCLOSE    = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_PCLOSE ),
        VTE_SEQ_INTERMEDIATE_MULT      = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_MULT   ),
        VTE_SEQ_INTERMEDIATE_PLUS      = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_PLUS   ),
        VTE_SEQ_INTERMEDIATE_COMMA     = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_COMMA  ),
        VTE_SEQ_INTERMEDIATE_MINUS     = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_MINUS  ),
        VTE_SEQ_INTERMEDIATE_DOT       = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_DOT    ),
        VTE_SEQ_INTERMEDIATE_SLASH     = VTE_SEQ_MAKE_INTERMEDIATE(VTE_SEQ_INTERMEDIATE_CHAR_SLASH  ),
};

#define VTE_SEQ_MAKE_PARAMETER(c) ('?' - (c) + 1)

enum {
        VTE_SEQ_PARAMETER_NONE  = 0,

        VTE_SEQ_PARAMETER_LT    = VTE_SEQ_MAKE_PARAMETER(VTE_SEQ_PARAMETER_CHAR_LT   ),
        VTE_SEQ_PARAMETER_EQUAL = VTE_SEQ_MAKE_PARAMETER(VTE_SEQ_PARAMETER_CHAR_EQUAL),
        VTE_SEQ_PARAMETER_GT    = VTE_SEQ_MAKE_PARAMETER(VTE_SEQ_PARAMETER_CHAR_GT   ),
        VTE_SEQ_PARAMETER_WHAT  = VTE_SEQ_MAKE_PARAMETER(VTE_SEQ_PARAMETER_CHAR_WHAT ),
};

enum {
#define _VTE_CMD(cmd) VTE_CMD_##cmd,
#define _VTE_NOP(cmd) VTE_CMD_##cmd,
#include "parser-cmd.hh"
#undef _VTE_CMD
#undef _VTE_NOP

        VTE_CMD_N,
        VTE_CMD_NOP_FIRST = VTE_CMD_ACK
};

enum {
#define _VTE_REPLY(cmd,type,final,pintro,intermediate,code) VTE_REPLY_##cmd,
#include "parser-reply.hh"
#undef _VTE_REPLY

        VTE_REPLY_N
};

enum {
#define _VTE_CHARSET_PASTE(name) VTE_CHARSET_##name,
#define _VTE_CHARSET(name) _VTE_CHARSET_PASTE(name)
#define _VTE_CHARSET_ALIAS_PASTE(name1,name2) VTE_CHARSET_##name1 = VTE_CHARSET_##name2,
#define _VTE_CHARSET_ALIAS(name1,name2) _VTE_CHARSET_ALIAS_PASTE(name1,name2)
#include "parser-charset.hh"
#undef _VTE_CHARSET_PASTE
#undef _VTE_CHARSET
#undef _VTE_CHARSET_ALIAS_PASTE
#undef _VTE_CHARSET_ALIAS
};

enum {
#define _VTE_OSC(osc,value) VTE_OSC_##osc = value,
#include "parser-osc.hh"
#undef _VTE_OSC

        VTE_OSC_N
};

enum {
#define _VTE_SGR(name, value) VTE_SGR_##name = value,
#define _VTE_NGR(...)
#include "parser-sgr.hh"
#undef _VTE_SGR
#undef _VTE_NGR
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-declarations"
enum {
#define _VTE_SGR(name, value) VTE_DECSGR_##name = value,
#define _VTE_NGR(...)
#include "parser-decsgr.hh"
#undef _VTE_SGR
#undef _VTE_NGR
};
#pragma GCC diagnostic pop

#define VTE_CHARSET_CHARSET_MASK   ((1U << 16) - 1U)
#define VTE_CHARSET_SLOT_OFFSET    (16)
#define VTE_CHARSET_GET_CHARSET(c) ((c) & VTE_CHARSET_CHARSET_MASK)
#define VTE_CHARSET_GET_SLOT(c)    ((c) >> VTE_CHARSET_SLOT_OFFSET)

enum {
      VTE_DISPATCH_UNRIPE = 1u << 0,
};

struct vte_seq_t {
        unsigned int type;
        unsigned int command;
        uint32_t terminator;
        unsigned int intermediates;
        unsigned int n_intermediates;
        unsigned int charset;
        unsigned int n_args;
        unsigned int n_final_args;
        vte_seq_arg_t args[VTE_PARSER_ARG_MAX];
        vte_seq_string_t arg_str;
        uint32_t introducer;
        uint32_t st;
};

#ifdef PARSER_INCLUDE_NOP
# define _VTE_NOQ(...) _VTE_SEQ(__VA_ARGS__)
#else
# define _VTE_NOQ(...)
#endif

/*
 * Terminal Parser
 * This file contains a bunch of UTF-8 helpers and the main ctlseq-parser. The
 * parser is a simple state-machine that correctly parses all CSI, DCS, OSC, ST
 * control sequences and generic escape sequences.
 * The parser itself does not perform any actions but lets the caller react to
 * detected sequences.
 *
 * This parser is mostly DEC VT100+ compatible; known differences are:
 *
 * * DEC only recognises up to 16 parameters; vte up to 32 (and that can be easily
 *   extended)
 *
 * * DEC's parameter values range is 0..16384; vte supports 0..65535 (16-bit range).
 *
 * * When the number of parameter exceeds that number, DEC executes the function
 *   with these parameters, ignoring the excessive parameters; vte ignores the
 *   whole function instead.
 *
 * * DEC ignores CSI sequences with colon-separated parameters; vte implements colon-
 *   separated parameters as subparameters (this is an extension taken from ITU-T T.416).
 *
 * * DEC executes format effector controls in CSI, OSC, DCS sequences as if the
 *   control was received before the control sequence; vte only does this for CSI
 *   sequences and ignores all controls except ESC and BEL in OSC control strings,
 *   and passes all controls except ESC through to the control string in DCS sequences.
 *
 * * DEC only allows ST (either C0 or C1) to terminate OSC strings; vte allows
 *   OSC to be terminated by BEL (this is a deprecated xterm extension).
 *
 * * DEC parses ESC Z as DECID, a deprecated function equivalent to DA1; vte
 *   implements ECMA-48's SCI (single character introducer) instead.
 */

/*
 * Command Parser
 * The ctl-seq parser "vte::parser::Parser" only detects whole sequences, it does
 * not detect the specific command. Once a sequence is parsed, the command-parsers
 * are used to figure out their meaning.
 */

/*
 * Intermediates (and, for CSI/DCS, the optional parameter character) are
 * stored efficiently in an unsigned int. Intermediates can be 2/00..2/15,
 * plus one value for 'no intermediate'; together that fits into 5 bits.
 * Parameter character can be 'no parameter character', or one from
 * 3/12..3/15; that fits into 3 bits.
 *
 * In @seq.intermediates, the nth intermediates is stored with shift n * 5,
 * plus (for CSI/DCS) an additional shift of 3 for the parameter character
 * which is stored at bits 0..2.
 *
 * VTE_SEQ_PARAMETER(u) extracts the parameter character
 *   of a CSI or DCS sequence
 * VTE_SEQ_REMOVE_PARAMETER(u) extracts the intermediates
 *   of a CSI or DCS sequence
 * VTE_SEQ_INTERMEDIATE(u) extracts the first intermediate from an
 *   intermediates value (for CSI/DCS, that must be without parameter
 *   character, see VTE_SEQ_REMOVE_PARAMETER)
 * VTE_SEQ_REMOVE_INTERMEDIATE(u) extracts the remaining intermediates
 *   after the first one; use VTE_SEQ_INTERMEDIATE on its return value
 *   to extract the 2nd intermediate, and so on
 */

#define VTE_SEQ_PARAMETER_BITS         (3)
#define VTE_SEQ_INTERMEDIATE_BITS      (5)
#define VTE_SEQ_INTERMEDIATE_MASK      ((1U << VTE_SEQ_INTERMEDIATE_BITS) - 1U)
#define VTE_SEQ_PARAMETER_MASK         ((1U << VTE_SEQ_PARAMETER_BITS) - 1U)
#define VTE_SEQ_PARAMETER(u)           ((u) & VTE_SEQ_PARAMETER_MASK)
#define VTE_SEQ_REMOVE_PARAMETER(u)    ((u) >> VTE_SEQ_PARAMETER_BITS)
#define VTE_SEQ_INTERMEDIATE(u)        ((u) & VTE_SEQ_INTERMEDIATE_MASK)
#define VTE_SEQ_REMOVE_INTERMEDIATE(u) ((u) >> VTE_SEQ_INTERMEDIATE_BITS)
#define VTE_MAKE_CHARSET(c,s)          ((c) | ((s) << VTE_CHARSET_SLOT_OFFSET))

/*
 * _VTE_SEQ_CODE_ESC(final, intermediates):
 *
 * Make a value combining the final character and the intermediates,
 * to be used to match a sequence against known sequences.
 *
 * Since this is only used with NONE or HASH as first intermediate,
 * we can reduce the size of the lookup table by slashing the least
 * significant bit off.
 *
 * Final characters is 3/0..7/14, needing 7 bits.
 */
#define _VTE_SEQ_CODE_ESC(f,i) (((f) - 0x30) | ((i) >> 1) << 7)

/*
 * _VTE_SEQ_CODE_COMBINE(parameter, intermediates)
 *
 * Combines intermediates and the parameter character into one
 * value to be used when matching a sequence against known sequences.
 */
#define _VTE_SEQ_CODE_COMBINE(p,i) ((p) | ((i) << VTE_SEQ_PARAMETER_BITS))

/*
 * _VTE_SEQ_CODE(final, intermediates):
 *
 * Make a value combining the final character and the intermediates,
 * to be used to match a sequence against known sequences. Used for
 * CSI and DCS sequences; use _VTE_SEQ_CODE_COMBINE to combine
 * parameter and intermediates into one to pass as 2nd argument here.
 *
 * Final character is 4/0..7/14, needing 6 bits.
 */
#define _VTE_SEQ_CODE(f,i) (((f) - 0x40) | ((i) << 6))

/*
 * VTE_TRANSITION:
 * @raw: the raw value
 * @state: the new state
 * @_action: function to call to handle action
 *
 * Set the state and call appropriate action, ensuring that we are
 * inlined without having to go through a function typedef.
 */
#define VTE_TRANSITION(raw, state, _action)  ({ m_state = state; _action(raw); })
#define VTE_TRANSITION_NO_ACTION(raw, state) ({ m_state = state; VTE_SEQ_NONE; })


namespace vte {
namespace parser {

/*
 * State Machine
 * This parser controls the parser-state and returns any detected sequence to
 * the caller. The parser is based on this state-diagram from Paul Williams:
 *   https://vt100.net/emu/
 * It was written from scratch and extended where needed.
 * This parser is fully compatible up to the vt500 series. We expect UCS-4 as
 * input. It's the callers responsibility to do any UTF-8 parsing.
 */
enum State {
        GROUND,           /* initial state and ground */
        DCS_PASS_ESC,     /* ESC after DCS which may be ESC \ aka C0 ST */
        OSC_STRING_ESC,   /* ESC after OSC which may be ESC \ aka C0 ST */
        ESC,              /* ESC sequence was started */
        ESC_INT,          /* intermediate escape characters */
        CSI_ENTRY,        /* starting CSI sequence */
        CSI_PARAM,        /* CSI parameters */
        CSI_INT,          /* intermediate CSI characters */
        CSI_IGNORE,       /* CSI error; ignore this CSI sequence */
        DCS_ENTRY,        /* starting DCS sequence */
        DCS_PARAM,        /* DCS parameters */
        DCS_INT,          /* intermediate DCS characters */
        DCS_PASS,         /* DCS data passthrough */
        DCS_IGNORE,       /* DCS error; ignore this DCS sequence */
        OSC_STRING,       /* parsing OSC sequence */
        ST_IGNORE,        /* unimplemented seq; ignore until ST */
        SCI,              /* single character introducer sequence was started */
};

class Sequence;

class Parser {
public:
        friend class Sequence;

        Parser() noexcept
        {
                memset(&m_seq, 0, sizeof m_seq);
                vte_seq_string_init(&m_seq.arg_str);
        }

        Parser(Parser const&) = delete;
        Parser(Parser&&) = delete;

        ~Parser() noexcept
        {
                vte_seq_string_free(&m_seq.arg_str);
        }

        Parser& operator=(Parser const&) = delete;
        Parser& operator=(Parser&&) = delete;

        inline int feed(uint32_t raw) noexcept
        {
                /*
                 * Notes:
                 *  * DEC treats GR codes as GL. We don't do that as we require UTF-8
                 *    as charset and, thus, it doesn't make sense to treat GR special.
                 *  * During control sequences, unexpected C1 codes cancel the sequence
                 *    and immediately start a new one. C0 codes, however, may or may not
                 *    be ignored/executed depending on the sequence.
                 */
                switch (raw) {
                case 0x18:                /* CAN */
                        return VTE_TRANSITION(raw, GROUND, action_ignore);
                case 0x1a:                /* SUB */
                        return VTE_TRANSITION(raw, GROUND, action_execute);
                case 0x7f:                 /* DEL */
                        return action_nop(raw);
                case 0x80 ... 0x8f:        /* C1 \ {DCS, SOS, SCI, CSI, ST, OSC, PM, APC} */
                case 0x91 ... 0x97:
                case 0x99:
                        return VTE_TRANSITION(raw, GROUND, action_execute);
                case 0x98:                /* SOS */
                case 0x9e:                /* PM */
                case 0x9f:                /* APC */
                        return VTE_TRANSITION_NO_ACTION(raw, ST_IGNORE);
                        // FIXMEchpe shouldn't this use action_clear?
                case 0x90:                /* DCS */
                        return VTE_TRANSITION(raw, DCS_ENTRY, action_dcs_start);
                case 0x9a:                /* SCI */
                        return VTE_TRANSITION(raw, SCI, action_clear);
                case 0x9d:                /* OSC */
                        return VTE_TRANSITION(raw, OSC_STRING, action_osc_start);
                case 0x9b:                /* CSI */
                        return VTE_TRANSITION(raw, CSI_ENTRY, action_clear_int_and_params);
                default: [[likely]]
                        return feed_to_state(raw);
                }
        }

        inline void reset() noexcept
        {
                VTE_TRANSITION(0, GROUND, action_ignore);
        }

        /*
         * set_dispatch_unripe:
         *
         * Enables or disables dispatch of unripe DCS sequences.
         * If enabled, known DCS sequences with the %VTE_DISPATCH_UNRIPE
         * flag will be dispatched when the Final character is received,
         * instead of when the control string terminator (ST) is received.
         * The application handling the unripe DCS sequence may then
         * either
         *
         * - do nothing; in this case the DCS sequence will be dispatched
         *   again when the control string was fully received. Ripe and
         *   unripe sequences can be distinguished by the value of
         *   parser.seq.st which will be 0 for an unripe sequence and
         *   either 0x5c (C0 ST) or 0x9c (C1 ST) for a ripe sequence. Or
         * - call vte_parser_ignore_until_st(); in this case the DCS
         *   sequence will be ignored until after the ST (or an other
         *   character that aborts the control string) has been
         *   received; or
         * - switch to a different parser (e.g. DECSIXEL) to parse the
         *   control string directly on-the-fly. Note that in this case,
         *   the subparser should take care to handle C0 and C1 controls
         *   the same way as this parser would.
         */
        inline void set_dispatch_unripe(bool enable) noexcept
        {
                m_dispatch_unripe = enable;
        }

        /*
         * ignore_until_st:
         *
         * When used on an unrip %VTE_SEQ_DCS sequence, makes the
         * parser ignore everything until the ST is received (or
         * the DCS is aborted by the usual other means).
         *
         * Note that there is some inconsistencies here:
         *
         * * SUB aborts the DCS in our parser, but e.g. a DECSIXEL
         *   parser will handle it as if 3/15 was received.
         *
         * * the ST terminating the DCS will be dispatched as an ST
         *   sequence, instead of producing an IGNORE sequence
         *   (this is easily fixable but would slightly complicate
         *   the parser for no actual gain).
         */
        inline void ignore_until_st() noexcept
        {
                switch (m_state) {
                case DCS_PASS:
                        VTE_TRANSITION_NO_ACTION(0, DCS_IGNORE);
                        break;
                default:
                        g_assert_not_reached();
                        break;
                }
        }

protected:
        vte_seq_t m_seq;
        guint m_state{0};
        bool m_dispatch_unripe{false};

        inline int feed_to_state(uint32_t raw) noexcept
        {
                switch (m_state) {
                case GROUND: [[likely]]
                        switch (raw) {
                        case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                        case 0x1c ... 0x1f:
                        case 0x80 ... 0x9f:        /* C1 */
                                return action_execute(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION(raw, ESC, action_clear_int);
                        default: [[likely]]
                                return action_print(raw);
                        }

                case DCS_PASS_ESC:
                case OSC_STRING_ESC:
                        if (raw == 0x5c /* '\' */) {
                                switch (m_state) {
                                case DCS_PASS_ESC:
                                        return VTE_TRANSITION(raw, GROUND, action_dcs_dispatch);
                                case OSC_STRING_ESC:
                                        return VTE_TRANSITION(raw, GROUND, action_osc_dispatch);
                                }
                        }

                        /* Do the deferred clear and fallthrough to ESC */
                        VTE_TRANSITION(0x1b /* ESC */, ESC, action_clear_int);

                        [[fallthrough]];
                case ESC:
                        switch (raw) {
                        case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                        case 0x1c ... 0x1f:
                                return action_execute(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION(raw, ESC, action_clear_int);
                        case 0x20 ... 0x2f:        /* [' ' - '\'] */
                                return VTE_TRANSITION(raw, ESC_INT, action_collect_esc);
                        case 0x30 ... 0x4f:        /* ['0' - '~'] \ */
                        case 0x51 ... 0x57:        /* { 'P', 'X', 'Z' '[', ']', '^', '_' } */
                        case 0x59:
                        case 0x5c:
                        case 0x60 ... 0x7e:
                                return VTE_TRANSITION(raw, GROUND, action_esc_dispatch);
                        case 0x50:                /* 'P' */
                                return VTE_TRANSITION(raw, DCS_ENTRY, action_dcs_start);
                        case 0x5a:                /* 'Z' */
                                return VTE_TRANSITION(raw, SCI, action_clear);
                        case 0x5b:                /* '[' */
                                return VTE_TRANSITION(raw, CSI_ENTRY, action_clear_params
                                                  /* rest already cleaned on ESC state entry */);
                        case 0x5d:                /* ']' */
                                return VTE_TRANSITION(raw, OSC_STRING, action_osc_start);
                        case 0x58:                /* 'X' */
                        case 0x5e:                /* '^' */
                        case 0x5f:                /* '_' */
                                return VTE_TRANSITION_NO_ACTION(raw, ST_IGNORE);
                        case 0x9c:                /* ST */
                                return VTE_TRANSITION(raw, GROUND, action_ignore);
                        }

                        return VTE_TRANSITION(raw, GROUND, action_ignore);
                case ESC_INT:
                        switch (raw) {
                        case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                        case 0x1c ... 0x1f:
                                return action_execute(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION(raw, ESC, action_clear_int);
                        case 0x20 ... 0x2f:        /* [' ' - '\'] */
                                return action_collect_esc(raw);
                        case 0x30 ... 0x7e:        /* ['0' - '~'] */
                                return VTE_TRANSITION(raw, GROUND, action_esc_dispatch);
                        case 0x9c:                /* ST */
                                return VTE_TRANSITION(raw, GROUND, action_ignore);
                        }

                        return VTE_TRANSITION(raw, GROUND, action_ignore);
                case CSI_ENTRY:
                        switch (raw) {
                        case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                        case 0x1c ... 0x1f:
                                return action_execute(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION(raw, ESC, action_clear_int);
                        case 0x20 ... 0x2f:        /* [' ' - '\'] */
                                return VTE_TRANSITION(raw, CSI_INT, action_collect_csi);
                        case 0x30 ... 0x39:        /* ['0' - '9'] */
                                return VTE_TRANSITION(raw, CSI_PARAM, action_param);
                        case 0x3a:                 /* ':' */
                                return VTE_TRANSITION(raw, CSI_PARAM, action_finish_subparam);
                        case 0x3b:                 /* ';' */
                                return VTE_TRANSITION(raw, CSI_PARAM, action_finish_param);
                        case 0x3c ... 0x3f:        /* ['<' - '?'] */
                                return VTE_TRANSITION(raw, CSI_PARAM, action_collect_parameter);
                        case 0x40 ... 0x7e:        /* ['@' - '~'] */
                                return VTE_TRANSITION(raw, GROUND, action_csi_dispatch);
                        case 0x9c:                /* ST */
                                return VTE_TRANSITION(raw, GROUND, action_ignore);
                        }

                        return VTE_TRANSITION_NO_ACTION(raw, CSI_IGNORE);
                case CSI_PARAM:
                        switch (raw) {
                        case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                        case 0x1c ... 0x1f:
                                return action_execute(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION(raw, ESC, action_clear_int);
                        case 0x20 ... 0x2f:        /* [' ' - '\'] */
                                return VTE_TRANSITION(raw, CSI_INT, action_collect_csi);
                        case 0x30 ... 0x39:        /* ['0' - '9'] */
                                return action_param(raw);
                        case 0x3a:                 /* ':' */
                                return action_finish_subparam(raw);
                        case 0x3b:                 /* ';' */
                                return action_finish_param(raw);
                        case 0x3c ... 0x3f:        /* ['<' - '?'] */
                                return VTE_TRANSITION_NO_ACTION(raw, CSI_IGNORE);
                        case 0x40 ... 0x7e:        /* ['@' - '~'] */
                                return VTE_TRANSITION(raw, GROUND, action_csi_dispatch);
                        case 0x9c:                /* ST */
                                return VTE_TRANSITION(raw, GROUND, action_ignore);
                        }

                        return VTE_TRANSITION_NO_ACTION(raw, CSI_IGNORE);
                case CSI_INT:
                        switch (raw) {
                        case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                        case 0x1c ... 0x1f:
                                return action_execute(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION(raw, ESC, action_clear_int);
                        case 0x20 ... 0x2f:        /* [' ' - '\'] */
                                return action_collect_csi(raw);
                        case 0x30 ... 0x3f:        /* ['0' - '?'] */
                                return VTE_TRANSITION_NO_ACTION(raw, CSI_IGNORE);
                        case 0x40 ... 0x7e:        /* ['@' - '~'] */
                                return VTE_TRANSITION(raw, GROUND, action_csi_dispatch);
                        case 0x9c:                /* ST */
                                return VTE_TRANSITION(raw, GROUND, action_ignore);
                        }

                        return VTE_TRANSITION_NO_ACTION(raw, CSI_IGNORE);
                case CSI_IGNORE:
                        switch (raw) {
                        case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                        case 0x1c ... 0x1f:
                                return action_execute(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION(raw, ESC, action_clear_int);
                        case 0x20 ... 0x3f:        /* [' ' - '?'] */
                                return action_nop(raw);
                        case 0x40 ... 0x7e:        /* ['@' - '~'] */
                                return VTE_TRANSITION_NO_ACTION(raw, GROUND);
                        case 0x9c:                /* ST */
                                return VTE_TRANSITION(raw, GROUND, action_ignore);
                        }

                        return action_nop(raw);
                case DCS_ENTRY:
                        switch (raw) {
                        case 0x00 ... 0x1a:        /* C0 \ ESC */
                        case 0x1c ... 0x1f:
                                return action_ignore(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION(raw, ESC, action_clear_int);
                        case 0x20 ... 0x2f:        /* [' ' - '\'] */
                                return VTE_TRANSITION(raw, DCS_INT, action_collect_csi);
                        case 0x30 ... 0x39:        /* ['0' - '9'] */
                                return VTE_TRANSITION(raw, DCS_PARAM, action_param);
                        case 0x3a:                 /* ':' */
                                return VTE_TRANSITION(raw, DCS_PARAM, action_finish_subparam);
                        case 0x3b:                 /* ';' */
                                return VTE_TRANSITION(raw, DCS_PARAM, action_finish_param);
                        case 0x3c ... 0x3f:        /* ['<' - '?'] */
                                return VTE_TRANSITION(raw, DCS_PARAM, action_collect_parameter);
                        case 0x40 ... 0x7e:        /* ['@' - '~'] */
                                return VTE_TRANSITION(raw, DCS_PASS, action_dcs_consume);
                        case 0x9c:                /* ST */
                                return VTE_TRANSITION(raw, GROUND, action_ignore);
                        }

                        return VTE_TRANSITION(raw, DCS_PASS, action_dcs_consume);
                case DCS_PARAM:
                        switch (raw) {
                        case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                        case 0x1c ... 0x1f:
                                return action_ignore(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION(raw, ESC, action_clear_int);
                        case 0x20 ... 0x2f:        /* [' ' - '\'] */
                                return VTE_TRANSITION(raw, DCS_INT, action_collect_csi);
                        case 0x30 ... 0x39:        /* ['0' - '9'] */
                                return action_param(raw);
                        case 0x3a:                 /* ':' */
                                return action_finish_subparam(raw);
                        case 0x3b:                 /* ';' */
                                return action_finish_param(raw);
                        case 0x3c ... 0x3f:        /* ['<' - '?'] */
                                return VTE_TRANSITION_NO_ACTION(raw, DCS_IGNORE);
                        case 0x40 ... 0x7e:        /* ['@' - '~'] */
                                return VTE_TRANSITION(raw, DCS_PASS, action_dcs_consume);
                        case 0x9c:                /* ST */
                                return VTE_TRANSITION(raw, GROUND, action_ignore);
                        }

                        return VTE_TRANSITION(raw, DCS_PASS, action_dcs_consume);
                case DCS_INT:
                        switch (raw) {
                        case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                        case 0x1c ... 0x1f:
                                return action_ignore(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION(raw, ESC, action_clear_int);
                        case 0x20 ... 0x2f:        /* [' ' - '\'] */
                                return action_collect_csi(raw);
                        case 0x30 ... 0x3f:        /* ['0' - '?'] */
                                return VTE_TRANSITION_NO_ACTION(raw, DCS_IGNORE);
                        case 0x40 ... 0x7e:        /* ['@' - '~'] */
                                return VTE_TRANSITION(raw, DCS_PASS, action_dcs_consume);
                        case 0x9c:                /* ST */
                                return VTE_TRANSITION(raw, GROUND, action_ignore);
                        }

                        return VTE_TRANSITION(raw, DCS_PASS, action_dcs_consume);
                case DCS_PASS:
                        switch (raw) {
                        case 0x00 ... 0x1a:        /* ASCII \ { ESC } */
                        case 0x1c ... 0x7f:
                                return action_dcs_collect(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION_NO_ACTION(raw, DCS_PASS_ESC);
                        case 0x9c:                /* ST */
                                return VTE_TRANSITION(raw, GROUND, action_dcs_dispatch);
                        }

                        return action_dcs_collect(raw);
                case DCS_IGNORE:
                        switch (raw) {
                        case 0x00 ... 0x1a:        /* ASCII \ { ESC } */
                        case 0x1c ... 0x7f:
                                return action_nop(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION(raw, ESC, action_clear_int);
                        case 0x9c:                /* ST */
                                return VTE_TRANSITION_NO_ACTION(raw, GROUND);
                        }

                        return action_nop(raw);
                case OSC_STRING:
                        switch (raw) {
                        case 0x00 ... 0x06:        /* C0 \ { BEL, ESC } */
                        case 0x08 ... 0x1a:
                        case 0x1c ... 0x1f:
                                return action_nop(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION_NO_ACTION(raw, OSC_STRING_ESC);
                        case 0x20 ... 0x7f:        /* [' ' - DEL] */
                                return action_osc_collect(raw);
                        case 0x07:                /* BEL */
                        case 0x9c:                /* ST */
                                return VTE_TRANSITION(raw, GROUND, action_osc_dispatch);
                        }

                        return action_osc_collect(raw);
                case ST_IGNORE:
                        switch (raw) {
                        case 0x00 ... 0x1a:        /* ASCII \ { ESC } */
                        case 0x1c ... 0x7f:
                                return action_nop(raw);
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION(raw, ESC, action_clear_int);
                        case 0x9c:                /* ST */
                                return VTE_TRANSITION(raw, GROUND, action_ignore);
                        }

                        return action_nop(raw);
                case SCI:
                        switch (raw) {
                        case 0x1b:                /* ESC */
                                return VTE_TRANSITION(raw, ESC, action_clear_int);
                        case 0x08 ... 0x0d:        /* BS, HT, LF, VT, FF, CR */
                        case 0x20 ... 0x7e:        /* [' ' - '~'] */
                                return VTE_TRANSITION(raw, GROUND, action_sci_dispatch);
                        }

                        return VTE_TRANSITION(raw, GROUND, action_ignore);
                }

                g_assert_not_reached();
                return VTE_SEQ_NONE;
        }

        /*
         * @introducer: either a C1 control, or the final in the equivalent ESC F sequence
         * @terminator: either a C1 control, or the final in the equivalent ESC F sequence
         *
         * Checks whether the OSC/DCS @introducer and the ST @terminator
         * are from the same control set, i.e. both C0 or both C1.
         *
         * For OSC, this check allows C0 OSC with BEL-as-ST to pass, too.
         */
        inline bool check_matching_controls(uint32_t introducer, uint32_t terminator)
        {
                return ((introducer ^ terminator) & 0x80) == 0;
        }

        /* ECMA-35 § 14.1 specifies that the final character 7/14 always identifies
         * an empty set. Note that that this does not apply for DRCS sets (§ 14.4),
         * since § 13.3.3 says that all the Ft (4/0..7/14) bytes are private-use.
         */
        inline constexpr uint32_t charset_empty_or_none(uint32_t raw) noexcept
        {
                return raw == 0x7e ? VTE_CHARSET_EMPTY : VTE_CHARSET_NONE;
        }

        inline void params_overflow(uint32_t raw) noexcept
        {
                /* An overflow of the parameter number can only happen in
                 * {CSI,DCS}_PARAM, and it occurs when
                 * seq.n_arg == VTE_PARSER_ARG_MAX, and either an 0…9
                 * is encountered, starting the next param, or an
                 * explicit ':' or ';' terminating a (defaulted) (sub)param,
                 * or when the intermediates/final character(s) occur
                 * after a defaulted (sub)param.
                 *
                 * Transition to {CSI,DCS}_IGNORE to ignore the
                 * whole sequence.
                 */
                VTE_TRANSITION_NO_ACTION(raw, m_state == CSI_PARAM ?  CSI_IGNORE : DCS_IGNORE);
        }

        uint32_t parse_host_sci(vte_seq_t const* seq) noexcept;
        uint32_t parse_host_dcs(vte_seq_t const* seq, unsigned int* flagsptr) noexcept;
        uint32_t parse_host_control(vte_seq_t const* seq) noexcept;
        uint32_t parse_host_csi(vte_seq_t const* seq) noexcept;
        uint32_t parse_host_escape(vte_seq_t const* seq, unsigned int *cs_out) noexcept;
        uint32_t parse_charset_control(uint32_t raw, unsigned int intermediates) noexcept;
        uint32_t parse_charset_ocs(uint32_t raw, unsigned int intermediates) noexcept;
        uint32_t parse_charset_96_n(uint32_t raw, unsigned int intermediates) noexcept;
        uint32_t parse_charset_96(uint32_t raw, unsigned int intermediates) noexcept;
        uint32_t parse_charset_94_n(uint32_t raw, unsigned int intermediates) noexcept;
        uint32_t parse_charset_94(uint32_t raw, unsigned int intermediates) noexcept;

        inline int action_clear(uint32_t raw) noexcept
        {
                /* seq.command is set when the sequence is executed,
                 * seq.terminator is set when the final character is received,
                 * and seq.introducer is set when the introducer is received,
                 * and all this happens before the sequence is dispatched.
                 * Therefore these fiedls need not be cleared in any case.
                 */
                return VTE_SEQ_NONE;
        }

        inline int action_clear_int(uint32_t raw) noexcept
        {
                m_seq.intermediates = 0;
                m_seq.n_intermediates = 0;

                return action_clear(raw);
        }

        inline int action_clear_params(uint32_t raw) noexcept
        {
                /* The (n_args+1)th parameter may have been started but not
                 * finialised, so it needs cleaning too. All further params
                 * have not been touched, so need not be cleaned.
                 */
                unsigned int n_args = G_UNLIKELY (m_seq.n_args >= VTE_PARSER_ARG_MAX)
                        ? VTE_PARSER_ARG_MAX
                        : m_seq.n_args + 1;
                memset(m_seq.args, 0, n_args * sizeof(m_seq.args[0]));
#ifdef PARSER_EXTRA_CLEAN
                /* Assert that the assumed-clean params are actually clean. */
                for (unsigned int n = n_args; n < VTE_PARSER_ARG_MAX; ++n)
                        vte_assert_cmpuint(m_seq.args[n], ==, VTE_SEQ_ARG_INIT_DEFAULT);
#endif

                m_seq.n_args = 0;
                m_seq.n_final_args = 0;

                return VTE_SEQ_NONE;
        }

        inline int action_clear_int_and_params(uint32_t raw) noexcept
        {
                action_clear_int(raw);
                return action_clear_params(raw);
        }


        inline int action_collect_csi(uint32_t raw) noexcept
        {
                g_assert(raw >= 0x20 && raw <= 0x2f);

                /* In addition to 2/0..2/15 intermediates, CSI/DCS sequence
                 * can also have one parameter byte 3/12..3/15 at the
                 * start of the parameters (see parser_collect_parameter below);
                 * that's what the extra shift is for.
                 */
                m_seq.intermediates |= (VTE_SEQ_MAKE_INTERMEDIATE(raw) << (VTE_SEQ_PARAMETER_BITS +
                                                                           VTE_SEQ_INTERMEDIATE_BITS * m_seq.n_intermediates++));

                return VTE_SEQ_NONE;
        }

        inline int action_collect_esc(uint32_t raw) noexcept
        {
                g_assert(raw >= 0x20 && raw <= 0x2f);

                /* ESCAPE sequences only have intermediates or 2/0..2/15, so there's no
                 * need for the extra shift as below for CSI/DCS sequences
                 */
                m_seq.intermediates |= (VTE_SEQ_MAKE_INTERMEDIATE(raw) << (VTE_SEQ_INTERMEDIATE_BITS * m_seq.n_intermediates++));

                return VTE_SEQ_NONE;
        }

        inline int action_collect_parameter(uint32_t raw) noexcept
        {
                g_assert(raw >= 0x3c && raw <= 0x3f);

                /* CSI/DCS may optionally have one parameter byte from 3/12..3/15
                 * at the start of the parameters; we put that into the lowest
                 * part of @seq.intermediates.
                 * Note that there can only be *one* such byte; the state machine
                 * already enforces that, so we do not need any additional checks
                 * here.
                 */
                m_seq.intermediates |= VTE_SEQ_MAKE_PARAMETER(raw);

                return VTE_SEQ_NONE;
        }

        inline int action_csi_dispatch(uint32_t raw) noexcept
        {
                /* m_seq is cleared during CSI-ENTER state, thus there's no need
                 * to clear invalid fields here. */

                if (G_LIKELY(m_seq.n_args < VTE_PARSER_ARG_MAX)) {
                        if (m_seq.n_args > 0 ||
                            vte_seq_arg_started(m_seq.args[m_seq.n_args])) {
                                vte_seq_arg_finish(&m_seq.args[m_seq.n_args], false);
                                ++m_seq.n_args;
                                ++m_seq.n_final_args;
                        }
                }

                m_seq.type = VTE_SEQ_CSI;
                m_seq.terminator = raw;
                m_seq.command = parse_host_csi(&m_seq);

                return VTE_SEQ_CSI;
        }

        inline int action_dcs_dispatch(uint32_t raw) noexcept
        {
                /* Most of m_seq was already filled in action_dcs_consume() */
                m_seq.st = raw;

                vte_seq_string_finish(&m_seq.arg_str);

                /* We only dispatch a DCS if the introducer and string
                 * terminator are from the same control set, i.e. both
                 * C0 or both C1; we discard sequences with mixed controls.
                 */
                if (!check_matching_controls(m_seq.introducer, raw))
                        return VTE_SEQ_IGNORE;

                return m_seq.type;
        }

        inline int action_dcs_collect(uint32_t raw) noexcept
        {
                if G_UNLIKELY (!vte_seq_string_push(&m_seq.arg_str, raw))
                        m_state = DCS_IGNORE;

                return VTE_SEQ_NONE;
        }

        inline int action_esc_dispatch(uint32_t raw) noexcept
        {
                m_seq.type = VTE_SEQ_ESCAPE;
                m_seq.terminator = raw;
                m_seq.charset = VTE_CHARSET_NONE;
                m_seq.command = parse_host_escape(&m_seq, &m_seq.charset);

                return VTE_SEQ_ESCAPE;
        }

        inline int action_execute(uint32_t raw) noexcept
        {
                m_seq.type = VTE_SEQ_CONTROL;
                m_seq.terminator = raw;
                m_seq.command = parse_host_control(&m_seq);

                return VTE_SEQ_CONTROL;
        }

        inline int action_dcs_consume(uint32_t raw) noexcept
        {
                /* m_seq is cleared during DCS-START state, thus there's no need
                 * to clear invalid fields here. */

                if G_LIKELY (m_seq.n_args < VTE_PARSER_ARG_MAX) {
                        if (m_seq.n_args > 0 ||
                            vte_seq_arg_started(m_seq.args[m_seq.n_args])) {
                                vte_seq_arg_finish(&m_seq.args[m_seq.n_args], false);
                                ++m_seq.n_args;
                                ++m_seq.n_final_args;
                        }
                }

                m_seq.type = VTE_SEQ_DCS;
                m_seq.terminator = raw;
                m_seq.st = 0;

                auto flags = unsigned{};
                m_seq.command = parse_host_dcs(&m_seq, &flags);

                return (flags & VTE_DISPATCH_UNRIPE) && m_dispatch_unripe ? VTE_SEQ_DCS : VTE_SEQ_NONE;
        }

        inline int action_dcs_start(uint32_t raw) noexcept
        {
                action_clear_int_and_params(raw);

                vte_seq_string_reset(&m_seq.arg_str);

                m_seq.introducer = raw;

                return VTE_SEQ_NONE;
        }

        /* The next two functions are only called when encountering a ';' or ':',
         * so if there's already MAX-1 parameters, the ';' or ':' would finish
         * the MAXth parameter and there would be a default or non-default
         * MAX+1th parameter following it.
         */
        inline int action_finish_param(uint32_t raw)
        {
                if G_LIKELY (m_seq.n_args < VTE_PARSER_ARG_MAX - 1) {
                        vte_seq_arg_finish(&m_seq.args[m_seq.n_args], false);
                        ++m_seq.n_args;
                        ++m_seq.n_final_args;
                } else {
                        params_overflow(raw);
                }

                return VTE_SEQ_NONE;
        }

        inline int action_finish_subparam(uint32_t raw) noexcept
        {
                if G_LIKELY (m_seq.n_args < VTE_PARSER_ARG_MAX - 1) {
                        vte_seq_arg_finish(&m_seq.args[m_seq.n_args], true);
                        ++m_seq.n_args;
                } else {
                        params_overflow(raw);
                }

                return VTE_SEQ_NONE;
        }

        inline int action_ignore(uint32_t raw) noexcept
        {
                m_seq.type = VTE_SEQ_IGNORE;
                m_seq.command = VTE_CMD_NONE;
                m_seq.terminator = raw;

                return VTE_SEQ_IGNORE;
        }

        inline int action_nop(uint32_t raw) noexcept
        {
                return VTE_SEQ_NONE;
        }


        inline int action_osc_collect(uint32_t raw) noexcept
        {
                /*
                 * Only characters from 0x20..0x7e and >= 0xa0 are allowed here.
                 * Our state-machine already verifies those restrictions.
                 */
                if G_UNLIKELY (!vte_seq_string_push(&m_seq.arg_str, raw))
                        m_state = ST_IGNORE;

                return VTE_SEQ_NONE;
        }

        inline int action_osc_dispatch(uint32_t raw) noexcept
        {
                /* m_seq is cleared during OSC_START state, thus there's no need
                 * to clear invalid fields here. */

                vte_seq_string_finish(&m_seq.arg_str);

                /* We only dispatch a DCS if the introducer and string
                 * terminator are from the same control set, i.e. both
                 * C0 or both C1; we discard sequences with mixed controls.
                 */
                if (!check_matching_controls(m_seq.introducer, raw))
                        return VTE_SEQ_IGNORE;

                m_seq.type = VTE_SEQ_OSC;
                m_seq.command = VTE_CMD_OSC;
                m_seq.st = raw;

                return VTE_SEQ_OSC;
        }

        inline int action_osc_start(uint32_t raw) noexcept
        {
                action_clear(raw);

                vte_seq_string_reset(&m_seq.arg_str);

                m_seq.introducer = raw;

                return VTE_SEQ_NONE;
        }

        inline int action_param(uint32_t raw) noexcept
        {
                /* g_assert(raw >= '0' && raw <= '9'); */

                if G_LIKELY (m_seq.n_args < VTE_PARSER_ARG_MAX)
                        vte_seq_arg_push(&m_seq.args[m_seq.n_args], raw);
                else
                        params_overflow(raw);

                return VTE_SEQ_NONE;
        }

        inline int action_print(uint32_t raw) noexcept
        {
                m_seq.type = VTE_SEQ_GRAPHIC;
                m_seq.command = VTE_CMD_GRAPHIC;
                m_seq.terminator = raw;

                return VTE_SEQ_GRAPHIC;
        }

        inline int action_sci_dispatch(uint32_t raw) noexcept
        {
                m_seq.type = VTE_SEQ_SCI;
                m_seq.terminator = raw;
                m_seq.command = parse_host_sci(&m_seq);

                return VTE_SEQ_SCI;
        }

}; // class Parser

class Sequence {
public:

        Sequence() = default;
        Sequence(Sequence const&) = delete;
        Sequence(Sequence&&) = delete;
        ~Sequence() = default;

        Sequence(Parser& parser)
        {
                m_seq = &parser.m_seq;
        }

        typedef int number;

        char* ucs4_to_utf8(gunichar const* str,
                           ssize_t len = -1) const noexcept;

        void print() const noexcept;

        /* type:
         *
         *
         * Returns: the type of the sequence, a value from the VTE_SEQ_* enum
         */
        inline constexpr unsigned int type() const noexcept
        {
                return m_seq->type;
        }

        /* command:
         *
         * Returns: the command the sequence codes for, a value
         *   from the VTE_CMD_* enum, or %VTE_CMD_NONE if the command is
         *   unknown
         */
        inline constexpr unsigned int command() const noexcept
        {
                return m_seq->command;
        }

        /* charset:
         *
         * This is the charset to use in a %VTE_CMD_GnDm, %VTE_CMD_GnDMm,
         * %VTE_CMD_CnD or %VTE_CMD_DOCS command.
         *
         * Returns: the charset, a value from the VTE_CHARSET_* enum.
         */
        inline constexpr unsigned int charset() const noexcept
        {
                return VTE_CHARSET_GET_CHARSET(m_seq->charset);
        }

        /* slot:
         *
         * This is the slot in a %VTE_CMD_GnDm, %VTE_CMD_GnDMm,
         * or %VTE_CMD_CnD command.
         *
         * Returns: the slot, a value from the 0..3 for Gn*, or 0..1 for CnD
         */
        inline constexpr unsigned int slot() const noexcept
        {
                return VTE_CHARSET_GET_SLOT(m_seq->charset);
        }

        /* introducer:
         *
         * This is the character introducing the sequence, if any.
         *
         * Returns: the introducing character
         */
        inline constexpr uint32_t introducer() const noexcept
        {
                return m_seq->introducer;
        }

        /* terminator:
         *
         * This is the character terminating the sequence, or, for a
         * %VTE_SEQ_GRAPHIC sequence, the graphic character.
         *
         * Returns: the terminating character
         */
        inline constexpr uint32_t terminator() const noexcept
        {
                return m_seq->terminator;
        }

        /* st:
         *
         * This is the string terminator ending a OSC, DCS, APC, PM, or SOS sequence
         *
         * Returns: the string terminator character
         */
        inline constexpr uint32_t st() const noexcept
        {
                return m_seq->st;
        }

        /* is_c1:
         *
         * Whether the sequence was introduced with a C0 or C1 control.
         *
         * Returns: true iff the introducer was a C1 control
         */
        inline constexpr bool is_c1() const noexcept
        {
                return (introducer() & 0x80) != 0;
        }

        /* is_st_c1:
         *
         * Whether the control string was terminated with a C0 or C1 control.
         *
         * Returns: true iff the terminator was the C1 ST
         */
        inline constexpr bool is_st_c1() const noexcept
        {
                return (st() & 0x80) != 0;
        }

        /* is_st_bel:
         *
         * Whether the control string was terminated with a BEL. This is only supported
         * for OSC, for xterm compatibility, and is deprecated.
         *
         * Returns: true iff the terminator was BEL
         */
        inline constexpr bool is_st_bel() const noexcept
        {
                return st() == 0x7;
        }

        /* is_ripe:
         *
         * Whether the control string is complete.
         * This returns true when the final character has been received,
         * and false when the string terminator has been received.
         * This is only meaningful for DCS sequences, which are dispatched
         * twice.
         *
         * Returns: true iff the DCS sequence is complete
         */
        inline constexpr bool is_ripe() const noexcept
        {
                return st() != 0;
        }

        inline constexpr bool is_unripe() const noexcept
        {
                return !is_ripe();
        }

        /* intermediates:
         *
         * This is the pintro and intermediate characters in the sequence, if any.
         *
         * Returns: the intermediates
         */
        inline constexpr unsigned int intermediates() const noexcept
        {
                return m_seq->intermediates;
        }

        // FIXMEchpe: upgrade to C++17 and use the u32string_view version below, instead
        /*
         * string:
         *
         * This is the string argument of a DCS or OSC sequence.
         *
         * Returns: the string argument
         */
        inline std::u32string string() const noexcept
        {
                size_t len;
                auto buf = vte_seq_string_get(&m_seq->arg_str, &len);
                return std::u32string(reinterpret_cast<char32_t*>(buf), len);
        }

        #if 0
        /*
         * string:
         *
         * This is the string argument of a DCS or OSC sequence.
         *
         * Returns: the string argument
         */
        inline constexpr std::u32string_view string() const noexcept
        {
                size_t len = 0;
                auto buf = vte_seq_string_get(&m_seq->arg_str, &len);
                return std::u32string_view(buf, len);
        }
        #endif

        /*
         * string:
         *
         * This is the string argument of a DCS or OSC sequence.
         *
         * Returns: the string argument
         */
        std::string string_utf8() const noexcept;

        inline char* string_param() const noexcept
        {
                size_t len = 0;
                auto buf = vte_seq_string_get(&m_seq->arg_str, &len);
                return ucs4_to_utf8(buf, len);
        }

        /* size:
         *
         * Returns: the number of parameters
         */
        inline constexpr unsigned int size() const noexcept
        {
                return m_seq->n_args;
        }


        /* size:
         *
         * Returns: the number of parameter blocks, counting runs of subparameters
         *   as only one parameter
         */
        inline constexpr unsigned int size_final() const noexcept
        {
                return m_seq->n_final_args;
        }

        /* capacity:
         *
         * Returns: the number of parameter blocks, counting runs of subparameters
         *   as only one parameter
         */
        inline constexpr unsigned int capacity() const noexcept
        {
                return G_N_ELEMENTS(m_seq->args);
        }

        /* param:
         * @idx:
         * @default_v: the value to use for default parameters
         *
         * Returns: the value of the parameter at index @idx, or @default_v if
         *   the parameter at this index has default value, or the index
         *   is out of bounds
         */
        inline constexpr int param(unsigned int idx,
                                   int default_v = -1) const noexcept
        {
                return __builtin_expect(idx < size(), 1) ? vte_seq_arg_value(m_seq->args[idx], default_v) : default_v;
        }

        /* param:
         * @idx:
         * @default_v: the value to use for default parameters
         * @min_v: the minimum value
         * @max_v: the maximum value
         *
         * Returns: the value of the parameter at index @idx, or @default_v if
         *   the parameter at this index has default value, or the index
         *   is out of bounds. The returned value is clamped to the
         *   range @min_v..@max_v (or returns min_v, if min_v > max_v).
         */
        inline constexpr int param(unsigned int idx,
                                   int default_v,
                                   int min_v,
                                   int max_v) const noexcept
        {
                auto v = param(idx, default_v);
                // not using std::clamp() since it's not guaranteed that min_v <= max_v
                return std::max(std::min(v, max_v), min_v);
        }

        /* param_range:
         * @idx:
         * @default_v: the value to use for default parameters
         * @min_v: the minimum value
         * @max_v: the maximum value
         * @oor_v: the value to return for out-of-range values
         *
         * Returns: the value of the parameter at index @idx, or @default_v if
         *   the parameter at this index has default value, or the index
         *   is out of bounds. If the value is outside the range @min_v..@max_v,
         *   returns @oor_v.
         */
        inline constexpr int param_range(unsigned int idx,
                                         int default_v,
                                         int min_v,
                                         int max_v,
                                         int oor_v) const noexcept
        {
                auto v = param(idx, default_v);
                return (v >= min_v && v <= max_v) ? v : oor_v;
        }

        /* param_nonfinal:
         * @idx:
         *
         * Returns: whether the parameter at @idx is nonfinal, i.e.
         * there are more subparameters after it.
         */
        inline constexpr bool param_nonfinal(unsigned int idx) const noexcept
        {
                return __builtin_expect(idx < size(), 1) ? vte_seq_arg_nonfinal(m_seq->args[idx]) : false;
        }

        /* param_default:
         * @idx:
         *
         * Returns: whether the parameter at @idx has default value
         */
        inline constexpr bool param_default(unsigned int idx) const noexcept
        {
                return __builtin_expect(idx < size(), 1) ? vte_seq_arg_default(m_seq->args[idx]) : true;
        }

        /* next:
         * @idx:
         *
         * Returns: the index of the next parameter block
         */
        inline constexpr unsigned int next(unsigned int idx) const noexcept
        {
                /* Find the final parameter */
                while (param_nonfinal(idx))
                        ++idx;
                /* And return the index after that one */
                return ++idx;
        }

        inline constexpr unsigned int cbegin() const noexcept
        {
                return 0;
        }

        inline constexpr unsigned int cend() const noexcept
        {
                return size();
        }

        /* collect:
         *
         * Collects some final parameters.
         *
         * Returns: %true if the sequence parameter list begins with
         *  a run of final parameters that were collected.
         */
        inline constexpr bool collect(unsigned int start_idx,
                                      std::initializer_list<int*> params,
                                      int default_v = -1) const noexcept
        {
                unsigned int idx = start_idx;
                for (auto i : params) {
                        *i = param(idx, default_v);
                        idx = next(idx);
                }

                return (idx - start_idx) == params.size();
        }

        /* collect1:
         * @idx:
         * @default_v:
         *
         * Collects one final parameter.
         *
         * Returns: the parameter value, or @default_v if the parameter has
         *   default value or is not a final parameter
         */
        inline constexpr int collect1(unsigned int idx,
                                      int default_v = -1) const noexcept
        {
                return __builtin_expect(idx < size(), 1) ? vte_seq_arg_value_final(m_seq->args[idx], default_v) : default_v;
        }

        /* collect1:
         * @idx:
         * @default_v:
         * @min_v:
         * @max_v
         *
         * Collects one final parameter.
         *
         * Returns: the parameter value clamped to the @min_v .. @max_v range (or @min_v,
         * if min_v > max_v),
         *   or @default_v if the parameter has default value or is not a final parameter
         */
        inline constexpr int collect1(unsigned int idx,
                                      int default_v,
                                      int min_v,
                                      int max_v) const noexcept
        {
                int v = __builtin_expect(idx < size(), 1) ? vte_seq_arg_value_final(m_seq->args[idx], default_v) : default_v;
                // not using std::clamp() since it's not guaranteed that min_v <= max_v
                return std::max(std::min(v, max_v), min_v);
        }

        /* collect_subparams:
         *
         * Collects some subparameters.
         *
         * Returns: %true if the sequence parameter list contains enough
         *   subparams at @start_idx
         */
        inline constexpr bool collect_subparams(unsigned int start_idx,
                                                std::initializer_list<int*> params,
                                                int default_v = -1) const noexcept
        {
                unsigned int idx = start_idx;
                for (auto i : params) {
                        *i = param(idx++, default_v);
                }

                return idx <= next(start_idx);
        }

        /* collect_number:
         * @start_idx:
         *
         * Collects a big number from a run of subparameters ending in a
         * final parameter.
         * This allows encoding numbers larger than the 16-bit parameter
         * limit, by using a run of subparameters which encode the number
         * in big-endian. Default subparams have value 0.
         *
         * Returns: the number, or %nullopt if the number exceeds the
         *   limits of uint64_t.
         */
        inline constexpr std::optional<uint64_t> collect_number(unsigned int start_idx) const noexcept
        {
                auto idx = start_idx;
                auto const next_idx = next(start_idx);

                // No more than 4 16-bit parameters can be combined
                // without exceeding the 64-bit number limits.
                if ((next_idx - idx) > 4)
                        return std::nullopt;

                // No leading default (empty) subparams allowed
                if (next_idx != (idx + 1) && param_default(idx))
                        return std::nullopt;

                auto value = uint64_t{0};
                for (auto i = idx; i < next_idx; ++i) {
                        value <<= 16;
                        value += param(i, 0);
                }

                return value;
        }

        /* collect_char:
         * @idx:
         * @default_v: return value for default parameter
         * @zero_v: return value for zero parameter
         *
         * Collects an unicode character from a parameter or a run of
         * subparameters ending in a final parameter.
         *
         * A default final parameter returns the character @default_v;
         * a final zeroed parameter returns the charcacter @zero_v if
         * not -1, or @default_v if -1.
         *
         * This allows encoding characters outside plane 0 using
         * either the big number encoding (see collect_number() above),
         * or as a pair of surrogates.
         *
         * Returns: the character, or %nullopt for incorrect encoding,
         *   or if the character specified by the params is a control
         *   character, or a surrogate
         */
        inline constexpr std::optional<char32_t> collect_char(unsigned int idx,
                                                              int default_v = 0x20,
                                                              int zero_v = -1) const noexcept
        {
                auto const n_params = next(idx) - idx;

                auto v = uint32_t{0};
                if (n_params == 1) {
                        auto pv = param(idx);
                        if (pv == 0)
                                pv = zero_v;

                        v = pv != -1 ? pv : default_v;
                } else if (n_params == 2) {
                        auto p0 = param(idx, 0);
                        auto p1 = param(idx + 1, 0);

                        // Surrogate pair
                        if ((p0 & 0xfffffc00u) == 0xd800u &&
                            (p1 & 0xfffffc00u) == 0xdc00u) {
                                v = ((p0 & 0x3ffu) << 10) + (p1 & 0x3ffu) + 0x10000u;
                        } else { // big number as above
                                v = p0 << 16 | p1;
                        }
                } else if (n_params > 2) {
                        v = 0;
                }

                auto valid = [](uint32_t c) constexpr noexcept -> bool
                {
                        return c < 0x110000u && // plane 0 ... plane 17
                                (c & 0xffffff60u) != 0 && // not C0 nor C1
                                c != 0x7fu && // not DEL
                                (c & 0xfffff800u) != 0xd800u; // not a surrogate
                };

                if (valid(v))
                        return char32_t{v};

                return std::nullopt;
        }

        inline explicit operator bool() const { return m_seq != nullptr; }

        /* This is only used in the test suite */
        vte_seq_t** seq_ptr() { return &m_seq; }

private:
        vte_seq_t* m_seq{nullptr};

        char const* type_string() const;
        char const* command_string() const;
}; // class Sequence


} // namespace parser
} // namespace vte
