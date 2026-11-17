/*
 * Copyright © 2018, 2020 Christian Persch
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

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <list>
#include <memory>
#include <stack>
#include <sys/types.h>

namespace vte {

namespace base {

class Chunk {
        // A Chunk contains the raw data read from PTY.
        //
        // Data will be read in blocks and accumulated into chunks.
        // Chunks will be processed in (potentially) multiple
        // parts (by potentially multiple (sub)parsers).
        //
private:
        class Recycler {
        public:
                void operator()(Chunk* chunk) {
                        if (chunk == nullptr)
                                return;
                        chunk->recycle();
                }
        };

        void recycle() noexcept;

        static constexpr const unsigned k_max_free_chunks = 16u;
        static constexpr const unsigned k_chunk_size = 0x2000u - 2 * sizeof(void*);
        static constexpr const unsigned k_overlap_size = 1u;

        enum class Flags : uint8_t {
                eSEALED  = 1u << 0,
                eEOS     = 1u << 1,
                eCHAINED = 1u << 2,
        };

        uint8_t* m_data{nullptr};
        size_t m_capacity{0};
        size_t m_start{k_overlap_size};
        size_t m_size{k_overlap_size};
        uint8_t m_flags{0};

public:

        // Returns: pointer to the raw data storage (includes space for pre-begin data)
        inline constexpr uint8_t* data() const noexcept { return m_data; }

        // Returns: the storage capacity of data()
        inline constexpr auto capacity() const noexcept { return m_capacity; }

        // Returns: pointer to where to start reading available data (inside data())
        inline constexpr uint8_t const* begin_reading() const noexcept { assert(m_start <= m_size); return data() + m_start; }

        // Returns: pointer to the end of available data()
        inline constexpr uint8_t const* end_reading() const noexcept { return data() + m_size; };

        // Returns: how much data there is to read between begin_reading() and end_reading()
        inline constexpr auto size_reading() const noexcept { return m_size - m_start; }

        // Returns: whether there is any data to read
        inline constexpr bool has_reading() const noexcept { return begin_reading() < end_reading(); }

        // Sets the value returned by begin_reading(). To be used after
        // processing some data, so that the next round knows where to start.
        void set_begin_reading(uint8_t const* ptr) noexcept
        {
                assert(ptr >= data() &&
                       (!chained() || ptr > data()) &&
                       ptr <= data() + capacity());
                m_start = unsigned(ptr - data());
        }

        // Returns: pointer to buffer to write data into
        // Note that there is *always* at least one byte writable at begin_writing()-1
        // to be used when reading from a PTY in CPKT mode.
        inline constexpr uint8_t* begin_writing() const noexcept { assert(m_size > 0); return data() + m_size; }

        // Returns: size of begin_writing() buffer
        inline constexpr auto capacity_writing() const noexcept { return m_capacity - m_size; }

        // Add to chunk size. To be called after writing data to begin_writing().
        inline void add_size(ssize_t len)
        {
                assert(len >= 0 && size_t(len) <= capacity_writing());
                m_size += len;
        }

        // Chain this Chunk to some other Chunk.
        // If the other chunk isn't EOS, we
        // copy the last k_overlap_size byte(s) from it to the start of
        // the new chunk, and set the new chunk as chained.
        // This will allow rewinding the stream during processing,
        // without keeping the preceding chunk around.
        void chain(Chunk const* previous)
        {
                assert(m_size == k_overlap_size && m_start == m_size); // or call reset() here?

                if (!previous->eos()) {
                        std::memcpy(m_data,
                                    previous->m_data + previous->m_size - k_overlap_size,
                                    k_overlap_size);

                        set_chained();
                }
        }

        // Special-case operator new, so that we can allocate
        // the chunk data together with the instance.
        void* operator new(std::size_t count)
        {
                assert(count < k_chunk_size);
                return std::malloc(k_chunk_size);
        }

        // Special-case operator delete for pairing with operator new.
        void operator delete(void* ptr)
        {
                std::free(ptr);
        }

        // Type to use when storing a Chunk, so that chunks can be recycled.
        using unique_type = std::unique_ptr<Chunk, Recycler>;

        Chunk()
                : m_data{reinterpret_cast<uint8_t*>(this) + sizeof(*this)},
                  m_capacity{k_chunk_size - sizeof(*this)}
        {
                std::memset(m_data, 0, k_overlap_size);
        }

        Chunk(Chunk const&) = delete;
        Chunk(Chunk&&) = delete;
        ~Chunk() = default;

        Chunk& operator= (Chunk const&) = delete;
        Chunk& operator= (Chunk&&) = delete;

        // Resets the chunk. Reset chunks will not be rewindable!
        void reset() noexcept
        {
                std::memset(m_data, 0, k_overlap_size);
                m_start = m_size = k_overlap_size;
                m_flags = 0;
        }

        // Returns: a new or recycled Chunk
        static unique_type get(Chunk const* chain_to) noexcept;

        // Prune recycled chunks
        static void prune(unsigned int max_size = k_max_free_chunks) noexcept;

        // Returns: whether the chunk is sealed, i.e. must not be used
        // to write more data into
        inline constexpr bool sealed() const noexcept { return m_flags & (uint8_t)Flags::eSEALED; }

        // Seal the chunk
        inline void set_sealed() noexcept { m_flags |= (uint8_t)Flags::eSEALED; }

        // Returns: whether the chunk is an EOS (end-of-stream)
        inline constexpr bool eos() const noexcept { return m_flags & (uint8_t)Flags::eEOS; }

        // Set the chunk EOS
        inline void set_eos() noexcept { m_flags |= (uint8_t)Flags::eEOS; }

        // Returns: whether the chunk was chained to some other chunk
        // and thus m_start may be set to < k_overlap_size.
        inline constexpr bool chained() const noexcept { return m_flags & (uint8_t)Flags::eCHAINED; }

        // Set the chunk as chained
        inline void set_chained() noexcept { m_flags |= (uint8_t)Flags::eCHAINED; }

        // Get the maximum chunk size
        static inline constexpr unsigned max_size() noexcept { return k_chunk_size; }

private:

        /* Note that this is using the standard deleter, not Recycler */
        static std::stack<std::unique_ptr<Chunk>, std::list<std::unique_ptr<Chunk>>> g_free_chunks;
};

} // namespace base

} // namespace vte
