/* Make sure linking a non-root-visible type emits a non-root-visible
   type, rather than silently promoting it to root-visible.  Do it by dumping,
   thus also testing the {non-root sigils} you get when dumping
   non-root-visible types.  */

#include <ctf-api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main (int argc, char *argv[])
{
  ctf_dict_t *in1;
  ctf_dict_t *fp;
  ctf_dict_t *dump_fp;
  ctf_archive_t *arc1;
  ctf_archive_t *final_arc;
  ctf_sect_t s;
  ctf_encoding_t encoding = { CTF_INT_SIGNED, 0, sizeof (char) };
  unsigned char *buf1, *buf2;
  size_t buf1_sz, buf2_sz;
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

  /* A non-root addition. */

  if ((ctf_add_integer (in1, CTF_ADD_NONROOT, "foo", &encoding)) == CTF_ERR)
    {
      fprintf (stderr, "Cannot add: %s\n", ctf_errmsg (ctf_errno (in1)));
      return 1;
    }

  /* Write it out and read it back in, to turn it into an archive.
     This would be unnecessary if ctf_link_add() were public :( */
  if ((buf1 = ctf_write_mem (in1, &buf1_sz, -1)) == NULL)
    {
      fprintf (stderr, "Cannot serialize: %s\n", ctf_errmsg (ctf_errno (in1)));
      return 1;
    }

  s.cts_name = "foo";
  s.cts_data = (void *) buf1;
  s.cts_size = buf1_sz;
  s.cts_entsize = 64; /* Unimportant.  */

  if ((arc1 = ctf_arc_bufopen (&s, NULL, NULL, &err)) == NULL)
    goto open_err;

  ctf_dict_close (in1);

  /* Link!  Even a one-file link does deduplication.  */

  if (ctf_link_add_ctf (fp, arc1, "a") < 0)
    goto link_err;

  if (ctf_link (fp, 0) < 0)
    goto link_err;

  /* Write it out.  We need a new buf here, because the archive is still
     using the other buf.  */

  if ((buf2 = ctf_link_write (fp, &buf2_sz, 4096)) == NULL)
    goto link_err;

  /* Read it back in.  */

  s.cts_data = (void *) buf2;
  s.cts_size = buf2_sz;

  if ((final_arc = ctf_arc_bufopen (&s, NULL, NULL, &err)) == NULL)
    goto open_err;

  /* Dump the types, and search for the {sigils of non-rootedness}.  */
  while ((dump_fp = ctf_archive_next (final_arc, &i, NULL, 0, &err)) != NULL)
    {
      char *dumpstr;

      while ((dumpstr = ctf_dump (dump_fp, &dump_state, CTF_SECT_TYPE,
				  NULL, NULL)) != NULL)
	{
	  if (strchr (dumpstr, '{') != NULL && strchr (dumpstr, '}') != NULL)
	    printf ("Non-root type found.\n");
	  free (dumpstr);
	}
      ctf_dict_close (dump_fp);
    }
  if (err != ECTF_NEXT_END)
    {
      fprintf (stderr, "Archive iteration error: %s\n", ctf_errmsg (err));
      return 1;
    }

  ctf_arc_close (final_arc);
  free (buf1);
  free (buf2);
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
