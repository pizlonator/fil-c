/* -*- mesa-c++  -*-
 * Copyright 2019 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "sfn_debug.h"

#include "util/u_debug.h"

namespace r600 {

stderr_streambuf::stderr_streambuf() {}

int
stderr_streambuf::sync()
{
   fflush(stderr);
   return 0;
}

int
stderr_streambuf::overflow(int c)
{
   fputc(c, stderr);
   return 0;
}

static const struct debug_named_value sfn_debug_options[] = {
   {"instr",    SfnLog::instr,       "Log all consumed nir instructions"    },
   {"ir",       SfnLog::r600ir,      "Log created R600 IR"                  },
   {"cc",       SfnLog::cc,          "Log R600 IR to assembly code creation"},
   {"noerr",    SfnLog::err,         "Don't log shader conversion errors"   },
   {"si",       SfnLog::shader_info, "Log shader info (non-zero values)"    },
   {"ts",       SfnLog::test_shader, "Log shaders in tests"                 },
   {"reg",      SfnLog::reg,         "Log register allocation and lookup"   },
   {"io",       SfnLog::io,          "Log shader in and output"             },
   {"ass",      SfnLog::assembly,    "Log IR to assembly conversion"        },
   {"flow",     SfnLog::flow,        "Log Flow instructions"                },
   {"merge",    SfnLog::merge,       "Log register merge operations"        },
   {"nomerge",  SfnLog::nomerge,     "Skip register merge step"             },
   {"tex",      SfnLog::tex,         "Log texture ops"                      },
   {"trans",    SfnLog::trans,       "Log generic translation messages"     },
   {"schedule", SfnLog::schedule,    "Log scheduling"                       },
   {"opt",      SfnLog::opt,         "Log optimization"                     },
   {"steps",    SfnLog::steps,       "Log shaders at transformation steps"  },
   {"noopt",    SfnLog::noopt,       "Don't run backend optimizations"      },
   {"warn" ,    SfnLog::warn,        "Print warnings"                       },
   DEBUG_NAMED_VALUE_END
};

SfnLog sfn_log;

std::streamsize
stderr_streambuf::xsputn(const char *s, std::streamsize n)
{
   std::streamsize i = n;
   while (i--)
      fputc(*s++, stderr);
   return n;
}

SfnLog::SfnLog():
    m_active_log_flags(0),
    m_log_mask(0),
    m_buf(),
    m_output(&m_buf)
{
   m_log_mask = debug_get_flags_option("R600_NIR_DEBUG", sfn_debug_options, 0);
   m_log_mask ^= err;
}

SfnLog&
SfnLog::operator<<(SfnLog::LogFlag const l)
{
   m_active_log_flags = l;
   return *this;
}

SfnLog&
SfnLog::operator<<(UNUSED std::ostream& (*f)(std::ostream&))
{
   if (m_active_log_flags & m_log_mask)
      m_output << f;
   return *this;
}

SfnLog&
SfnLog::operator<<(nir_shader& sh)
{
   if (m_active_log_flags & m_log_mask)
      nir_print_shader(&sh, stderr);
   return *this;
}

SfnLog&
SfnLog::operator<<(nir_instr& instr)
{
   if (m_active_log_flags & m_log_mask)
      nir_print_instr(&instr, stderr);
   return *this;
}

SfnTrace::SfnTrace(SfnLog::LogFlag flag, const char *msg):
    m_flag(flag),
    m_msg(msg)
{
   sfn_log << m_flag << std::string( 2 * m_indention++, ' ') << "BEGIN: " << m_msg << "\n";
}

SfnTrace::~SfnTrace()
{
   assert(m_indention > 0);
   sfn_log << m_flag << std::string( 2 * m_indention--, ' ') << "END:   " << m_msg << "\n";
}

int SfnTrace::m_indention = 0;

} // namespace r600
