/*
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

#include "config.h"

#include "chunk.hh"

#include <cstddef> // offsetof
#include <new>

namespace vte {

namespace base {

void
Chunk::recycle() noexcept
{
        g_free_chunks.push(std::unique_ptr<Chunk>(this));
        /* FIXME: bzero out the chunk for security? */
}

std::stack<std::unique_ptr<Chunk>, std::list<std::unique_ptr<Chunk>>> Chunk::g_free_chunks;

Chunk::unique_type
Chunk::get(Chunk const* chain_to) noexcept
{
        Chunk* chunk;
        if (!g_free_chunks.empty()) {
                chunk = g_free_chunks.top().release();
                g_free_chunks.pop();

                chunk->reset();
        } else {
                chunk = new Chunk();
        }

        if (chain_to)
                chunk->chain(chain_to);

        return Chunk::unique_type(chunk);
}
void
Chunk::prune(unsigned int max_size) noexcept
{
        while (g_free_chunks.size() > max_size)
                g_free_chunks.pop();
}

} // namespace base

} // namespace vte
