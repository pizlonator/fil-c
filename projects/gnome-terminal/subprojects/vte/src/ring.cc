/*
 * Copyright (C) 2002,2009,2010 Red Hat, Inc.
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
 *
 * Red Hat Author(s): Nalin Dahyabhai, Behdad Esfahbod
 */

#include "config.h"

#include "debug.h"
#include "ring.hh"
#include "vterowdata.hh"

#include <string.h>

/*
 * Copy the common attributes from VteCellAttr to VteStreamCellAttr or vice versa.
 */
static inline void
_attrcpy (void *dst, void *src)
{
        memcpy(dst, src, VTE_CELL_ATTR_COMMON_BYTES);
}

using namespace vte::base;

/*
 * VteRing: A buffer ring
 */

#if VTE_DEBUG
void
Ring::validate() const
{
	_vte_debug_print(VTE_DEBUG_RING,
			" Delta = %lu, Length = %lu, Next = %lu, Max = %lu, Writable = %lu.\n",
			m_start, m_end - m_start, m_end,
			m_max, m_end - m_writable);

	vte_assert_cmpuint(m_start, <=, m_writable);
	vte_assert_cmpuint(m_writable, <=, m_end);

	vte_assert_cmpuint(m_end - m_start, <=, m_max);
	vte_assert_cmpuint(m_end - m_writable, <=, m_mask);
}
#else
#define validate(...) do { } while(0)
#endif

Ring::Ring(row_t max_rows,
           bool has_streams)
        : m_max{MAX(max_rows, 3)},
          m_has_streams{has_streams},
          m_last_attr{basic_cell.attr}
{
	_vte_debug_print(VTE_DEBUG_RING, "New ring %p.\n", this);

	m_array = (VteRowData* ) g_malloc0 (sizeof (m_array[0]) * (m_mask + 1));

	if (has_streams) {
		m_attr_stream = _vte_file_stream_new ();
		m_text_stream = _vte_file_stream_new ();
		m_row_stream = _vte_file_stream_new ();
	} else {
		m_attr_stream = m_text_stream = m_row_stream = nullptr;
	}

	m_utf8_buffer = g_string_sized_new (128);

	_vte_row_data_init (&m_cached_row);

        m_hyperlinks = g_ptr_array_new();
        auto empty_str = g_string_new_len("", 0);
        g_ptr_array_add(m_hyperlinks, empty_str);

	validate();
}

Ring::~Ring()
{
	for (size_t i = 0; i <= m_mask; i++)
		_vte_row_data_fini (&m_array[i]);

	g_free (m_array);

	if (m_has_streams) {
		g_object_unref (m_attr_stream);
		g_object_unref (m_text_stream);
		g_object_unref (m_row_stream);
	}

	g_string_free (m_utf8_buffer, TRUE);

        for (size_t i = 0; i < m_hyperlinks->len; i++)
                g_string_free (hyperlink_get(i), TRUE);
        g_ptr_array_free (m_hyperlinks, TRUE);

	_vte_row_data_fini(&m_cached_row);
}

#define SET_BIT(buf, n) buf[(n) / 8] |= (1 << ((n) % 8))
#define GET_BIT(buf, n) ((buf[(n) / 8] >> ((n) % 8)) & 1)

/*
 * Do a round of garbage collection. Hyperlinks that no longer occur in the ring are wiped out.
 */
void
Ring::hyperlink_gc()
{
        row_t i, j;
        hyperlink_idx_t idx;
        VteRowData* row;
        char *used;

        _vte_debug_print (VTE_DEBUG_HYPERLINK,
                          "hyperlink: GC starting (highest used idx is %d)\n",
                          m_hyperlink_highest_used_idx);

        m_hyperlink_maybe_gc_counter = 0;

        if (m_hyperlink_highest_used_idx == 0) {
                _vte_debug_print (VTE_DEBUG_HYPERLINK,
                                  "hyperlink: GC done (no links at all, nothing to do)\n");
                return;
        }

        /* One bit for each idx to see if it's used. */
        used = (char *) g_malloc0 (m_hyperlink_highest_used_idx / 8 + 1);

        /* A few special values not to be garbage collected. */
        SET_BIT(used, m_hyperlink_current_idx);
        SET_BIT(used, m_hyperlink_hover_idx);
        SET_BIT(used, m_last_attr.hyperlink_idx);

        for (i = m_writable; i < m_end; i++) {
                row = get_writable_index(i);
                for (j = 0; j < row->len; j++) {
                        idx = row->cells[j].attr.hyperlink_idx;
                        SET_BIT(used, idx);
                }
        }

        for (idx = 1; idx <= m_hyperlink_highest_used_idx; idx++) {
                if (!GET_BIT(used, idx) && hyperlink_get(idx)->len != 0) {
                        _vte_debug_print (VTE_DEBUG_HYPERLINK,
                                          "hyperlink: GC purging link %d to id;uri=\"%s\"\n",
                                          idx, hyperlink_get(idx)->str);
                        /* Wipe out the ID and URI itself so it doesn't linger on in the memory for a long time */
                        memset(hyperlink_get(idx)->str, 0, hyperlink_get(idx)->len);
                        g_string_truncate (hyperlink_get(idx), 0);
                }
        }

        while (m_hyperlink_highest_used_idx >= 1 && hyperlink_get(m_hyperlink_highest_used_idx)->len == 0) {
               m_hyperlink_highest_used_idx--;
        }

        _vte_debug_print (VTE_DEBUG_HYPERLINK,
                          "hyperlink: GC done (highest used idx is now %d)\n",
                          m_hyperlink_highest_used_idx);

        g_free (used);
}

/*
 * Cumulate the given value, and do a GC when 65536 is reached.
 */
void
Ring::hyperlink_maybe_gc(row_t increment)
{
        m_hyperlink_maybe_gc_counter += increment;

        _vte_debug_print (VTE_DEBUG_HYPERLINK,
                          "hyperlink: maybe GC, counter at %ld\n",
                          m_hyperlink_maybe_gc_counter);

        if (m_hyperlink_maybe_gc_counter >= 65536)
                hyperlink_gc();
}

/*
 * Find existing idx for the hyperlink or allocate a new one.
 *
 * Returns 0 if given no hyperlink or an empty one, or if the pool is full.
 * Returns the idx (either already existing or newly allocated) from 1 up to
 * VTE_HYPERLINK_COUNT_MAX inclusive otherwise.
 *
 * FIXME do something more effective than a linear search
 */
Ring::hyperlink_idx_t
Ring::get_hyperlink_idx_no_update_current(char const* hyperlink)
{
        hyperlink_idx_t idx;
        gsize len;
        GString *str;

        if (!hyperlink || !hyperlink[0])
                return 0;

        len = strlen(hyperlink);

        /* Linear search for this particular URI */
        auto const last_idx = m_hyperlink_highest_used_idx + 1;
        for (idx = 1; idx < last_idx; ++idx) {
                if (strcmp(hyperlink_get(idx)->str, hyperlink) == 0) {
                        _vte_debug_print (VTE_DEBUG_HYPERLINK,
                                          "get_hyperlink_idx: already existing idx %d for id;uri=\"%s\"\n",
                                          idx, hyperlink);
                        return idx;
                }
        }

        /* FIXME it's the second time we're GCing if coming from get_hyperlink_idx */
        hyperlink_gc();

        /* Another linear search for an empty slot where a GString is already allocated */
        for (idx = 1; idx < m_hyperlinks->len; idx++) {
                if (hyperlink_get(idx)->len == 0) {
                        _vte_debug_print (VTE_DEBUG_HYPERLINK,
                                          "get_hyperlink_idx: reassigning old idx %d for id;uri=\"%s\"\n",
                                          idx, hyperlink);
                        /* Grow size if required, however, never shrink to avoid long-term memory fragmentation. */
                        g_string_append_len (hyperlink_get(idx), hyperlink, len);
                        m_hyperlink_highest_used_idx = MAX (m_hyperlink_highest_used_idx, idx);
                        return idx;
                }
        }

        /* All allocated slots are in use. Gotta allocate a new one */
        vte_assert_cmpuint(m_hyperlink_highest_used_idx + 1, ==, m_hyperlinks->len);

        /* VTE_HYPERLINK_COUNT_MAX should be big enough for this not to happen under
           normal circumstances. Anyway, it's cheap to protect against extreme ones. */
        if (m_hyperlink_highest_used_idx == VTE_HYPERLINK_COUNT_MAX) {
                _vte_debug_print (VTE_DEBUG_HYPERLINK,
                                  "get_hyperlink_idx: idx 0 (ran out of available idxs) for id;uri=\"%s\"\n",
                                  hyperlink);
                return 0;
        }

        idx = ++m_hyperlink_highest_used_idx;
        _vte_debug_print (VTE_DEBUG_HYPERLINK,
                          "get_hyperlink_idx: brand new idx %d for id;uri=\"%s\"\n",
                          idx, hyperlink);
        str = g_string_new_len (hyperlink, len);
        g_ptr_array_add(m_hyperlinks, str);

        vte_assert_cmpuint(m_hyperlink_highest_used_idx + 1, ==, m_hyperlinks->len);

        return idx;
}

/*
 * Find existing idx for the hyperlink or allocate a new one.
 *
 * Returns 0 if given no hyperlink or an empty one, or if the pool is full.
 * Returns the idx (either already existing or newly allocated) from 1 up to
 * VTE_HYPERLINK_COUNT_MAX inclusive otherwise.
 *
 * The current idx is also updated, in order not to be garbage collected.
 */
Ring::hyperlink_idx_t
Ring::get_hyperlink_idx(char const* hyperlink)
{
        /* Release current idx and do a round of GC to possibly purge its hyperlink,
         * even if new hyperlink is nullptr or empty. */
        m_hyperlink_current_idx = 0;
        hyperlink_gc();

        m_hyperlink_current_idx = get_hyperlink_idx_no_update_current(hyperlink);
        return m_hyperlink_current_idx;
}

void
Ring::freeze_row(row_t position,
                 VteRowData const* row)
{
	VteCell *cell;
	GString *buffer = m_utf8_buffer;
        GString *hyperlink;
	int i;
        gboolean froze_hyperlink = FALSE;

	_vte_debug_print (VTE_DEBUG_RING, "Freezing row %lu.\n", position);

        g_assert(m_has_streams);

	RowRecord record;
	memset(&record, 0, sizeof(record));
	record.text_start_offset = _vte_stream_head(m_text_stream);
	record.attr_start_offset = _vte_stream_head(m_attr_stream);
        record.width = row->len;
	record.is_ascii = 1;

	g_string_truncate (buffer, 0);
	for (i = 0, cell = row->cells; i < row->len; i++, cell++) {
		VteCellAttr attr;
		int num_chars;

		/* Attr storage:
		 *
		 * 1. We don't store attrs for fragments.  They can be
		 * reconstructed using the columns of their start cell.
		 *
		 * 2. We store one attr per vteunistr character starting
		 * from the second character, with columns=0.
		 *
		 * That's enough to reconstruct the attrs, and to store
		 * the text in real UTF-8.
		 */
		attr = cell->attr;
		if (G_LIKELY (!attr.fragment())) {
			CellAttrChange attr_change;
                        guint16 hyperlink_length;

			if (memcmp(&m_last_attr, &attr, sizeof (VteCellAttr)) != 0) {
				m_last_attr_text_start_offset = record.text_start_offset + buffer->len;
				memset(&attr_change, 0, sizeof (attr_change));
				attr_change.text_end_offset = m_last_attr_text_start_offset;
                                _attrcpy(&attr_change.attr, &m_last_attr);
                                hyperlink = hyperlink_get(m_last_attr.hyperlink_idx);
                                attr_change.attr.hyperlink_length = hyperlink->len;
				_vte_stream_append (m_attr_stream, (char const* ) &attr_change, sizeof (attr_change));
                                if (G_UNLIKELY (hyperlink->len != 0)) {
                                        _vte_stream_append (m_attr_stream, hyperlink->str, hyperlink->len);
                                        froze_hyperlink = TRUE;
                                }
                                hyperlink_length = attr_change.attr.hyperlink_length;
                                _vte_stream_append (m_attr_stream, (char const* ) &hyperlink_length, 2);
				if (!buffer->len)
					/* This row doesn't use last_attr, adjust */
                                        record.attr_start_offset += sizeof (attr_change) + hyperlink_length + 2;
				m_last_attr = attr;
			}

			num_chars = _vte_unistr_strlen (cell->c);
			if (num_chars > 1) {
                                /* Combining chars */
				attr.set_columns(0);
				m_last_attr_text_start_offset = record.text_start_offset + buffer->len
								  + g_unichar_to_utf8 (_vte_unistr_get_base (cell->c), nullptr);
				memset(&attr_change, 0, sizeof (attr_change));
				attr_change.text_end_offset = m_last_attr_text_start_offset;
                                _attrcpy(&attr_change.attr, &m_last_attr);
                                hyperlink = hyperlink_get(m_last_attr.hyperlink_idx);
                                attr_change.attr.hyperlink_length = hyperlink->len;
				_vte_stream_append (m_attr_stream, (char const* ) &attr_change, sizeof (attr_change));
                                if (G_UNLIKELY (hyperlink->len != 0)) {
                                        _vte_stream_append (m_attr_stream, hyperlink->str, hyperlink->len);
                                        froze_hyperlink = TRUE;
                                }
                                hyperlink_length = attr_change.attr.hyperlink_length;
                                _vte_stream_append (m_attr_stream, (char const* ) &hyperlink_length, 2);
				m_last_attr = attr;
			}

			if (cell->c < 32 || cell->c > 126) record.is_ascii = 0;
			_vte_unistr_append_to_string (cell->c, buffer);
		}
	}
	if (!row->attr.soft_wrapped)
		g_string_append_c (buffer, '\n');
	record.soft_wrapped = row->attr.soft_wrapped;
        record.bidi_flags = row->attr.bidi_flags;

	_vte_stream_append(m_text_stream, buffer->str, buffer->len);
	append_row_record(&record, position);

        /* After freezing some hyperlinks, do a hyperlink GC. The constant is totally arbitrary, feel free to fine tune. */
        if (froze_hyperlink)
                hyperlink_maybe_gc(1024);
}

/* If do_truncate (data is placed back from the stream to the ring), real new hyperlink idxs are looked up or allocated.
 *
 * If !do_truncate (data is fetched only to be displayed), hyperlinked cells are given the pseudo idx VTE_HYPERLINK_IDX_TARGET_IN_STREAM,
 * except for the hyperlink_hover_idx which gets this real idx. This is important for hover underlining.
 *
 * Optionally updates the hyperlink parameter to point to the ring-owned hyperlink target. */
void
Ring::thaw_row(row_t position,
               VteRowData* row,
               bool do_truncate,
               int hyperlink_column,
               char const** hyperlink)
{
	RowRecord records[2], record;
	VteCellAttr attr;
	CellAttrChange attr_change;
	VteCell cell;
	char const* p, *q, *end;
	GString *buffer = m_utf8_buffer;
        char hyperlink_readbuf[VTE_HYPERLINK_TOTAL_LENGTH_MAX + 1];

        hyperlink_readbuf[0] = '\0';
        if (hyperlink) {
                m_hyperlink_buf[0] = '\0';
                *hyperlink = m_hyperlink_buf;
        }

	_vte_debug_print (VTE_DEBUG_RING, "Thawing row %lu.\n", position);

        g_assert(m_has_streams);

	_vte_row_data_clear (row);

	attr_change.text_end_offset = 0;

	if (!read_row_record(&records[0], position))
		return;
	if ((position + 1) * sizeof (records[0]) < _vte_stream_head (m_row_stream)) {
		if (!read_row_record(&records[1], position + 1))
			return;
	} else
		records[1].text_start_offset = _vte_stream_head (m_text_stream);

	g_string_set_size (buffer, records[1].text_start_offset - records[0].text_start_offset);
	if (!_vte_stream_read (m_text_stream, records[0].text_start_offset, buffer->str, buffer->len))
		return;

	record = records[0];

	if (G_LIKELY (buffer->len && buffer->str[buffer->len - 1] == '\n'))
                g_string_truncate (buffer, buffer->len - 1);
	else
		row->attr.soft_wrapped = TRUE;
        row->attr.bidi_flags = records[0].bidi_flags;

	p = buffer->str;
	end = p + buffer->len;
	while (p < end) {
		if (record.text_start_offset >= m_last_attr_text_start_offset) {
			attr = m_last_attr;
                        strcpy(hyperlink_readbuf, hyperlink_get(attr.hyperlink_idx)->str);
		} else {
			if (record.text_start_offset >= attr_change.text_end_offset) {
				if (!_vte_stream_read (m_attr_stream, record.attr_start_offset, (char *) &attr_change, sizeof (attr_change)))
					return;
				record.attr_start_offset += sizeof (attr_change);
                                vte_assert_cmpuint (attr_change.attr.hyperlink_length, <=, VTE_HYPERLINK_TOTAL_LENGTH_MAX);
                                if (attr_change.attr.hyperlink_length && !_vte_stream_read (m_attr_stream, record.attr_start_offset, hyperlink_readbuf, attr_change.attr.hyperlink_length))
                                        return;
                                hyperlink_readbuf[attr_change.attr.hyperlink_length] = '\0';
                                record.attr_start_offset += attr_change.attr.hyperlink_length + 2;

                                _attrcpy(&attr, &attr_change.attr);
                                attr.hyperlink_idx = 0;
                                if (G_UNLIKELY (attr_change.attr.hyperlink_length)) {
                                        if (do_truncate) {
                                                /* Find the existing idx or allocate a new one, just as when receiving an OSC 8 escape sequence.
                                                 * Do not update the current idx though. */
                                                attr.hyperlink_idx = get_hyperlink_idx_no_update_current(hyperlink_readbuf);
                                        } else {
                                                /* Use a special hyperlink idx, except if to be underlined because the hyperlink is the same as the hovered cell's. */
                                                attr.hyperlink_idx = VTE_HYPERLINK_IDX_TARGET_IN_STREAM;
                                                if (m_hyperlink_hover_idx != 0 && strcmp(hyperlink_readbuf, hyperlink_get(m_hyperlink_hover_idx)->str) == 0) {
                                                        /* FIXME here we're calling the expensive strcmp() above and get_hyperlink_idx_no_update_current() way too many times. */
                                                        attr.hyperlink_idx = get_hyperlink_idx_no_update_current(hyperlink_readbuf);
                                                }
                                        }
                                }
			}
		}

		cell.attr = attr;
                _VTE_DEBUG_IF(VTE_DEBUG_RING | VTE_DEBUG_HYPERLINK) {
                        /* Debug: Reverse the colors for the stream's contents. */
                        if (!do_truncate) {
                                cell.attr.attr ^= VTE_ATTR_REVERSE;
                        }
                }
		cell.c = g_utf8_get_char (p);

		q = g_utf8_next_char (p);
		record.text_start_offset += q - p;
		p = q;

		if (G_UNLIKELY (cell.attr.columns() == 0)) {
			if (G_LIKELY (row->len)) {
				/* Combine it */
				row->cells[row->len - 1].c = _vte_unistr_append_unichar (row->cells[row->len - 1].c, cell.c);
                                /* Spread it to all the previous cells of a potentially multicell character */
                                for (int i = row->len - 1; i >= 1 && row->cells[i].attr.fragment(); i--) {
                                        row->cells[i - 1].c = row->cells[i].c;
                                }
			} else {
				cell.attr.set_columns(1);
                                if (row->len == hyperlink_column && hyperlink != nullptr)
                                        *hyperlink = strcpy(m_hyperlink_buf, hyperlink_readbuf);
				_vte_row_data_append (row, &cell);
			}
		} else {
                        if (row->len == hyperlink_column && hyperlink != nullptr)
                                *hyperlink = strcpy(m_hyperlink_buf, hyperlink_readbuf);
			_vte_row_data_append (row, &cell);
			if (cell.attr.columns() > 1) {
				/* Add the fragments */
				int i, columns = cell.attr.columns();
				cell.attr.set_fragment(true);
				cell.attr.set_columns(1);
                                for (i = 1; i < columns; i++) {
                                        if (row->len == hyperlink_column && hyperlink != nullptr)
                                                *hyperlink = strcpy(m_hyperlink_buf, hyperlink_readbuf);
					_vte_row_data_append (row, &cell);
                                }
			}
		}
	}

        /* FIXME this is extremely complicated (by design), figure out something better.
           This is the only place where we need to walk backwards in attr_stream,
           which is the reason for the hyperlink's length being repeated after the hyperlink itself. */
	if (do_truncate) {
		gsize attr_stream_truncate_at = records[0].attr_start_offset;
		_vte_debug_print (VTE_DEBUG_RING, "Truncating\n");
		if (records[0].text_start_offset <= m_last_attr_text_start_offset) {
			/* Check the previous attr record. If its text ends where truncating, this attr record also needs to be removed. */
                        guint16 hyperlink_length;
                        if (_vte_stream_read (m_attr_stream, attr_stream_truncate_at - 2, (char *) &hyperlink_length, 2)) {
                                vte_assert_cmpuint (hyperlink_length, <=, VTE_HYPERLINK_TOTAL_LENGTH_MAX);
                                if (_vte_stream_read (m_attr_stream, attr_stream_truncate_at - 2 - hyperlink_length - sizeof (attr_change), (char *) &attr_change, sizeof (attr_change))) {
                                        if (records[0].text_start_offset == attr_change.text_end_offset) {
                                                _vte_debug_print (VTE_DEBUG_RING, "... at attribute change\n");
                                                attr_stream_truncate_at -= sizeof (attr_change) + hyperlink_length + 2;
                                        }
				}
			}
			/* Reconstruct last_attr from the first record of attr_stream that we cut off,
			   last_attr_text_start_offset from the last record that we keep. */
			if (_vte_stream_read (m_attr_stream, attr_stream_truncate_at, (char *) &attr_change, sizeof (attr_change))) {
                                _attrcpy(&m_last_attr, &attr_change.attr);
                                m_last_attr.hyperlink_idx = 0;
                                if (attr_change.attr.hyperlink_length && _vte_stream_read (m_attr_stream, attr_stream_truncate_at + sizeof (attr_change), (char *) &hyperlink_readbuf, attr_change.attr.hyperlink_length)) {
                                        hyperlink_readbuf[attr_change.attr.hyperlink_length] = '\0';
                                        m_last_attr.hyperlink_idx = get_hyperlink_idx(hyperlink_readbuf);
                                }
                                if (_vte_stream_read (m_attr_stream, attr_stream_truncate_at - 2, (char *) &hyperlink_length, 2)) {
                                        vte_assert_cmpuint (hyperlink_length, <=, VTE_HYPERLINK_TOTAL_LENGTH_MAX);
                                        if (_vte_stream_read (m_attr_stream, attr_stream_truncate_at - 2 - hyperlink_length - sizeof (attr_change), (char *) &attr_change, sizeof (attr_change))) {
                                                m_last_attr_text_start_offset = attr_change.text_end_offset;
                                        } else {
                                                m_last_attr_text_start_offset = 0;
                                        }
				} else {
					m_last_attr_text_start_offset = 0;
				}
			} else {
				m_last_attr_text_start_offset = 0;
				m_last_attr = basic_cell.attr;
			}
		}
		_vte_stream_truncate (m_row_stream, position * sizeof (record));
		_vte_stream_truncate (m_attr_stream, attr_stream_truncate_at);
		_vte_stream_truncate (m_text_stream, records[0].text_start_offset);
	}
}

void
Ring::reset_streams(row_t position)
{
	_vte_debug_print (VTE_DEBUG_RING, "Reseting streams to %lu.\n", position);

	if (m_has_streams) {
		_vte_stream_reset(m_row_stream, position * sizeof(RowRecord));
                _vte_stream_reset(m_text_stream, _vte_stream_head(m_text_stream));
                _vte_stream_reset(m_attr_stream, _vte_stream_head(m_attr_stream));
	}

	m_last_attr_text_start_offset = 0;
	m_last_attr = basic_cell.attr;
}

Ring::row_t
Ring::reset()
{
        _vte_debug_print (VTE_DEBUG_RING, "Reseting the ring at %lu.\n", m_end);

        reset_streams(m_end);
        m_start = m_writable = m_end;
        m_cached_row_num = (row_t)-1;

        return m_end;
}

VteRowData const*
Ring::index(row_t position)
{
	if (G_LIKELY (position >= m_writable))
		return get_writable_index(position);

	if (m_cached_row_num != position) {
		_vte_debug_print(VTE_DEBUG_RING, "Caching row %lu.\n", position);
                thaw_row(position, &m_cached_row, false, -1, nullptr);
		m_cached_row_num = position;
	}

	return &m_cached_row;
}

bool
Ring::is_soft_wrapped(row_t position)
{
        const VteRowData *row;
        RowRecord record;

        if (G_UNLIKELY (position < m_start || position >= m_end))
                return false;

        if (G_LIKELY (position >= m_writable)) {
                row = get_writable_index(position);
                return row->attr.soft_wrapped;
        }

        /* The row is scrolled out to the stream. Save work by not reading the actual row.
         * The requested information is readily available in row_stream, too. */
        if (G_UNLIKELY (!read_row_record(&record, position)))
                return false;
        return record.soft_wrapped;
}

/* Returns whether the given visual row contains the beginning of a prompt, i.e.
 * contains a prompt character which is immediately preceded by either a hard newline
 * or a non-prompt character (possibly at the end of previous, soft wrapped row).
 *
 * This way we catch soft wrapped multiline prompts at their first line only,
 * and catch prompts that do not begin at the beginning of a row.
 *
 * FIXME extend support for deliberately multiline (hard wrapped) prompts:
 * https://gitlab.gnome.org/GNOME/vte/-/issues/2681#note_1904004
 *
 * FIXME this is very slow, it unnecessarily reads text_stream
 * in which we're not interested at all. Implement a faster algorithm. */
bool
Ring::contains_prompt_beginning(row_t position)
{
        const VteRowData *row = index(position);
        if (row == NULL || row->len == 0) {
                return false;
        }

        /* First check the places where the previous character is also readily available. */
        int col = 0;
        while (col < row->len && row->cells[col].attr.shellintegration() == ShellIntegrationMode::ePROMPT) {
                col++;
        }
        while (col < row->len && row->cells[col].attr.shellintegration() != ShellIntegrationMode::ePROMPT) {
                col++;
        }
        if (col < row->len) {
                return true;
        }

        /* Finally check the first character where we might need to look at the previous row. */
        if (row->cells[0].attr.shellintegration() == ShellIntegrationMode::ePROMPT) {
                row = index(position - 1);
                if (row == NULL ||
                    !row->attr.soft_wrapped ||
                    (row->len >= 1 /* this is guaranteed beucase soft_wrapped */ &&
                     row->cells[row->len - 1].attr.shellintegration() != ShellIntegrationMode::ePROMPT)) {
                        return true;
                }
        }
        return false;
}

/*
 * Returns the hyperlink idx at the given position.
 *
 * Updates the hyperlink parameter to point to the hyperlink's target.
 * The buffer is owned by the ring and must not be modified by the caller.
 *
 * Optionally also updates the internal concept of the hovered idx. In this case,
 * a real idx is looked up or newly allocated in the hyperlink pool even if the
 * cell is scrolled out to the streams.
 * This is to be able to underline all cells that share the same hyperlink.
 *
 * Otherwise cells from the stream might get the pseudo idx VTE_HYPERLINK_IDX_TARGET_IN_STREAM.
 */
Ring::hyperlink_idx_t
Ring::get_hyperlink_at_position(row_t position,
                                column_t col,
                                bool update_hover_idx,
                                char const** hyperlink)
{
        hyperlink_idx_t idx;
        char const* hp;

        if (hyperlink == nullptr)
                hyperlink = &hp;
        *hyperlink = nullptr;

        if (update_hover_idx) {
                /* Invalidate the cache because new hover idx might result in new idxs to report. */
                m_cached_row_num = (row_t)-1;
        }

        if (G_UNLIKELY (!contains(position) || col < 0)) {
                if (update_hover_idx)
                        m_hyperlink_hover_idx = 0;
                return 0;
        }

        if (G_LIKELY (position >= m_writable)) {
                VteRowData* row = get_writable_index(position);
                if (col >= _vte_row_data_length(row)) {
                        if (update_hover_idx)
                                m_hyperlink_hover_idx = 0;
                        return 0;
                }
                *hyperlink = hyperlink_get(row->cells[col].attr.hyperlink_idx)->str;
                idx = row->cells[col].attr.hyperlink_idx;
        } else {
                thaw_row(position, &m_cached_row, false, col, hyperlink);
                /* Note: Intentionally don't set cached_row_num. We're about to update
                 * m_hyperlink_hover_idx which makes some idxs no longer valid. */
                idx = get_hyperlink_idx_no_update_current(*hyperlink);
        }
        if (**hyperlink == '\0')
                *hyperlink = nullptr;
        if (update_hover_idx)
                m_hyperlink_hover_idx = idx;
        return idx;
}

void
Ring::freeze_one_row()
{
	VteRowData* row;

	if (G_UNLIKELY (m_writable == m_start))
		reset_streams(m_writable);

	row = get_writable_index(m_writable);
	freeze_row(m_writable, row);

	m_writable++;
}

void
Ring::thaw_one_row()
{
	VteRowData* row;

	vte_assert_cmpuint(m_start, <, m_writable);

	ensure_writable_room();

	m_writable--;

	if (m_writable == m_cached_row_num)
		m_cached_row_num = (row_t)-1; /* Invalidate cached row */

	row = get_writable_index(m_writable);
        thaw_row(m_writable, row, true, -1, nullptr);
}

void
Ring::discard_one_row()
{
	m_start++;
	if (G_UNLIKELY(m_start == m_writable)) {
		reset_streams(m_writable);
	} else if (m_start < m_writable) {
                /* Advance the tail sometimes. Not always, in order to slightly improve performance. */
                if (m_start % 256 == 0) {
                        RowRecord record;
                        _vte_stream_advance_tail(m_row_stream, m_start * sizeof (record));
                        if (G_LIKELY(read_row_record(&record, m_start))) {
                                _vte_stream_advance_tail(m_text_stream, record.text_start_offset);
                                _vte_stream_advance_tail(m_attr_stream, record.attr_start_offset);
                        }
                }
	} else {
		m_writable = m_start;
	}
}

void
Ring::maybe_freeze_one_row()
{
        /* See the comment about m_visible_rows + 1 at ensure_writable_room(). */
        if (G_LIKELY(m_mask >= m_visible_rows + 1 &&
                     m_writable + m_mask + 1 == m_end))
		freeze_one_row();
	else
		ensure_writable_room();
}

//FIXMEchpe maybe inline this one
void
Ring::maybe_discard_one_row()
{
	if (length() == m_max)
		discard_one_row();
}

void
Ring::ensure_writable_room()
{
	row_t new_mask, old_mask, i, end;
	VteRowData* old_array, *new_array;;

        /* Keep at least m_visible_rows + 1 rows in the ring.
         * The BiDi spec requires that the just scrolled out row
         * is still alterable (can be switched to hard line ending).
         * It's nice anyway to make that hard wrapped upon a clear. */
        if (G_LIKELY(m_mask >= m_visible_rows + 1 &&
                     m_writable + m_mask + 1 > m_end))
		return;

	old_mask = m_mask;
	old_array = m_array;

	do {
		m_mask = (m_mask << 1) + 1;
        } while (m_mask < m_visible_rows + 1 || m_writable + m_mask + 1 <= m_end);

	_vte_debug_print(VTE_DEBUG_RING, "Enlarging writable array from %lu to %lu\n", old_mask, m_mask);

	m_array = (VteRowData* ) g_malloc0(sizeof (m_array[0]) * (m_mask + 1));

	new_mask = m_mask;
	new_array = m_array;

	end = m_writable + old_mask + 1;
	for (i = m_writable; i < end; i++)
		new_array[i & new_mask] = old_array[i & old_mask];

	g_free (old_array);
}

/**
 * Ring::resize:
 * @max_rows: new maximum numbers of rows in the ring
 *
 * Changes the number of lines the ring can contain.
 */
void
Ring::resize(row_t max_rows)
{
	_vte_debug_print(VTE_DEBUG_RING, "Resizing to %lu.\n", max_rows);

	validate();

	/* Adjust the start of tail chunk now */
	if (length() > max_rows) {
		m_start = m_end - max_rows;
		if (m_start >= m_writable) {
			reset_streams(m_writable);
			m_writable = m_start;
		}
	}

	m_max = max_rows;
}

void
Ring::shrink(row_t max_len)
{
	if (length() <= max_len)
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Shrinking to %lu.\n", max_len);

	validate();

	if (m_writable - m_start <= max_len)
		m_end = m_start + max_len;
	else {
		while (m_writable - m_start > max_len) {
			ensure_writable(m_writable - 1);
			m_end = m_writable;
		}
	}

	/* TODO May want to shrink down m_array */

	validate();
}

/**
 * Ring::insert:
 * @position: an index
 *
 * Inserts a new, empty, row into @ring at the @position'th offset.
 * The item at that position and any items after that are shifted down.
 *
 * Return: the newly added row.
 */
VteRowData*
Ring::insert(row_t position, guint8 bidi_flags)
{
	row_t i;
	VteRowData* row, tmp;

	_vte_debug_print(VTE_DEBUG_RING, "Inserting at position %lu.\n", position);
	validate();

	maybe_discard_one_row();
	ensure_writable(position);
	ensure_writable_room();

	vte_assert_cmpuint (position, >=, m_writable);
	vte_assert_cmpuint (position, <=, m_end);

        //FIXMEchpe WTF use better data structures!
	tmp = *get_writable_index(m_end);
	for (i = m_end; i > position; i--)
		*get_writable_index(i) = *get_writable_index(i - 1);
	*get_writable_index(position) = tmp;

	row = get_writable_index(position);
	_vte_row_data_clear (row);
        row->attr.bidi_flags = bidi_flags;
	m_end++;

	maybe_freeze_one_row();
        validate();
	return row;
}

/**
 * Ring::remove:
 * @position: an index
 *
 * Removes the @position'th item from @ring.
 */
void
Ring::remove(row_t position)
{
	row_t i;
	VteRowData tmp;

	_vte_debug_print(VTE_DEBUG_RING, "Removing item at position %lu.\n", position);
        validate();

	if (G_UNLIKELY(!contains(position)))
		return;

	ensure_writable(position);

        //FIXMEchpe WTF as above
	tmp = *get_writable_index(position);
	for (i = position; i < m_end - 1; i++)
		*get_writable_index(i) = *get_writable_index(i + 1);
	*get_writable_index(m_end - 1) = tmp;

	if (m_end > m_writable)
		m_end--;

        validate();
}


/**
 * Ring::append:
 * @data: the new item
 *
 * Appends a new item to the ring.
 *
 * Return: the newly added row.
 */
VteRowData*
Ring::append(guint8 bidi_flags)
{
        return insert(next(), bidi_flags);
}


/**
 * Ring::drop_scrollback:
 * @position: drop contents up to this point, which must be in the writable region.
 *
 * Drop the scrollback (offscreen contents).
 *
 * TODOegmont: We wouldn't need the position argument after addressing 708213#c29.
 */
void
Ring::drop_scrollback(row_t position)
{
        ensure_writable(position);

        m_start = m_writable = position;
        reset_streams(position);
}

/**
 * Ring::set_visible_rows:
 * @rows: the number of visible rows
 *
 * Set the number of visible rows.
 * It's required to be set correctly for the alternate screen so that it
 * never hits the streams. It's also required for clearing the scrollback.
 */
void
Ring::set_visible_rows(row_t rows)
{
        m_visible_rows = rows;
}


/* Convert a (row,col) into a CellTextOffset.
 * Requires the row to be frozen, or be outsize the range covered by the ring.
 */
bool
Ring::frozen_row_column_to_text_offset(row_t position,
				       column_t column,
				       CellTextOffset* offset)
{
	RowRecord records[2];
	VteCell *cell;
	GString *buffer = m_utf8_buffer;
	VteRowData const* row;
	unsigned int i, num_chars, off;

	if (position >= m_end) {
		offset->text_offset = _vte_stream_head(m_text_stream) + position - m_end;
		offset->fragment_cells = 0;
		offset->eol_cells = column;
		return true;
	}

	if (G_UNLIKELY (position < m_start)) {
		/* This happens when the marker (saved cursor position) is
		   scrolled off at the top of the scrollback buffer. */
		position = m_start;
		column = 0;
		/* go on */
	}

	vte_assert_cmpuint(position, <, m_writable);
	if (!read_row_record(&records[0], position))
		return false;
	if ((position + 1) * sizeof (records[0]) < _vte_stream_head(m_row_stream)) {
		if (!read_row_record(&records[1], position + 1))
			return false;
	} else
		records[1].text_start_offset = _vte_stream_head(m_text_stream);

	g_string_set_size (buffer, records[1].text_start_offset - records[0].text_start_offset);
	if (!_vte_stream_read(m_text_stream, records[0].text_start_offset, buffer->str, buffer->len))
		return false;

	if (G_LIKELY (buffer->len && buffer->str[buffer->len - 1] == '\n'))
                g_string_truncate (buffer, buffer->len - 1);

	row = index(position);

	/* row and buffer now contain the same text, in different representation */

	/* count the number of characters up to the given column */
	offset->fragment_cells = 0;
	offset->eol_cells = -1;
	num_chars = 0;
	for (i = 0, cell = row->cells; i < row->len && i < column; i++, cell++) {
		if (G_LIKELY (!cell->attr.fragment())) {
			if (G_UNLIKELY (i + cell->attr.columns() > column)) {
				offset->fragment_cells = column - i;
				break;
			}
			num_chars += _vte_unistr_strlen(cell->c);
		}
	}
	if (i >= row->len) {
		offset->eol_cells = column - i;
	}

	/* count the number of UTF-8 bytes for the given number of characters */
	off = 0;
	while (num_chars > 0 && off < buffer->len) {
		off++;
		if ((buffer->str[off] & 0xC0) != 0x80) num_chars--;
	}
	offset->text_offset = records[0].text_start_offset + off;
	return true;
}


/* Given a row number and a CellTextOffset, compute the column within that row.
   It's the caller's responsibility to ensure that CellTextOffset really falls into that row.
   Requires the row to be frozen, or be outsize the range covered by the ring.
 */
bool
Ring::frozen_row_text_offset_to_column(row_t position,
				       CellTextOffset const* offset,
				       column_t* column)
{
	RowRecord records[2];
	VteCell *cell;
	GString *buffer = m_utf8_buffer;
	VteRowData const* row;
	unsigned int i, off, num_chars, nc;

	if (position >= m_end) {
		*column = offset->eol_cells;
		return true;
	}

	if (G_UNLIKELY (position < m_start)) {
		/* This happens when the marker (saved cursor position) is
		   scrolled off at the top of the scrollback buffer. */
		*column = 0;
		return true;
	}

	vte_assert_cmpuint(position, <, m_writable);
	if (!read_row_record(&records[0], position))
		return false;
	if ((position + 1) * sizeof (records[0]) < _vte_stream_head(m_row_stream)) {
		if (!read_row_record(&records[1], position + 1))
			return false;
	} else
		records[1].text_start_offset = _vte_stream_head (m_text_stream);

	g_string_set_size (buffer, records[1].text_start_offset - records[0].text_start_offset);
	if (!_vte_stream_read(m_text_stream, records[0].text_start_offset, buffer->str, buffer->len))
		return false;

	if (G_LIKELY (buffer->len && buffer->str[buffer->len - 1] == '\n'))
                g_string_truncate (buffer, buffer->len - 1);

        /* Now that we've chopped off the likely trailing newline (which is only rarely missing,
         * if the ring ends in a soft wrapped line; see bug 181), the position we're about to
         * locate can be anywhere in the string, including just after its last character,
         * but not beyond that. */
        vte_assert_cmpuint(offset->text_offset, >=, records[0].text_start_offset);
        vte_assert_cmpuint(offset->text_offset, <=, records[0].text_start_offset + buffer->len);

	row = index(position);

	/* row and buffer now contain the same text, in different representation */

	/* count the number of characters for the given UTF-8 text offset */
	off = offset->text_offset - records[0].text_start_offset;
	num_chars = 0;
	for (i = 0; i < off; i++) {
		if ((buffer->str[i] & 0xC0) != 0x80) num_chars++;
	}

	/* count the number of columns for the given number of characters */
	for (i = 0, cell = row->cells; i < row->len; i++, cell++) {
		if (G_LIKELY (!cell->attr.fragment())) {
			if (num_chars == 0) break;
			nc = _vte_unistr_strlen(cell->c);
			if (nc > num_chars) break;
			num_chars -= nc;
		}
	}

	/* always add fragment_cells, but add eol_cells only if we're at eol */
	i += offset->fragment_cells;
	if (G_UNLIKELY (offset->eol_cells >= 0 && i == row->len))
		i += offset->eol_cells;
	*column = i;
	return true;
}


/**
 * Ring::rewrap:
 * @columns: new number of columns
 * @markers: 0-terminated array of #VteVisualPosition
 *
 * Reflow the @ring to match the new number of @columns.
 * For all @markers, find the cell at that position and update them to
 * reflect the cell's new position.
 */
/* See ../doc/rewrap.txt for design and implementation details. */
void
Ring::rewrap(column_t columns,
             VteVisualPosition** markers)
{
	row_t old_row_index, new_row_index;
	int i;
	int num_markers = 0;
	CellTextOffset *marker_text_offsets;
	VteVisualPosition *new_markers;
	RowRecord old_record;
	CellAttrChange attr_change;
	VteStream *new_row_stream;
	gsize paragraph_start_text_offset;
	gsize paragraph_end_text_offset;
	gsize paragraph_len;  /* excluding trailing '\n' */
	gsize attr_offset;
	gsize old_ring_end;

	if (G_UNLIKELY(length() == 0))
		return;
	_vte_debug_print(VTE_DEBUG_RING, "Ring before rewrapping:\n");
        validate();
	new_row_stream = _vte_file_stream_new();

	/* Freeze everything, because rewrapping is really complicated and we don't want to
	   duplicate the code for frozen and thawed rows. */
	while (m_writable < m_end)
		freeze_one_row();

	/* For markers given as (row,col) pairs find their offsets in the text stream.
	   This code requires that the rows are already frozen. */
	while (markers[num_markers] != nullptr)
		num_markers++;
	marker_text_offsets = (CellTextOffset *) g_malloc(num_markers * sizeof (marker_text_offsets[0]));
	new_markers = (VteVisualPosition *) g_malloc(num_markers * sizeof (new_markers[0]));
	for (i = 0; i < num_markers; i++) {
		/* Convert visual column into byte offset */
		if (!frozen_row_column_to_text_offset(markers[i]->row, markers[i]->col, &marker_text_offsets[i]))
			goto err;
		new_markers[i].row = new_markers[i].col = -1;
		_vte_debug_print(VTE_DEBUG_RING,
				"Marker #%d old coords:  row %ld  col %ld  ->  text_offset %" G_GSIZE_FORMAT " fragment_cells %d  eol_cells %d\n",
				i, markers[i]->row, markers[i]->col, marker_text_offsets[i].text_offset,
				marker_text_offsets[i].fragment_cells, marker_text_offsets[i].eol_cells);
	}

	/* Prepare for rewrapping */
	if (!read_row_record(&old_record, m_start))
		goto err;
	paragraph_start_text_offset = old_record.text_start_offset;
	paragraph_end_text_offset = _vte_stream_head(m_text_stream);  /* initialized to silence gcc */
	new_row_index = 0;

	attr_offset = old_record.attr_start_offset;
	if (!_vte_stream_read(m_attr_stream, attr_offset, (char *) &attr_change, sizeof (attr_change))) {
                _attrcpy(&attr_change.attr, &m_last_attr);
                attr_change.attr.hyperlink_length = hyperlink_get(m_last_attr.hyperlink_idx)->len;
		attr_change.text_end_offset = _vte_stream_head(m_text_stream);
	}

	old_row_index = m_start + 1;
	while (paragraph_start_text_offset < _vte_stream_head(m_text_stream)) {
		/* Find the boundaries of the next paragraph */
                gsize paragraph_width = 0;
		gboolean prev_record_was_soft_wrapped = FALSE;
		gboolean paragraph_is_ascii = TRUE;
                guint8 paragraph_bidi_flags = old_record.bidi_flags;
		gsize text_offset = paragraph_start_text_offset;
		RowRecord new_record;
		column_t col = 0;

		_vte_debug_print(VTE_DEBUG_RING,
				"  Old paragraph:  row %lu  (text_offset %" G_GSIZE_FORMAT ")  up to (exclusive)  ",  /* no '\n' */
                                 old_row_index - 1,
                                 paragraph_start_text_offset);
		while (old_row_index <= m_end) {
                        paragraph_width += old_record.width;
			prev_record_was_soft_wrapped = old_record.soft_wrapped;
			paragraph_is_ascii = paragraph_is_ascii && old_record.is_ascii;
			if (G_LIKELY (old_row_index < m_end)) {
				if (!read_row_record(&old_record, old_row_index))
					goto err;
				paragraph_end_text_offset = old_record.text_start_offset;
			} else {
				paragraph_end_text_offset = _vte_stream_head (m_text_stream);
			}
			old_row_index++;
			if (!prev_record_was_soft_wrapped)
				break;
		}

		paragraph_len = paragraph_end_text_offset - paragraph_start_text_offset;
		if (!prev_record_was_soft_wrapped)  /* The last paragraph can be soft wrapped! */
			paragraph_len--;  /* Strip trailing '\n' */
		_vte_debug_print(VTE_DEBUG_RING,
				"row %lu  (text_offset %" G_GSIZE_FORMAT ")%s  len %" G_GSIZE_FORMAT "  is_ascii %d\n",
                                 old_row_index - 1,
                                 paragraph_end_text_offset,
				prev_record_was_soft_wrapped ? "  soft_wrapped" : "",
				paragraph_len, paragraph_is_ascii);
		/* Wrap the paragraph */
		if (attr_change.text_end_offset <= text_offset) {
			/* Attr change at paragraph boundary, advance to next attr. */
                        attr_offset += sizeof (attr_change) + attr_change.attr.hyperlink_length + 2;
			if (!_vte_stream_read(m_attr_stream, attr_offset, (char *) &attr_change, sizeof (attr_change))) {
                                _attrcpy(&attr_change.attr, &m_last_attr);
                                attr_change.attr.hyperlink_length = hyperlink_get(m_last_attr.hyperlink_idx)->len;
				attr_change.text_end_offset = _vte_stream_head(m_text_stream);
			}
		}
		memset(&new_record, 0, sizeof (new_record));
		new_record.text_start_offset = text_offset;
		new_record.attr_start_offset = attr_offset;
		new_record.is_ascii = paragraph_is_ascii;
                new_record.bidi_flags = paragraph_bidi_flags;

		while (paragraph_len > 0) {
			/* Wrap one continuous run of identical attributes within the paragraph. */
			gsize runlength;  /* number of bytes we process in one run: identical attributes, within paragraph */
			if (attr_change.text_end_offset <= text_offset) {
				/* Attr change at line boundary, advance to next attr. */
                                attr_offset += sizeof (attr_change) + attr_change.attr.hyperlink_length + 2;
				if (!_vte_stream_read(m_attr_stream, attr_offset, (char *) &attr_change, sizeof (attr_change))) {
                                        _attrcpy(&attr_change.attr, &m_last_attr);
                                        attr_change.attr.hyperlink_length = hyperlink_get(m_last_attr.hyperlink_idx)->len;
					attr_change.text_end_offset = _vte_stream_head(m_text_stream);
				}
			}
			runlength = MIN(paragraph_len, attr_change.text_end_offset - text_offset);

                        if (paragraph_width <= (gsize) columns) {
                                /* Quick shortcut code path if the entire paragraph fits in one row. */
                                text_offset += runlength;
                                paragraph_len -= runlength;
                                /* The setting of "col" here is hacky. This very code here is potentially executed
                                   multiple times within a single paragraph, if it has attribute changes. The code above
                                   that reads the next attribute record has to iterate through those changes. Yet, we
                                   don't want to waste time tracking those attribute changes and finding their
                                   corresponding text offsets, we don't even want to read the text, as we won't need
                                   that. We rely on the fact that "paragraph_width" and "columns" are constants
                                   thoughout the wrapping of a particular paragraph, hence if this branch is hit once
                                   then it is hit every time; also "col" is unused then in this loop and only needs to
                                   have the correct value after we leave the loop. So each time simply set "col"
                                   straight away to its final value. */
                                col = paragraph_width;
                        } else if (G_UNLIKELY (attr_change.attr.columns() == 0)) {
				/* Combining characters all fit in the current row */
				text_offset += runlength;
				paragraph_len -= runlength;
			} else {
				while (runlength) {
					if (col >= columns - attr_change.attr.columns() + 1) {
						/* Wrap now, write the soft wrapped row's record */
                                                new_record.width = col;
						new_record.soft_wrapped = 1;
						_vte_stream_append(new_row_stream, (char const* ) &new_record, sizeof (new_record));
						_vte_debug_print(VTE_DEBUG_RING,
								"    New row %ld  text_offset %" G_GSIZE_FORMAT "  attr_offset %" G_GSIZE_FORMAT "  soft_wrapped\n",
								new_row_index,
								new_record.text_start_offset, new_record.attr_start_offset);
						for (i = 0; i < num_markers; i++) {
							if (G_UNLIKELY (marker_text_offsets[i].text_offset >= new_record.text_start_offset &&
									marker_text_offsets[i].text_offset < text_offset)) {
								new_markers[i].row = new_row_index;
								_vte_debug_print(VTE_DEBUG_RING,
										"      Marker #%d will be here in row %lu\n", i, new_row_index);
							}
						}

						new_row_index++;
						new_record.text_start_offset = text_offset;
						new_record.attr_start_offset = attr_offset;
						col = 0;
					}
					if (paragraph_is_ascii) {
						/* Shortcut for quickly wrapping ASCII (excluding TAB) text.
						   Don't read text_stream, and advance by a whole row of characters. */
						int len = MIN(runlength, (gsize) (columns - col));
						col += len;
						text_offset += len;
						paragraph_len -= len;
						runlength -= len;
					} else {
						/* Process one character only. */
						char textbuf[6];  /* fits at least one UTF-8 character */
						int textbuf_len;
						col += attr_change.attr.columns();
						/* Find beginning of next UTF-8 character */
						text_offset++; paragraph_len--; runlength--;
						textbuf_len = MIN(runlength, sizeof (textbuf));
						if (!_vte_stream_read(m_text_stream, text_offset, textbuf, textbuf_len))
							goto err;
						for (i = 0; i < textbuf_len && (textbuf[i] & 0xC0) == 0x80; i++) {
							text_offset++; paragraph_len--; runlength--;
						}
					}
				}
			}
		}

		/* Write the record of the paragraph's last row. */
		/* Hard wrapped, except maybe at the end of the very last paragraph */
                new_record.width = col;
		new_record.soft_wrapped = prev_record_was_soft_wrapped;
		_vte_stream_append(new_row_stream, (char const* ) &new_record, sizeof (new_record));
		_vte_debug_print(VTE_DEBUG_RING,
				"    New row %ld  text_offset %" G_GSIZE_FORMAT "  attr_offset %" G_GSIZE_FORMAT "\n",
				new_row_index,
				new_record.text_start_offset, new_record.attr_start_offset);
		for (i = 0; i < num_markers; i++) {
			if (G_UNLIKELY (marker_text_offsets[i].text_offset >= new_record.text_start_offset &&
					marker_text_offsets[i].text_offset < paragraph_end_text_offset)) {
				new_markers[i].row = new_row_index;
				_vte_debug_print(VTE_DEBUG_RING,
						"      Marker #%d will be here in row %lu\n", i, new_row_index);
			}
		}

		new_row_index++;
		paragraph_start_text_offset = paragraph_end_text_offset;
	}

	/* Update the ring. */
	old_ring_end = m_end;
	g_object_unref(m_row_stream);
	m_row_stream = new_row_stream;
	m_writable = m_end = new_row_index;
	m_start = 0;
	if (m_end > m_max)
		m_start = m_end - m_max;
	m_cached_row_num = (row_t) -1;

	/* Find the markers. This requires that the ring is already updated. */
	for (i = 0; i < num_markers; i++) {
		/* Compute the row for markers beyond the ring */
		if (new_markers[i].row == -1)
			new_markers[i].row = markers[i]->row - old_ring_end + m_end;
		/* Convert byte offset into visual column */
                if (!frozen_row_text_offset_to_column(new_markers[i].row, &marker_text_offsets[i], &new_markers[i].col)) {
                        /* This really shouldn't happen. It's too late to "goto err", the old stream is closed, the ring is updated.
                         * It would be a bit cumbersome to refactor the code to still revert here. Choose a simple solution. */
                        new_markers[i].col = 0;
                }
		_vte_debug_print(VTE_DEBUG_RING,
				"Marker #%d new coords:  text_offset %" G_GSIZE_FORMAT "  fragment_cells %d  eol_cells %d  ->  row %ld  col %ld\n",
				i, marker_text_offsets[i].text_offset, marker_text_offsets[i].fragment_cells,
				marker_text_offsets[i].eol_cells, new_markers[i].row, new_markers[i].col);
		markers[i]->row = new_markers[i].row;
		markers[i]->col = new_markers[i].col;
	}
	g_free(marker_text_offsets);
	g_free(new_markers);

	_vte_debug_print(VTE_DEBUG_RING, "Ring after rewrapping:\n");
        validate();
	return;

err:
#if VTE_DEBUG
	_vte_debug_print(VTE_DEBUG_RING,
			"Error while rewrapping\n");
	g_assert_not_reached();
#endif
	g_object_unref(new_row_stream);
	g_free(marker_text_offsets);
	g_free(new_markers);
}


bool
Ring::write_row(GOutputStream* stream,
                VteRowData* row,
                VteWriteFlags flags,
                GCancellable* cancellable,
                GError** error)
{
	VteCell *cell;
	GString *buffer = m_utf8_buffer;
	int i;
	gsize bytes_written;

	/* Simple version of the loop in freeze_row().
	 * TODO Should unify one day */
	g_string_truncate (buffer, 0);
	for (i = 0, cell = row->cells; i < row->len; i++, cell++) {
		if (G_LIKELY (!cell->attr.fragment()))
			_vte_unistr_append_to_string (cell->c, buffer);
	}
	if (!row->attr.soft_wrapped)
		g_string_append_c (buffer, '\n');

	return g_output_stream_write_all (stream, buffer->str, buffer->len, &bytes_written, cancellable, error);
}

/**
 * Ring::write_contents:
 * @stream: a #GOutputStream to write to
 * @flags: a set of #VteWriteFlags
 * @cancellable: optional #GCancellable object, %nullptr to ignore
 * @error: a #GError location to store the error occuring, or %nullptr to ignore
 *
 * Write entire ring contents to @stream according to @flags.
 *
 * Return: %TRUE on success, %FALSE if there was an error
 */
bool
Ring::write_contents(GOutputStream* stream,
                     VteWriteFlags flags,
                     GCancellable* cancellable,
                     GError** error)
{
	row_t i;

	_vte_debug_print(VTE_DEBUG_RING, "Writing contents to GOutputStream.\n");

	if (m_start < m_writable)
	{
		RowRecord record;

		if (read_row_record(&record, m_start))
		{
			gsize start_offset = record.text_start_offset;
			gsize end_offset = _vte_stream_head(m_text_stream);
			char buf[4096];
			while (start_offset < end_offset)
			{
				gsize bytes_written, len;

				len = MIN (G_N_ELEMENTS (buf), end_offset - start_offset);

				if (!_vte_stream_read (m_text_stream, start_offset,
						       buf, len))
					return false;

				if (!g_output_stream_write_all (stream, buf, len,
								&bytes_written, cancellable,
								error))
					return false;

				start_offset += len;
			}
		}
		else
                        //FIXMEchpe g_set_error!!
			return false;
	}

	for (i = m_writable; i < m_end; i++) {
		if (!write_row(stream,
                               get_writable_index(i),
                               flags, cancellable, error))
			return false;
	}

	return true;
}
