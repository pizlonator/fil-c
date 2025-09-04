/* Make sure that you can modify then ctf_gzwrite() a dict
   and it changes after modification.  */

#include <ctf-api.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

char *read_gz(const char *path, size_t *len)
{
  char *in = NULL;
  char buf[4096];
  gzFile foo;
  size_t ret;

  if ((foo = gzopen (path, "rb")) == NULL)
    return NULL;

  *len = 0;
  while ((ret = gzread (foo, buf, 4096)) > 0)
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
      int errnum;
      const char *err;
      err = gzerror (foo, &errnum);
      if (errnum != Z_ERRNO)
	fprintf (stderr, "error reading %s: %s\n", path, err);
      else
	fprintf (stderr, "error reading %s: %s\n", path, strerror(errno));
      exit (1);
    }
  gzclose (foo);
  return in;
}

int
main (int argc, char *argv[])
{
  ctf_dict_t *fp, *fp_b;
  ctf_archive_t *ctf;
  gzFile foo;
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

  if ((foo = gzopen ("tmpdir/one.gz", "wb")) == NULL)
    goto write_gzerr;
  if (ctf_gzwrite (fp, foo) < 0)
    goto write_err;
  gzclose (foo);

  if ((foo = gzopen ("tmpdir/two.gz", "wb")) == NULL)
    goto write_gzerr;
  if (ctf_gzwrite (fp, foo) < 0)
    goto write_err;
  gzclose (foo);

  if ((a = read_gz ("tmpdir/one.gz", &a_len)) == NULL)
    goto read_err;

  if ((b = read_gz ("tmpdir/two.gz", &b_len)) == NULL)
    goto read_err;

  if (a_len != b_len || memcmp (a, b, a_len) != 0)
    {
      fprintf (stderr, "consecutive gzwrites are different: lengths %zu and %zu\n", a_len, b_len);
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

  unlink ("tmpdir/two.gz");
  if ((foo = gzopen ("tmpdir/two.gz", "wb")) == NULL)
    goto write_gzerr;
  if (ctf_gzwrite (fp, foo) < 0)
    goto write_err;
  gzclose (foo);

  if ((b = read_gz ("tmpdir/two.gz", &b_len)) == NULL)
    goto read_err;

  if (a_len == b_len && memcmp (a, b, b_len) == 0)
    {
      fprintf (stderr, "gzwrites after adding types does not change the dict\n");
      return 1;
    }

  free (a);
  if ((fp_b = ctf_simple_open (b, b_len, NULL, 0, 0, NULL, 0, &err)) == NULL)
    goto open_err;

  if (ctf_type_reference (fp_b, ptrtype) == CTF_ERR)
    fprintf (stderr, "Lookup of pointer preserved across writeout failed: %s\n", ctf_errmsg (ctf_errno (fp_b)));

  if (ctf_type_reference (fp_b, ptrtype) != type)
    fprintf (stderr, "Look up of newly-added type in serialized dict yields ID %lx, expected %lx\n", ctf_type_reference (fp_b, ptrtype), type);

  if (ctf_lookup_by_symbol_name (fp_b, "an_int") == CTF_ERR)
    fprintf (stderr, "Lookup of symbol an_int failed: %s\n", ctf_errmsg (ctf_errno (fp_b)));

  free (b);
  ctf_dict_close (fp);
  ctf_dict_close (fp_b);
  ctf_close (ctf);

  printf ("All done.\n");
  return 0;
 
 open_err:
  fprintf (stderr, "%s: cannot open: %s\n", argv[0], ctf_errmsg (err));
  return 1;
 write_err: 
  fprintf (stderr, "%s: cannot write: %s\n", argv[0], ctf_errmsg (ctf_errno (fp)));
  return 1;
 write_gzerr:
  {
    int errnum;
    const char *err;

    err = gzerror (foo, &errnum);
    if (errnum != Z_ERRNO)
      fprintf (stderr, "error gzwriting: %s\n", err);
    else
      fprintf (stderr, "error gzwriting: %s\n", strerror(errno));
    return 1;
  }
 read_err: 
  fprintf (stderr, "%s: cannot read\n", argv[0]);
  return 1;
}
