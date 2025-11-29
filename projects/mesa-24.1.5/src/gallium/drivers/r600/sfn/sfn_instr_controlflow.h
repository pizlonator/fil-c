/* -*- mesa-c++  -*-
 * Copyright 2022 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef CONTROLFLOWINSTR_H
#define CONTROLFLOWINSTR_H

#include "sfn_instr_alu.h"

namespace r600 {

class ControlFlowInstr : public Instr {
public:
   enum CFType {
      cf_else,
      cf_endif,
      cf_loop_begin,
      cf_loop_end,
      cf_loop_break,
      cf_loop_continue,
      cf_wait_ack
   };

   ControlFlowInstr(CFType type);

   ControlFlowInstr(const ControlFlowInstr& orig) = default;

   bool is_equal_to(const ControlFlowInstr& lhs) const;

   void accept(ConstInstrVisitor& visitor) const override;
   void accept(InstrVisitor& visitor) override;

   CFType cf_type() const { return m_type; }

   int nesting_corr() const override;

   static Instr::Pointer from_string(std::string type_str);

   bool end_block() const override { return true; }

   int nesting_offset() const override;

private:
   bool do_ready() const override;
   void do_print(std::ostream& os) const override;

   CFType m_type;
};

class IfInstr : public Instr {
public:
   IfInstr(AluInstr *pred);
   IfInstr(const IfInstr& orig);

   bool is_equal_to(const IfInstr& lhs) const;

   void set_predicate(AluInstr *new_predicate);

   AluInstr *predicate() const { return m_predicate; }
   AluInstr *predicate() { return m_predicate; }

   uint32_t slots() const override;

   void accept(ConstInstrVisitor& visitor) const override;
   void accept(InstrVisitor& visitor) override;

   bool replace_source(PRegister old_src, PVirtualValue new_src) override;

   static Instr::Pointer from_string(std::istream& is, ValueFactory& value_factory, bool is_cayman);

   bool end_block() const override { return true; }
   int nesting_offset() const override { return 1; }

private:
   bool do_ready() const override;
   void do_print(std::ostream& os) const override;
   void forward_set_blockid(int id, int index) override;
   void forward_set_scheduled() override;

   AluInstr *m_predicate;
};

} // namespace r600

#endif // CONTROLFLOWINSTR_H
