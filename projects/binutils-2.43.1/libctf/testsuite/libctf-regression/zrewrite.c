/* Make sure that you can modify then ctf_compress_write() a dict
   and it changes after modification.  */

#include <ctf-api.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *read_file(const char *path, size_t *len)
{
  char *in = NULL;
  char buf[4096];
  int foo;
  size_t ret;

  if ((foo = open (path, O_RDONLY)) < 0)
    {
      fprintf (stderr, "error opening %s: %s\n", path, strerror(errno));
      exit (1);
    }

  *len = 0;
  while ((ret = read (foo, buf, 4096)) > 0)
    {
      if ((in = realloc (in, *len + ret)) == NULL)
	{
	  fprintf (stderr, "Out of memory\n");
	  exit (1);
	}

      memcpy (&in[*len], buf, ret);
      *len += ret;
    }

  if (ret < 0)
    {
      fprintf (stderr, "error reading %s: %s\n", path, strerror(errno));
      exit (1);
    }
  close (foo);
  return in;
}

int
main (int argc, char *argv[])
{
  ctf_dict_t *fp, *fp_b;
  ctf_archive_t *ctf, *ctf_b;
  int foo;
  char *a, *b;
  size_t a_len, b_len;
  ctf_id_t type, ptrtype;
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

  if ((foo = open ("tmpdir/one", O_CREAT | O_TRUNC | O_WRONLY, 0666)) < 0)
    goto write_stderr;
  if (ctf_compress_write (fp, foo) < 0)
    goto write_err;
  close (foo);

  if ((foo = open ("tmpdir/two", O_CREAT | O_TRUNC | O_WRONLY, 0666)) < 0)
    goto write_stderr;
  if (ctf_compress_write (fp, foo) < 0)
    goto write_err;
  close (foo);

  a = read_file ("tmpdir/one", &a_len);
  b = read_file ("tmpdir/two", &b_len);

  if (a_len != b_len || memcmp (a, b, a_len) != 0)
    {
      fprintf (stderr, "consecutive compress_writes are different: lengths %zu and %zu\n", a_len, b_len);
      return 1;
    }

  free (b);

  /* Add some new types to the dict and write it out, then read it back in and
     make sure they're still there, and that at least some of the
     originally-present data objects are still there too.  */

  if ((type = ctf_lookup_by_name (fp, "struct a_struct")) == CTF_ERR)
    fprintf (stderr, "Lookup of struct a_struct failed: %s\n", ctf_errmsg (ctf_errno (fp)));

  if ((ptrtype = ctf_add_pointer (fp, CTF_ADD_ROOT, type)) == CTF_ERR)
    fprintf (stderr, "Cannot add pointer to ctf_opened dict: %s\n", ctf_errmsg (ctf_errno (fp)));

  unlink ("tmpdir/two");

  if ((foo = open ("tmpdir/two", O_CREAT | O_TRUNC | O_WRONLY, 0666)) < 0)
    goto write_stderr;
  if (ctf_compress_write (fp, foo) < 0)
    goto write_err;
  close (foo);

  b = read_file ("tmpdir/two", &b_len);

  if (a_len == b_len && memcmp (a, b, b_len) == 0)
    {
      fprintf (stderr, "compress_writes after adding types does not change the dict\n");
      return 1;
    }

  free (a);
  free (b);

  if ((ctf_b = ctf_open ("tmpdir/two", NULL, &err)) == NULL)
    goto open_err;
  if ((fp_b = ctf_dict_open (ctf_b, NULL, &err)) == NULL)
    goto open_err;

  if (ctf_type_reference (fp_b, ptrtype) == CTF_ERR)
    fprintf (stderr, "Lookup of pointer preserved across writeout failed: %s\n", ctf_errmsg (ctf_errno (fp_b)));

  if (ctf_type_reference (fp_b, ptrtype) != type)
    fprintf (stderr, "Look up of newly-added type in serialized dict yields ID %lx, expected %lx\n", ctf_type_reference (fp_b, ptrtype), type);

  if (ctf_lookup_by_symbol_name (fp_b, "an_int") == CTF_ERR)
    fprintf (stderr, "Lookup of symbol an_int failed: %s\n", ctf_errmsg (ctf_errno (fp_b)));

  ctf_dict_close (fp);
  ctf_close (ctf);

  ctf_dict_close (fp_b);
  ctf_close (ctf_b);

  printf ("All done.\n");
  return 0;
 
 open_err:
  fprintf (stderr, "%s: cannot open: %s\n", argv[0], ctf_errmsg (err));
  return 1;
 write_err: 
  fprintf (stderr, "%s: cannot write: %s\n", argv[0], ctf_errmsg (ctf_errno (fp)));
  return 1;
 write_stderr:
  fprintf (stderr, "%s: cannot open for writing: %s\n", argv[0], strerror (errno));
  return 1;
}
