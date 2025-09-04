/* Make sure you can add to ctf_open()ed CTF dicts, and that you
   cannot make changes to existing types.  */

#include <ctf-api.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char *argv[])
{
  ctf_dict_t *fp;
  ctf_archive_t *ctf;
  ctf_id_t type, ptrtype;
  ctf_arinfo_t ar = {0, 0, 0};
  ctf_encoding_t en = { CTF_INT_SIGNED, 0, sizeof (int) };
  unsigned char *ctf_written;
  size_t size;
  int err;

  if (argc != 2)
    {
      fprintf (stderr, "Syntax: %s PROGRAM\n", argv[0]);
      exit(1);
    }

  if ((ctf = ctf_open (argv[1], NULL, &err)) == NULL)
    goto open_err;
  if ((fp = ctf_dict_open (ctf, NULL, &err)) == NULL)
    goto open_err;

  /* Check that various modifications to already-written types
     are prohibited.  */

  if (ctf_add_integer (fp, CTF_ADD_ROOT, "int", &en) == 0)
    fprintf (stderr, "allowed to add integer existing in readonly portion\n");

  if (ctf_errno (fp) != ECTF_RDONLY)
    fprintf (stderr, "unexpected error %s attempting to add integer in readonly portion\n", ctf_errmsg (ctf_errno (fp)));

  if (ctf_add_typedef (fp, CTF_ADD_ROOT, "a_typedef", 0) == 0)
    fprintf (stderr, "allowed to add typedef existing in readonly portion\n");

  if (ctf_errno (fp) != ECTF_RDONLY)
    fprintf (stderr, "unexpected error %s attempting to add typedef in readonly portion\n", ctf_errmsg (ctf_errno (fp)));

  if (ctf_add_struct (fp, CTF_ADD_ROOT, "a_struct") == 0)
    fprintf (stderr, "allowed to add struct existing in readonly portion\n");

  if (ctf_errno (fp) != ECTF_RDONLY)
    fprintf (stderr, "unexpected error %s attempting to add struct in readonly portion\n", ctf_errmsg (ctf_errno (fp)));

  if (ctf_add_union (fp, CTF_ADD_ROOT, "a_union") == 0)
    fprintf (stderr, "allowed to add union existing in readonly portion\n");

  if (ctf_errno (fp) != ECTF_RDONLY)
    fprintf (stderr, "unexpected error %s attempting to add union in readonly portion\n", ctf_errmsg (ctf_errno (fp)));

  if (ctf_add_enum (fp, CTF_ADD_ROOT, "an_enum") == 0)
    fprintf (stderr, "allowed to add enum existing in readonly portion\n");

  if (ctf_errno (fp) != ECTF_RDONLY)
    fprintf (stderr, "unexpected error %s attempting to add enum in readonly portion\n", ctf_errmsg (ctf_errno (fp)));

  if (ctf_add_struct (fp, CTF_ADD_ROOT, "struct_forward") == 0)
    fprintf (stderr, "allowed to promote struct forward existing in readonly portion\n");

  if (ctf_errno (fp) != ECTF_RDONLY)
    fprintf (stderr, "unexpected error %s attempting to promote struct forward in readonly portion\n", ctf_errmsg (ctf_errno (fp)));

  if (ctf_add_union (fp, CTF_ADD_ROOT, "union_forward") == 0)
    fprintf (stderr, "allowed to promote union forward existing in readonly portion\n");

  if (ctf_errno (fp) != ECTF_RDONLY)
    fprintf (stderr, "unexpected error %s attempting to promote union forward in readonly portion\n", ctf_errmsg (ctf_errno (fp)));

  if (ctf_add_enum (fp, CTF_ADD_ROOT, "enum_forward") == 0)
    fprintf (stderr, "allowed to promote enum forward existing in readonly portion\n");

  if (ctf_errno (fp) != ECTF_RDONLY)
    fprintf (stderr, "unexpected error %s attempting to promote enum forward in readonly portion\n", ctf_errmsg (ctf_errno (fp)));

  if ((type = ctf_lookup_by_name (fp, "struct a_struct")) == CTF_ERR)
    fprintf (stderr, "Lookup of struct a_struct failed: %s\n", ctf_errmsg (ctf_errno (fp)));

  if (ctf_add_member (fp, type, "wombat", 0) == 0)
    fprintf (stderr, "allowed to add member to struct existing in readonly portion\n");

  if (ctf_errno (fp) != ECTF_RDONLY)
    fprintf (stderr, "unexpected error %s attempting to add member to struct in readonly portion\n", ctf_errmsg (ctf_errno (fp)));

  if ((type = ctf_lookup_by_name (fp, "union a_union")) == CTF_ERR)
    fprintf (stderr, "Lookup of union a_union failed: %s\n", ctf_errmsg (ctf_errno (fp)));

  if (ctf_add_member (fp, type, "wombat", 0) == 0)
    fprintf (stderr, "allowed to add member to union existing in readonly portion\n");

  if (ctf_errno (fp) != ECTF_RDONLY)
    fprintf (stderr, "unexpected error %s attempting to add member to union in readonly portion\n", ctf_errmsg (ctf_errno (fp)));

  if ((type = ctf_lookup_by_name (fp, "enum an_enum")) == CTF_ERR)
    fprintf (stderr, "Lookup of enum an_enum failed: %s\n", ctf_errmsg (ctf_errno (fp)));

  if (ctf_add_enumerator (fp, type, "wombat", 0) == 0)
    fprintf (stderr, "allowed to add enumerator to enum existing in readonly portion\n");

  if (ctf_errno (fp) != ECTF_RDONLY)
    fprintf (stderr, "unexpected error %s attempting to add enumerator to enum in readonly portion\n", ctf_errmsg (ctf_errno (fp)));

  if ((type = ctf_lookup_by_name (fp, "an_array")) == CTF_ERR)
    fprintf (stderr, "Lookup of an_array failed: %s\n", ctf_errmsg (ctf_errno (fp)));

  if ((type = ctf_type_reference (fp, type)) == CTF_ERR)
    fprintf (stderr, "Lookup of type reffed by an_array failed: %s\n", ctf_errmsg (ctf_errno (fp)));

  if (ctf_set_array (fp, type, &ar) == 0)
    fprintf (stderr, "allowed to set array in readonly portion\n");

  if (ctf_errno (fp) != ECTF_RDONLY)
    fprintf (stderr, "unexpected error %s attempting to set array in readonly portion\n", ctf_errmsg (ctf_errno (fp)));

  if ((ctf_written = ctf_write_mem (fp, &size, 4096)) == NULL)
    fprintf (stderr, "Re-writeout unexpectedly failed: %s\n", ctf_errmsg (ctf_errno (fp)));
  free (ctf_written);

  /* Finally, make sure we can add new types, and look them up again.  */

  if ((type = ctf_lookup_by_name (fp, "struct a_struct")) == CTF_ERR)
    fprintf (stderr, "Lookup of struct a_struct failed: %s\n", ctf_errmsg (ctf_errno (fp)));

  if ((ptrtype = ctf_add_pointer (fp, CTF_ADD_ROOT, type)) == CTF_ERR)
    fprintf (stderr, "Cannot add pointer to ctf_opened dict: %s\n", ctf_errmsg (ctf_errno (fp)));

  if (ctf_type_reference (fp, ptrtype) == CTF_ERR)
    fprintf (stderr, "Lookup of pointer preserved across writeout failed: %s\n", ctf_errmsg (ctf_errno (fp)));

  if (ctf_type_reference (fp, ptrtype) != type)
    fprintf (stderr, "Look up of newly-added type in serialized dict yields ID %lx, expected %lx\n", ctf_type_reference (fp, ptrtype), type);

  ctf_dict_close (fp);
  ctf_close (ctf);

  printf ("All done.\n");
  return 0;
 
 open_err:
  fprintf (stderr, "%s: cannot open: %s\n", argv[0], ctf_errmsg (err));
  return 1;
}    
