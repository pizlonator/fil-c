/*
 * Copyright (C) 2008 Red Hat, Inc.
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
 * Author(s):
 * 	Behdad Esfahbod
 */

#include <config.h>

#include "debug.h"

#include "vteunistr.h"

#include <string.h>


/* Overview:
 *
 * The way vteunistr is implemented is very simple: Unicode only defines
 * codepoints less than 0x110000.  That leaves plenty of room in a guint32 to
 * use for other things.  So, whenever our "string" contains only one Unicode
 * character, we use its code as our vteunistr.  Otherwise, we register the
 * string in a central registry and assign a unique number to it and use that.
 * This number is "our own private internal non-unicode code for this
 * sequence of characters".
 *
 * The rest of the problem would be how to efficiently implement this
 * registry.  It does *NOT* really have to be efficient at all, as it will
 * only be accessed in case of combining marks.  And the strings are pretty
 * short (two or three characters).  But our implementation is quite efficient
 * anyway.
 *
 * The access pattern of using vteunistr's is that we have a vteunistr in a
 * terminal cell, a new gunichar comes in and we decide to combine with it,
 * and we combine them and get a new vteunistr.  So, that is exactly how we
 * encode vteunistr's: all we need to know about a vteunistr to be able to
 * reconstruct its string is the vteunistr and the gunichar that joined to
 * form it.  That's what VteUnistrDecomp is.  That is the decomposition.
 *
 * We start giving new vteunistr's unique numbers starting at
 * %VTE_UNISTR_START+1 and going up.  We keep the decompositions in a GArray,
 * called unistr_decomp.  The first entry of the array is unused (that's why
 * we start from %VTE_UNISTR_START plus one).  The decomposition table provides
 * enough information to efficiently answer questions like "what's the first
 * gunichar in this vteunistr?", "what's the sequence of gunichar's in this
 * vteunistr?", and "how many gunichar's are there in this vteunistr?".
 *
 * We still do not have any efficient way to construct new vteunistr's though.
 * Given a vteunistr and a gunichar, we have to walk over the entire
 * decomposition table to see if we have already registered (encoded) this
 * combination.  To make that operation fast, we use a reverse map, that is,
 * a GHashTable named unistr_comp.  The hash table maps a decomposition to its
 * encoded vteunistr value.  The value obviously fits in a pointer and does
 * not need memory allocation.  We also want to avoid allocating memory for
 * the key of the hash table entries, as we already have those decompositions
 * in the memory in the unistr_decomp array.  We cannot use direct pointers
 * though as when growing, the GArray may resize and move to a new memory
 * buffer, rendering all our pointers invalid.  For this reason, we keep the
 * index into the array as our hash table key.  When we want to perform a
 * lookup in the hash table, we insert the decomposition that we are searching
 * for as item zero in the unistr_decomp table, then lookup for an equal entry
 * of it in the hash table.  Finally, if the hash lookup fails, we add the new
 * decomposition to the lookup array and the hash table, and return the newly
 * encoded vteunistr value.
 */

static vteunistr unistr_next = VTE_UNISTR_START + 1;

struct VteUnistrDecomp {
	vteunistr prefix;
	gunichar  suffix;
};

GArray     *unistr_decomp;
GHashTable *unistr_comp;

#define DECOMP_FROM_INDEX(i)	g_array_index (unistr_decomp, struct VteUnistrDecomp, (i))
#define DECOMP_FROM_UNISTR(s)	DECOMP_FROM_INDEX ((s) - VTE_UNISTR_START)

static guint
unistr_comp_hash (gconstpointer key)
{
	struct VteUnistrDecomp *decomp;
	decomp = &DECOMP_FROM_INDEX (GPOINTER_TO_UINT (key));
	return decomp->prefix ^ decomp->suffix;
}

static gboolean
unistr_comp_equal (gconstpointer a, gconstpointer b)
{
	return 0 == memcmp (&DECOMP_FROM_INDEX (GPOINTER_TO_UINT (a)),
			    &DECOMP_FROM_INDEX (GPOINTER_TO_UINT (b)),
			    sizeof (struct VteUnistrDecomp));
}

vteunistr
_vte_unistr_append_unichar (vteunistr s, gunichar c)
{
	struct VteUnistrDecomp decomp;
	vteunistr ret = 0;

	decomp.prefix = s;
	decomp.suffix = c;

	if (G_UNLIKELY (!unistr_decomp)) {
		unistr_decomp = g_array_new (FALSE, TRUE, sizeof (struct VteUnistrDecomp));
		g_array_set_size (unistr_decomp, 1);
		unistr_comp = g_hash_table_new (unistr_comp_hash, unistr_comp_equal);
	} else {
		DECOMP_FROM_INDEX (0) = decomp;
		ret = GPOINTER_TO_UINT (g_hash_table_lookup (unistr_comp, GUINT_TO_POINTER (0)));
	}

	if (G_UNLIKELY (!ret)) {
		/* sanity check to avoid OOM */
		if (G_UNLIKELY (_vte_unistr_strlen (s) > 10 || unistr_next - VTE_UNISTR_START > 100000))
			return s;

		ret = unistr_next++;
		g_array_append_val (unistr_decomp, decomp);
		g_hash_table_insert (unistr_comp,
				     GUINT_TO_POINTER (ret - VTE_UNISTR_START),
				     GUINT_TO_POINTER (ret));
	}

	return ret;
}

vteunistr
_vte_unistr_append_unistr (vteunistr s, vteunistr t)
{
        g_return_val_if_fail (s < unistr_next, s);
        g_return_val_if_fail (t < unistr_next, s);
        if (G_UNLIKELY (t >= VTE_UNISTR_START)) {
                s = _vte_unistr_append_unistr (s, DECOMP_FROM_UNISTR (t).prefix);
                return _vte_unistr_append_unichar (s, DECOMP_FROM_UNISTR (t).suffix);
        } else {
                return _vte_unistr_append_unichar (s, t);
        }
}

gunichar
_vte_unistr_get_base (vteunistr s)
{
	g_return_val_if_fail (s < unistr_next, s);
	while (G_UNLIKELY (s >= VTE_UNISTR_START))
		s = DECOMP_FROM_UNISTR (s).prefix;
	return (gunichar) s;
}

void
_vte_unistr_append_to_gunichars (vteunistr s, VteBidiChars *a)
{
        if (G_UNLIKELY (s >= VTE_UNISTR_START)) {
                struct VteUnistrDecomp *decomp;
                decomp = &DECOMP_FROM_UNISTR (s);
                _vte_unistr_append_to_gunichars (decomp->prefix, a);
                s = decomp->suffix;
        }
        gunichar val = (gunichar) s;
        vte_bidi_chars_append(a, &val);
}

vteunistr
_vte_unistr_replace_base (vteunistr s, gunichar c)
{
        g_return_val_if_fail (s < unistr_next, s);

        if (G_LIKELY (_vte_unistr_get_base(s) == c))
                return s;

        VteBidiChars a;
        vte_bidi_chars_init(&a);
        _vte_unistr_append_to_gunichars (s, &a);
        vte_assert_cmpint(vte_bidi_chars_get_size(&a), >=, 1);

        s = c;
        for (gsize i = 1; i < vte_bidi_chars_get_size(&a); i++)
                s = _vte_unistr_append_unichar (s, *vte_bidi_chars_get (&a, i));

        vte_bidi_chars_clear (&a);

        return s;
}

void
(_vte_unistr_append_to_string) (vteunistr s, GString *gs)
{
	g_return_if_fail (s < unistr_next);
	if (s >= VTE_UNISTR_START) {
		struct VteUnistrDecomp *decomp;
		decomp = &DECOMP_FROM_UNISTR (s);
		_vte_unistr_append_to_string (decomp->prefix, gs);
		s = decomp->suffix;
	}
	g_string_append_unichar (gs, (gunichar) s);
}

int
(_vte_unistr_strlen) (vteunistr s)
{
	int len = 1;
	g_return_val_if_fail (s < unistr_next, len);
	while (s >= VTE_UNISTR_START) {
		s = DECOMP_FROM_UNISTR (s).prefix;
		len++;
	}
	return len;
}
