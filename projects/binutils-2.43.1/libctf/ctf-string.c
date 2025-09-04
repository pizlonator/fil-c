/* CTF string table management.
   Copyright (C) 2019-2024 Free Software Foundation, Inc.

   This file is part of libctf.

   libctf is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not see
   <http://www.gnu.org/licenses/>.  */

#include <assert.h>
#include <ctf-impl.h>
#include <string.h>

static ctf_str_atom_t *
ctf_str_add_ref_internal (ctf_dict_t *fp, const char *str,
			  int flags, uint32_t *ref);

/* Convert an encoded CTF string name into a pointer to a C string, possibly
  using an explicit internal provisional strtab rather than the fp-based
  one.  */
const char *
ctf_strraw_explicit (ctf_dict_t *fp, uint32_t name, ctf_strs_t *strtab)
{
  ctf_strs_t *ctsp = &fp->ctf_str[CTF_NAME_STID (name)];

  if ((CTF_NAME_STID (name) == CTF_STRTAB_0) && (strtab != NULL))
    ctsp = strtab;

  /* If this name is in the external strtab, and there is a synthetic
     strtab, use it in preference.  (This is used to add the set of strings
     -- symbol names, etc -- the linker knows about before the strtab is
     written out.)  */

  if (CTF_NAME_STID (name) == CTF_STRTAB_1
      && fp->ctf_syn_ext_strtab != NULL)
    return ctf_dynhash_lookup (fp->ctf_syn_ext_strtab,
			       (void *) (uintptr_t) name);

  /* If the name is in the internal strtab, and the name offset is beyond
     the end of the ctsp->cts_len but below the ctf_str_prov_offset, this is
     a provisional string added by ctf_str_add*() but not yet built into a
     real strtab: get the value out of the ctf_prov_strtab.  */

  if (CTF_NAME_STID (name) == CTF_STRTAB_0
      && name >= ctsp->cts_len && name < fp->ctf_str_prov_offset)
      return ctf_dynhash_lookup (fp->ctf_prov_strtab,
				 (void *) (uintptr_t) name);

  if (ctsp->cts_strs != NULL && CTF_NAME_OFFSET (name) < ctsp->cts_len)
    return (ctsp->cts_strs + CTF_NAME_OFFSET (name));

  /* String table not loaded or corrupt offset.  */
  return NULL;
}

/* Convert an encoded CTF string name into a pointer to a C string by looking
  up the appropriate string table buffer and then adding the offset.  */
const char *
ctf_strraw (ctf_dict_t *fp, uint32_t name)
{
  return ctf_strraw_explicit (fp, name, NULL);
}

/* Return a guaranteed-non-NULL pointer to the string with the given CTF
   name.  */
const char *
ctf_strptr (ctf_dict_t *fp, uint32_t name)
{
  const char *s = ctf_strraw (fp, name);
  return (s != NULL ? s : "(?)");
}

/* As above, but return info on what is wrong in more detail.
   (Used for type lookups.) */

const char *
ctf_strptr_validate (ctf_dict_t *fp, uint32_t name)
{
  const char *str = ctf_strraw (fp, name);

  if (str == NULL)
    {
      if (CTF_NAME_STID (name) == CTF_STRTAB_1
	  && fp->ctf_syn_ext_strtab == NULL
	  && fp->ctf_str[CTF_NAME_STID (name)].cts_strs == NULL)
	{
	  ctf_set_errno (fp, ECTF_STRTAB);
	  return NULL;
	}

      ctf_set_errno (fp, ECTF_BADNAME);
      return NULL;
    }
  return str;
}

/* Remove all refs to a given atom.  */
static void
ctf_str_purge_atom_refs (ctf_str_atom_t *atom)
{
  ctf_str_atom_ref_t *ref, *next;
  ctf_str_atom_ref_movable_t *movref, *movnext;

  for (ref = ctf_list_next (&atom->csa_refs); ref != NULL; ref = next)
    {
      next = ctf_list_next (ref);
      ctf_list_delete (&atom->csa_refs, ref);
      free (ref);
    }

  for (movref = ctf_list_next (&atom->csa_movable_refs);
       movref != NULL; movref = movnext)
    {
      movnext = ctf_list_next (movref);
      ctf_list_delete (&atom->csa_movable_refs, movref);

      ctf_dynhash_remove (movref->caf_movable_refs, movref);

      free (movref);
    }
}

/* Free an atom.  */
static void
ctf_str_free_atom (void *a)
{
  ctf_str_atom_t *atom = a;

  ctf_str_purge_atom_refs (atom);

  if (atom->csa_flags & CTF_STR_ATOM_FREEABLE)
    free (atom->csa_str);

  free (atom);
}

/* Create the atoms table.  There is always at least one atom in it, the null
   string: but also pull in atoms from the internal strtab.  (We rely on
   calls to ctf_str_add_external to populate external strtab entries, since
   these are often not quite the same as what appears in any external
   strtab, and the external strtab is often huge and best not aggressively
   pulled in.)  */
int
ctf_str_create_atoms (ctf_dict_t *fp)
{
  size_t i;

  fp->ctf_str_atoms = ctf_dynhash_create (ctf_hash_string, ctf_hash_eq_string,
					  NULL, ctf_str_free_atom);
  if (!fp->ctf_str_atoms)
    return -ENOMEM;

  if (!fp->ctf_prov_strtab)
    fp->ctf_prov_strtab = ctf_dynhash_create (ctf_hash_integer,
					      ctf_hash_eq_integer,
					      NULL, NULL);
  if (!fp->ctf_prov_strtab)
    goto oom_prov_strtab;

  fp->ctf_str_movable_refs = ctf_dynhash_create (ctf_hash_integer,
						 ctf_hash_eq_integer,
						 NULL, NULL);
  if (!fp->ctf_str_movable_refs)
    goto oom_movable_refs;

  errno = 0;
  ctf_str_add (fp, "");
  if (errno == ENOMEM)
    goto oom_str_add;

  /* Pull in all the strings in the strtab as new atoms.  The provisional
     strtab must be empty at this point, so there is no need to populate
     atoms from it as well.  Types in this subset are frozen and readonly,
     so the refs list and movable refs list need not be populated.  */

  for (i = 0; i < fp->ctf_str[CTF_STRTAB_0].cts_len;
       i += strlen (&fp->ctf_str[CTF_STRTAB_0].cts_strs[i]) + 1)
    {
      ctf_str_atom_t *atom;

      if (fp->ctf_str[CTF_STRTAB_0].cts_strs[i] == 0)
	continue;

      atom = ctf_str_add_ref_internal (fp, &fp->ctf_str[CTF_STRTAB_0].cts_strs[i],
				       0, 0);

      if (!atom)
	goto oom_str_add;

      atom->csa_offset = i;
    }

  fp->ctf_str_prov_offset = fp->ctf_str[CTF_STRTAB_0].cts_len + 1;

  return 0;

 oom_str_add:
  ctf_dynhash_destroy (fp->ctf_str_movable_refs);
  fp->ctf_str_movable_refs = NULL;
 oom_movable_refs:
  ctf_dynhash_destroy (fp->ctf_prov_strtab);
  fp->ctf_prov_strtab = NULL;
 oom_prov_strtab:
  ctf_dynhash_destroy (fp->ctf_str_atoms);
  fp->ctf_str_atoms = NULL;
  return -ENOMEM;
}

/* Destroy the atoms table and associated refs.  */
void
ctf_str_free_atoms (ctf_dict_t *fp)
{
  ctf_dynhash_destroy (fp->ctf_prov_strtab);
  ctf_dynhash_destroy (fp->ctf_str_atoms);
  ctf_dynhash_destroy (fp->ctf_str_movable_refs);
  if (fp->ctf_dynstrtab)
    {
      free (fp->ctf_dynstrtab->cts_strs);
      free (fp->ctf_dynstrtab);
    }
}

#define CTF_STR_ADD_REF 0x1
#define CTF_STR_PROVISIONAL 0x2
#define CTF_STR_MOVABLE 0x4

/* Allocate a ref and bind it into a ref list.  */

static ctf_str_atom_ref_t *
aref_create (ctf_dict_t *fp, ctf_str_atom_t *atom, uint32_t *ref, int flags)
{
  ctf_str_atom_ref_t *aref;
  size_t s = sizeof (struct ctf_str_atom_ref);

  if (flags & CTF_STR_MOVABLE)
    s = sizeof (struct ctf_str_atom_ref_movable);

  aref = malloc (s);

  if (!aref)
    return NULL;

  aref->caf_ref = ref;

  /* Movable refs get a backpointer to them in ctf_str_movable_refs, and a
     pointer to ctf_str_movable_refs itself in the ref, for use when freeing
     refs: they can be moved later in batches via a call to
     ctf_str_move_refs.  */

  if (flags & CTF_STR_MOVABLE)
    {
      ctf_str_atom_ref_movable_t *movref = (ctf_str_atom_ref_movable_t *) aref;

      movref->caf_movable_refs = fp->ctf_str_movable_refs;

      if (ctf_dynhash_insert (fp->ctf_str_movable_refs, ref, aref) < 0)
	{
	  free (aref);
	  return NULL;
	}
      ctf_list_append (&atom->csa_movable_refs, movref);
    }
  else
    ctf_list_append (&atom->csa_refs, aref);

  return aref;
}

/* Add a string to the atoms table, copying the passed-in string if
   necessary.  Return the atom added. Return NULL only when out of memory
   (and do not touch the passed-in string in that case).

   Possibly add a provisional entry for this string to the provisional
   strtab.  If the string is in the provisional strtab, update its ref list
   with the passed-in ref, causing the ref to be updated when the strtab is
   written out.  */

static ctf_str_atom_t *
ctf_str_add_ref_internal (ctf_dict_t *fp, const char *str,
			  int flags, uint32_t *ref)
{
  char *newstr = NULL;
  ctf_str_atom_t *atom = NULL;
  int added = 0;

  atom = ctf_dynhash_lookup (fp->ctf_str_atoms, str);

  /* Existing atoms get refs added only if they are provisional:
     non-provisional strings already have a fixed strtab offset, and just
     get their ref updated immediately, since its value cannot change.  */

  if (atom)
    {
      if (!ctf_dynhash_lookup (fp->ctf_prov_strtab, (void *) (uintptr_t)
			       atom->csa_offset))
	{
	  if (flags & CTF_STR_ADD_REF)
	    {
	      if (atom->csa_external_offset)
		*ref = atom->csa_external_offset;
	      else
		*ref = atom->csa_offset;
	    }
	  return atom;
	}

      if (flags & CTF_STR_ADD_REF)
	{
	  if (!aref_create (fp, atom, ref, flags))
	    {
	      ctf_set_errno (fp, ENOMEM);
	      return NULL;
	    }
	}

      return atom;
    }

  /* New atom.  */

  if ((atom = malloc (sizeof (struct ctf_str_atom))) == NULL)
    goto oom;
  memset (atom, 0, sizeof (struct ctf_str_atom));

  /* Don't allocate new strings if this string is within an mmapped
     strtab.  */

  if ((unsigned char *) str < (unsigned char *) fp->ctf_data_mmapped
      || (unsigned char *) str > (unsigned char *) fp->ctf_data_mmapped + fp->ctf_data_mmapped_len)
    {
      if ((newstr = strdup (str)) == NULL)
	goto oom;
      atom->csa_flags |= CTF_STR_ATOM_FREEABLE;
      atom->csa_str = newstr;
    }
  else
    atom->csa_str = (char *) str;

  if (ctf_dynhash_insert (fp->ctf_str_atoms, atom->csa_str, atom) < 0)
    goto oom;
  added = 1;

  atom->csa_snapshot_id = fp->ctf_snapshots;

  /* New atoms marked provisional go into the provisional strtab, and get a
     ref added.  */

  if (flags & CTF_STR_PROVISIONAL)
    {
      atom->csa_offset = fp->ctf_str_prov_offset;

      if (ctf_dynhash_insert (fp->ctf_prov_strtab, (void *) (uintptr_t)
			      atom->csa_offset, (void *) atom->csa_str) < 0)
	goto oom;

      fp->ctf_str_prov_offset += strlen (atom->csa_str) + 1;

      if (flags & CTF_STR_ADD_REF)
      {
	if (!aref_create (fp, atom, ref, flags))
	  goto oom;
      }
    }

  return atom;

 oom:
  if (added)
    ctf_dynhash_remove (fp->ctf_str_atoms, atom->csa_str);
  free (atom);
  free (newstr);
  ctf_set_errno (fp, ENOMEM);
  return NULL;
}

/* Add a string to the atoms table, without augmenting the ref list for this
   string: return a 'provisional offset' which can be used to return this string
   until ctf_str_write_strtab is called, or 0 on failure.  (Everywhere the
   provisional offset is assigned to should be added as a ref using
   ctf_str_add_ref() as well.) */
uint32_t
ctf_str_add (ctf_dict_t *fp, const char *str)
{
  ctf_str_atom_t *atom;

  if (!str)
    str = "";

  atom = ctf_str_add_ref_internal (fp, str, CTF_STR_PROVISIONAL, 0);
  if (!atom)
    return 0;

  return atom->csa_offset;
}

/* Like ctf_str_add(), but additionally augment the atom's refs list with the
   passed-in ref, whether or not the string is already present.  There is no
   attempt to deduplicate the refs list (but duplicates are harmless).  */
uint32_t
ctf_str_add_ref (ctf_dict_t *fp, const char *str, uint32_t *ref)
{
  ctf_str_atom_t *atom;

  if (!str)
    str = "";

  atom = ctf_str_add_ref_internal (fp, str, CTF_STR_ADD_REF
				   | CTF_STR_PROVISIONAL, ref);
  if (!atom)
    return 0;

  return atom->csa_offset;
}

/* Like ctf_str_add_ref(), but note that the ref may be moved later on.  */
uint32_t
ctf_str_add_movable_ref (ctf_dict_t *fp, const char *str, uint32_t *ref)
{
  ctf_str_atom_t *atom;

  if (!str)
    str = "";

  atom = ctf_str_add_ref_internal (fp, str, CTF_STR_ADD_REF
				   | CTF_STR_PROVISIONAL
				   | CTF_STR_MOVABLE, ref);
  if (!atom)
    return 0;

  return atom->csa_offset;
}

/* Add an external strtab reference at OFFSET.  Returns zero if the addition
   failed, nonzero otherwise.  */
int
ctf_str_add_external (ctf_dict_t *fp, const char *str, uint32_t offset)
{
  ctf_str_atom_t *atom;

  if (!str)
    str = "";

  atom = ctf_str_add_ref_internal (fp, str, 0, 0);
  if (!atom)
    return 0;

  atom->csa_external_offset = CTF_SET_STID (offset, CTF_STRTAB_1);

  if (!fp->ctf_syn_ext_strtab)
    fp->ctf_syn_ext_strtab = ctf_dynhash_create (ctf_hash_integer,
						 ctf_hash_eq_integer,
						 NULL, NULL);
  if (!fp->ctf_syn_ext_strtab)
    {
      ctf_set_errno (fp, ENOMEM);
      return 0;
    }

  if (ctf_dynhash_insert (fp->ctf_syn_ext_strtab,
			  (void *) (uintptr_t)
			  atom->csa_external_offset,
			  (void *) atom->csa_str) < 0)
    {
      /* No need to bother freeing the syn_ext_strtab: it will get freed at
	 ctf_str_write_strtab time if unreferenced.  */
      ctf_set_errno (fp, ENOMEM);
      return 0;
    }

  return 1;
}

/* Note that refs have moved from (SRC, LEN) to DEST.  We use the movable
   refs backpointer for this, because it is done an amortized-constant
   number of times during structure member and enumerand addition, and if we
   did a linear search this would turn such addition into an O(n^2)
   operation.  Even this is not linear, but it's better than that.  */
int
ctf_str_move_refs (ctf_dict_t *fp, void *src, size_t len, void *dest)
{
  uintptr_t p;

  if (src == dest)
    return 0;

  for (p = (uintptr_t) src; p - (uintptr_t) src < len; p++)
    {
      ctf_str_atom_ref_movable_t *ref;

      if ((ref = ctf_dynhash_lookup (fp->ctf_str_movable_refs,
				     (ctf_str_atom_ref_t *) p)) != NULL)
	{
	  int out_of_memory;

	  ref->caf_ref = (uint32_t *) (((uintptr_t) ref->caf_ref +
					(uintptr_t) dest - (uintptr_t) src));
	  ctf_dynhash_remove (fp->ctf_str_movable_refs,
			      (ctf_str_atom_ref_t *) p);
	  out_of_memory = ctf_dynhash_insert (fp->ctf_str_movable_refs,
					      ref->caf_ref, ref);
	  assert (out_of_memory == 0);
	}
    }

  return 0;
}

/* Remove a single ref.  */
void
ctf_str_remove_ref (ctf_dict_t *fp, const char *str, uint32_t *ref)
{
  ctf_str_atom_ref_t *aref, *anext;
  ctf_str_atom_ref_movable_t *amovref, *amovnext;
  ctf_str_atom_t *atom = NULL;

  atom = ctf_dynhash_lookup (fp->ctf_str_atoms, str);
  if (!atom)
    return;

  for (aref = ctf_list_next (&atom->csa_refs); aref != NULL; aref = anext)
    {
      anext = ctf_list_next (aref);
      if (aref->caf_ref == ref)
	{
	  ctf_list_delete (&atom->csa_refs, aref);
	  free (aref);
	}
    }

  for (amovref = ctf_list_next (&atom->csa_movable_refs);
       amovref != NULL; amovref = amovnext)
    {
      amovnext = ctf_list_next (amovref);
      if (amovref->caf_ref == ref)
	{
	  ctf_list_delete (&atom->csa_movable_refs, amovref);
	  ctf_dynhash_remove (fp->ctf_str_movable_refs, ref);
	  free (amovref);
	}
    }
}

/* A ctf_dynhash_iter_remove() callback that removes atoms later than a given
   snapshot ID.  External atoms are never removed, because they came from the
   linker string table and are still present even if you roll back type
   additions.  */
static int
ctf_str_rollback_atom (void *key _libctf_unused_, void *value, void *arg)
{
  ctf_str_atom_t *atom = (ctf_str_atom_t *) value;
  ctf_snapshot_id_t *id = (ctf_snapshot_id_t *) arg;

  return (atom->csa_snapshot_id > id->snapshot_id)
    && (atom->csa_external_offset == 0);
}

/* Roll back, deleting all (internal) atoms created after a particular ID.  */
void
ctf_str_rollback (ctf_dict_t *fp, ctf_snapshot_id_t id)
{
  ctf_dynhash_iter_remove (fp->ctf_str_atoms, ctf_str_rollback_atom, &id);
}

/* An adaptor around ctf_purge_atom_refs.  */
static void
ctf_str_purge_one_atom_refs (void *key _libctf_unused_, void *value,
			     void *arg _libctf_unused_)
{
  ctf_str_atom_t *atom = (ctf_str_atom_t *) value;

  ctf_str_purge_atom_refs (atom);
}

/* Remove all the recorded refs from the atoms table.  */
static void
ctf_str_purge_refs (ctf_dict_t *fp)
{
  ctf_dynhash_iter (fp->ctf_str_atoms, ctf_str_purge_one_atom_refs, NULL);
}

/* Update a list of refs to the specified value. */
static void
ctf_str_update_refs (ctf_str_atom_t *refs, uint32_t value)
{
  ctf_str_atom_ref_t *ref;
  ctf_str_atom_ref_movable_t *movref;

  for (ref = ctf_list_next (&refs->csa_refs); ref != NULL;
       ref = ctf_list_next (ref))
    *(ref->caf_ref) = value;

  for (movref = ctf_list_next (&refs->csa_movable_refs);
       movref != NULL; movref = ctf_list_next (movref))
    *(movref->caf_ref) = value;
}

/* Sort the strtab.  */
static int
ctf_str_sort_strtab (const void *a, const void *b)
{
  ctf_str_atom_t **one = (ctf_str_atom_t **) a;
  ctf_str_atom_t **two = (ctf_str_atom_t **) b;

  return (strcmp ((*one)->csa_str, (*two)->csa_str));
}

/* Write out and return a strtab containing all strings with recorded refs,
   adjusting the refs to refer to the corresponding string.  The returned
   strtab is already assigned to strtab 0 in this dict, is owned by this
   dict, and may be NULL on error.  Also populate the synthetic strtab with
   mappings from external strtab offsets to names, so we can look them up
   with ctf_strptr().  Only external strtab offsets with references are
   added.

   As a side effect, replaces the strtab of the current dict with the newly-
   generated strtab.  This is an exception to the general rule that
   serialization does not change the dict passed in, because the alternative
   is to copy the entire atoms table on every reserialization just to avoid
   modifying the original, which is excessively costly for minimal gain.

   We use the lazy man's approach and double memory costs by always storing
   atoms as individually allocated entities whenever they come from anywhere
   but a freshly-opened, mmapped dict, even though after serialization there
   is another copy in the strtab; this ensures that ctf_strptr()-returned
   pointers to them remain valid for the lifetime of the dict.

   This is all rendered more complex because if a dict is ctf_open()ed it
   will have a bunch of strings in its strtab already, and their strtab
   offsets can never change (without piles of complexity to rescan the
   entire dict just to get all the offsets to all of them into the atoms
   table).  Entries below the existing strtab limit are just copied into the
   new dict: entries above it are new, and are are sorted first, then
   appended to it.  The sorting is purely a compression-efficiency
   improvement, and we get nearly as good an improvement from sorting big
   chunks like this as we would from sorting the whole thing.  */

const ctf_strs_writable_t *
ctf_str_write_strtab (ctf_dict_t *fp)
{
  ctf_strs_writable_t *strtab;
  size_t strtab_count = 0;
  uint32_t cur_stroff = 0;
  ctf_str_atom_t **sorttab;
  ctf_next_t *it = NULL;
  size_t i;
  void *v;
  int err;
  int new_strtab = 0;
  int any_external = 0;

  strtab = calloc (1, sizeof (ctf_strs_writable_t));
  if (!strtab)
    return NULL;

  /* The strtab contains the existing string table at its start: figure out
     how many new strings we need to add.  We only need to add new strings
     that have no external offset, that have refs, and that are found in the
     provisional strtab.  If the existing strtab is empty we also need to
     add the null string at its start.  */

  strtab->cts_len = fp->ctf_str[CTF_STRTAB_0].cts_len;

  if (strtab->cts_len == 0)
    {
      new_strtab = 1;
      strtab->cts_len++; 			/* For the \0.  */
    }

  /* Count new entries in the strtab: i.e. entries in the provisional
     strtab.  Ignore any entry for \0, entries which ended up in the
     external strtab, and unreferenced entries.  */

  while ((err = ctf_dynhash_next (fp->ctf_prov_strtab, &it, NULL, &v)) == 0)
    {
      const char *str = (const char *) v;
      ctf_str_atom_t *atom;

      atom = ctf_dynhash_lookup (fp->ctf_str_atoms, str);
      if (!ctf_assert (fp, atom))
	goto err_strtab;

      if (atom->csa_str[0] == 0 || atom->csa_external_offset
	  || (ctf_list_empty_p (&atom->csa_refs)
	      && ctf_list_empty_p (&atom->csa_movable_refs)))
	continue;

      strtab->cts_len += strlen (atom->csa_str) + 1;
      strtab_count++;
    }
  if (err != ECTF_NEXT_END)
    {
      ctf_dprintf ("ctf_str_write_strtab: error counting strtab entries: %s\n",
		   ctf_errmsg (err));
      goto err_strtab;
    }

  ctf_dprintf ("%lu bytes of strings in strtab: %lu pre-existing.\n",
	       (unsigned long) strtab->cts_len,
	       (unsigned long) fp->ctf_str[CTF_STRTAB_0].cts_len);

  /* Sort the new part of the strtab.  */

  sorttab = calloc (strtab_count, sizeof (ctf_str_atom_t *));
  if (!sorttab)
    {
      ctf_set_errno (fp, ENOMEM);
      goto err_strtab;
    }

  i = 0;
  while ((err = ctf_dynhash_next (fp->ctf_prov_strtab, &it, NULL, &v)) == 0)
    {
      ctf_str_atom_t *atom;

      atom = ctf_dynhash_lookup (fp->ctf_str_atoms, v);
      if (!ctf_assert (fp, atom))
	goto err_sorttab;

      if (atom->csa_str[0] == 0 || atom->csa_external_offset
	  || (ctf_list_empty_p (&atom->csa_refs)
	      && ctf_list_empty_p (&atom->csa_movable_refs)))
	continue;

      sorttab[i++] = atom;
    }

  qsort (sorttab, strtab_count, sizeof (ctf_str_atom_t *),
	 ctf_str_sort_strtab);

  if ((strtab->cts_strs = malloc (strtab->cts_len)) == NULL)
    goto err_sorttab;

  cur_stroff = fp->ctf_str[CTF_STRTAB_0].cts_len;

  if (new_strtab)
    {
      strtab->cts_strs[0] = 0;
      cur_stroff++;
    }
  else
    memcpy (strtab->cts_strs, fp->ctf_str[CTF_STRTAB_0].cts_strs,
	    fp->ctf_str[CTF_STRTAB_0].cts_len);

  /* Work over the sorttab, add its strings to the strtab, and remember
     where they are in the csa_offset for the appropriate atom.  No ref
     updating is done at this point, because refs might well relate to
     already-existing strings, or external strings, which do not need adding
     to the strtab and may not be in the sorttab.  */

  for (i = 0; i < strtab_count; i++)
    {
      sorttab[i]->csa_offset = cur_stroff;
      strcpy (&strtab->cts_strs[cur_stroff], sorttab[i]->csa_str);
      cur_stroff += strlen (sorttab[i]->csa_str) + 1;
    }
  free (sorttab);
  sorttab = NULL;

  /* Update all refs, then purge them as no longer necessary: also update
     the strtab appropriately.  */

  while ((err = ctf_dynhash_next (fp->ctf_str_atoms, &it, NULL, &v)) == 0)
    {
      ctf_str_atom_t *atom = (ctf_str_atom_t *) v;
      uint32_t offset;

      if (ctf_list_empty_p (&atom->csa_refs) &&
	  ctf_list_empty_p (&atom->csa_movable_refs))
	continue;

      if (atom->csa_external_offset)
	{
	  any_external = 1;
	  offset = atom->csa_external_offset;
	}
      else
	offset = atom->csa_offset;
      ctf_str_update_refs (atom, offset);
    }
  if (err != ECTF_NEXT_END)
    {
      ctf_dprintf ("ctf_str_write_strtab: error iterating over atoms while updating refs: %s\n",
		   ctf_errmsg (err));
      goto err_strtab;
    }
  ctf_str_purge_refs (fp);

  if (!any_external)
    {
      ctf_dynhash_destroy (fp->ctf_syn_ext_strtab);
      fp->ctf_syn_ext_strtab = NULL;
    }

  /* Replace the old strtab with the new one in this dict.  */

  if (fp->ctf_dynstrtab)
    {
      free (fp->ctf_dynstrtab->cts_strs);
      free (fp->ctf_dynstrtab);
    }

  fp->ctf_dynstrtab = strtab;
  fp->ctf_str[CTF_STRTAB_0].cts_strs = strtab->cts_strs;
  fp->ctf_str[CTF_STRTAB_0].cts_len = strtab->cts_len;

  /* All the provisional strtab entries are now real strtab entries, and
     ctf_strptr() will find them there.  The provisional offset now starts right
     beyond the new end of the strtab.  */

  ctf_dynhash_empty (fp->ctf_prov_strtab);
  fp->ctf_str_prov_offset = strtab->cts_len + 1;
  return strtab;

 err_sorttab:
  free (sorttab);
 err_strtab:
  free (strtab);
  return NULL;
}
