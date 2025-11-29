/* -*- mesa-c++  -*-
 * Copyright 2022 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef INSTRFACTORY_H
#define INSTRFACTORY_H

#include "sfn_instr.h"
#include "sfn_valuefactory.h"

#include <iosfwd>

namespace r600 {

class Shader;
class InstrFactory : public Allocate {
public:
   InstrFactory();

   PInst from_string(const std::string& s, int nesting_depth, bool is_cayman);
   bool from_nir(nir_instr *instr, Shader& shader);
   auto& value_factory() { return m_value_factory; }

private:
   bool load_const(nir_load_const_instr *lc, Shader& shader);
   bool process_jump(nir_jump_instr *instr, Shader& shader);
   bool process_undef(nir_undef_instr *undef, Shader& shader);

   Instr::Pointer export_from_string(std::istream& is, bool is_last);

   ValueFactory m_value_factory;
   AluGroup *group;
};

} // namespace r600

#endif // INSTRFACTORY_H
