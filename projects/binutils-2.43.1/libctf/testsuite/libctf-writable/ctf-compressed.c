/* Make sure linking and writing an archive with a low threshold correctly
   compresses it.  (This tests for two bugs, one where archives weren't
   serialized regardless of their threshold, and another where nothing was.)  */

#include <ctf-api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main (int argc, char *argv[])
{
  ctf_dict_t *in1;
  ctf_dict_t *in2;
  ctf_dict_t *fp;
  ctf_dict_t *dump_fp;
  ctf_archive_t *arc1, *arc2;
  ctf_archive_t *final_arc;
  ctf_sect_t s1, s2;
  ctf_encoding_t encoding = { CTF_INT_SIGNED, 0, sizeof (char) };
  ctf_encoding_t encoding2 = { CTF_INT_SIGNED, 0, sizeof (long long) };
  unsigned char *buf1, *buf2, *buf3;
  size_t buf1_sz, buf2_sz, buf3_sz;
  ctf_dump_state_t *dump_state = NULL;
  ctf_next_t *i = NULL;
  int err;

  /* Linking does not currently work on mingw because of an unreliable tmpfile
     implementation on that platform (see
     https://github.com/msys2/MINGW-packages/issues/18878).  Simply skip for
     now.  */

#ifdef __MINGW32__
  printf ("UNSUPPORTED: platform bug breaks ctf_link\n");
  return 0;
#else

  if ((fp = ctf_create (&err)) == NULL)
    goto create_err;

  if ((in1 = ctf_create (&err)) == NULL)
    goto create_err;

  if ((in2 = ctf_create (&err)) == NULL)
    goto create_err;

  /* Force a conflict to get an archive created.  */

  if ((ctf_add_integer (in1, CTF_ADD_ROOT, "foo", &encoding)) == CTF_ERR)
    {
      fprintf (stderr, "Cannot add: %s\n", ctf_errmsg (ctf_errno (in1)));
      return 1;
    }

  if ((ctf_add_integer (in2, CTF_ADD_ROOT, "foo", &encoding2)) == CTF_ERR)
    {
      fprintf (stderr, "Cannot add: %s\n", ctf_errmsg (ctf_errno (in2)));
      return 1;
    }

  /* Write them out and read them back in, to turn them into archives.
     This would be unnecessary if ctf_link_add() were public :( */
  if ((buf1 = ctf_write_mem (in1, &buf1_sz, -1)) == NULL)
    {
      fprintf (stderr, "Cannot serialize: %s\n", ctf_errmsg (ctf_errno (in1)));
      return 1;
    }

  if ((buf2 = ctf_write_mem (in2, &buf2_sz, -1)) == NULL)
    {
      fprintf (stderr, "Cannot serialize: %s\n", ctf_errmsg (ctf_errno (in2)));
      return 1;
    }

  s1.cts_name = "foo";
  s2.cts_name = "bar";
  s1.cts_data = (void *) buf1;
  s2.cts_data = (void *) buf2;
  s1.cts_size = buf1_sz;
  s2.cts_size = buf2_sz;
  s1.cts_entsize = 64; /* Unimportant.  */
  s2.cts_entsize = 64; /* Unimportant.  */

  if ((arc1 = ctf_arc_bufopen (&s1, NULL, NULL, &err)) == NULL ||
      (arc2 = ctf_arc_bufopen (&s2, NULL, NULL, &err)) == NULL)
    goto open_err;

  ctf_dict_close (in1);
  ctf_dict_close (in2);

  /* Link them together.  */

  if (ctf_link_add_ctf (fp, arc1, "a") < 0 ||
      ctf_link_add_ctf (fp, arc2, "b") < 0)
    goto link_err;

  if (ctf_link (fp, 0) < 0)
    goto link_err;

  /* Write them out.  We need a new buf here, because the archives are still
     using the other two bufs.  */

  if ((buf3 = ctf_link_write (fp, &buf3_sz, 1)) == NULL)
    goto link_err;

  /* Read them back in.  */

  s1.cts_data = (void *) buf3;
  s1.cts_size = buf3_sz;

  if ((final_arc = ctf_arc_bufopen (&s1, NULL, NULL, &err)) == NULL)
    goto open_err;

  if (ctf_archive_count (final_arc) != 2)
    {
      fprintf (stderr, "Archive is the wrong length: %zi.\n", ctf_archive_count (final_arc));
      return -1;
    }

  /* Dump the header of each archive member, and search for CTF_F_COMPRESS in
     the resulting dump.  */
  while ((dump_fp = ctf_archive_next (final_arc, &i, NULL, 0, &err)) != NULL)
    {
      char *dumpstr;

      while ((dumpstr = ctf_dump (dump_fp, &dump_state, CTF_SECT_HEADER,
				  NULL, NULL)) != NULL)
	{
	  if (strstr (dumpstr, "CTF_F_COMPRESS") != NULL)
	    printf ("Output is compressed.\n");
	  free (dumpstr);
	}
      ctf_dict_close (dump_fp);
    }
  if (err != ECTF_NEXT_END)
    {
      fprintf (stderr, "Archive iteation error: %s\n", ctf_errmsg (err));
      return 1;
    }

  ctf_arc_close (final_arc);
  free (buf1);
  free (buf2);
  free (buf3);
  ctf_dict_close (fp);
  return 0;

 create_err:
  fprintf (stderr, "Cannot create: %s\n", ctf_errmsg (err));
  return 1;
 open_err:
  fprintf (stderr, "Cannot open: %s\n", ctf_errmsg (err));
  return 1;
 link_err:
  fprintf (stderr, "Cannot link: %s\n", ctf_errmsg (ctf_errno (fp)));
  return 1;
#endif
}
