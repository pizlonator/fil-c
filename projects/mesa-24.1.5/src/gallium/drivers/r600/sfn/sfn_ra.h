/* -*- mesa-c++  -*-
 * Copyright 2022 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef INTERFERENCE_H
#define INTERFERENCE_H

#include "sfn_valuefactory.h"

#include <vector>

namespace r600 {

class ComponentInterference {
public:
   using Row = std::vector<int>;

   void prepare_row(int row);

   void add(size_t idx1, size_t idx2);

   auto row(int idx) const -> const Row&
   {
      assert((size_t)idx < m_rows.size());
      return m_rows[idx];
   }

private:
   std::vector<Row> m_rows;
};

class Interference {
public:
   Interference(LiveRangeMap& map);

   const auto& row(int comp, int index) const
   {
      assert(comp < 4);
      return m_components_maps[comp].row(index);
   }

private:
   void initialize();
   void initialize(ComponentInterference& comp, LiveRangeMap::ChannelLiveRange& clr);

   LiveRangeMap& m_map;
   std::array<ComponentInterference, 4> m_components_maps;
};

bool
register_allocation(LiveRangeMap& lrm);

} // namespace r600

#endif // INTERFERENCE_H
