/* t-mlkem.c - Check the Crystal Kyber computation by Known Answers
 * Copyright (C) 2024 g10 Code GmbH
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "stopwatch.h"

#define PGM "t-mlkem"

#define NEED_SHOW_NOTE
#define NEED_PREPEND_SRCDIR
#define NEED_READ_TEXTLINE
#define NEED_COPY_DATA
#define NEED_HEX2BUFFER
#include "t-common.h"

#define N_TESTS 30

static int custom_data_file;

/*
 * The input line is like:
 *
 *      [Kyber-512]
 *      [Kyber-768]
 *      [Kyber-1024]
 *
 */
static int
parse_annotation (const char *line, int lineno)
{
  const char *s;

  s = strchr (line, '-');
  if (!s)
    {
      fail ("syntax error at input line %d", lineno);
      return 0;
    }

  switch (atoi (s+1))
    {
    case 512:
      return GCRY_KEM_MLKEM512;
      break;
    case 768:
    default:
      return GCRY_KEM_MLKEM768;
      break;
    case 1024:
      return GCRY_KEM_MLKEM1024;
      break;
    }
}

static void
one_test (int testno, int algo,
          const char *sk_str, const char *ct_str, const char *ss_str)
{
  gpg_error_t err;
  unsigned char *sk, *ct, *ss;
  size_t sk_len, ct_len, ss_len;
  unsigned char ss_computed[GCRY_KEM_MLKEM1024_SHARED_LEN];

  sk = ct = ss = 0;

  if (verbose > 1)
    info ("Running test %d\n", testno);

  if (!(sk = hex2buffer (sk_str, &sk_len)))
    {
      fail ("error preparing input for test %d, %s: %s",
            testno, "sk", "invalid hex string");
      goto leave;
    }
  if (!(ct = hex2buffer (ct_str, &ct_len)))
    {
      fail ("error preparing input for test %d, %s: %s",
            testno, "ct", "invalid hex string");
      goto leave;
    }
  if (!(ss = hex2buffer (ss_str, &ss_len)))
    {
      fail ("error preparing input for test %d, %s: %s",
            testno, "ss", "invalid hex string");
      goto leave;
    }

  err = gcry_kem_decap (algo, sk, sk_len, ct, ct_len,
                        ss_computed, ss_len, NULL, 0);
  if (err)
    fail ("gcry_kem_decap failed for test %d: %s", testno, gpg_strerror (err));

  if (memcmp (ss_computed, ss, ss_len) != 0)
    {
      size_t i;

      fail ("test %d failed: mismatch\n", testno);
      fputs ("ss_computed:", stderr);
      for (i = 0; i < ss_len; i++)
        fprintf (stderr, " %02x", ss_computed[i]);
      putc ('\n', stderr);
      fputs ("ss_knownans:", stderr);
      for (i = 0; i < ss_len; i++)
        fprintf (stderr, " %02x", ss[i]);
      putc ('\n', stderr);
    }

 leave:
  xfree (sk);
  xfree (ct);
  xfree (ss);
}

static void
check_mlkem_kat (int algo, const char *fname)
{
  FILE *fp;
  int lineno, ntests;
  char *line;
  int testno;
  char *sk_str, *pk_str, *ct_str, *ss_str;

  info ("Checking ML-KEM.\n");

  fp = fopen (fname, "r");
  if (!fp)
    die ("error opening '%s': %s\n", fname, strerror (errno));

  testno = 0;
  sk_str = pk_str = ct_str = ss_str = NULL;
  lineno = ntests = 0;
  while ((line = read_textline (fp, &lineno)))
    {
      if (!strncmp (line, "[", 1))
        algo = parse_annotation (line, lineno);
      else if (!strncmp (line, "Public Key:", 11))
        copy_data (&pk_str, line, lineno);
      else if (!strncmp (line, "Secret Key:", 11))
        copy_data (&sk_str, line, lineno);
      else if (!strncmp (line, "Ciphertext:", 11))
        copy_data (&ct_str, line, lineno);
      else if (!strncmp (line, "Shared Secret A:", 16))
        copy_data (&ss_str, line, lineno);
      else if (!strncmp (line, "Shared Secret B:", 16))
        ;
      else if (!strncmp (line, "Pseudorandom", 12))
        ;
      else
        fail ("unknown tag at input line %d", lineno);

      xfree (line);
      if (pk_str && sk_str && ct_str && ss_str)
        {
          testno++;
          one_test (testno, algo, sk_str, ct_str, ss_str);
          ntests++;
          if (!(ntests % 256))
            show_note ("%d of %d tests done\n", ntests, N_TESTS);
          xfree (pk_str);  pk_str = NULL;
          xfree (sk_str);  sk_str = NULL;
          xfree (ct_str);  ct_str = NULL;
          xfree (ss_str);  ss_str = NULL;
        }
    }
  xfree (pk_str);
  xfree (sk_str);
  xfree (ct_str);
  xfree (ss_str);

  if (ntests != N_TESTS && !custom_data_file)
    fail ("did %d tests but expected %d", ntests, N_TESTS);
  else if ((ntests % 256))
    show_note ("%d tests done\n", ntests);

  fclose (fp);
}

int
main (int argc, char **argv)
{
  int last_argc = -1;
  char *fname   = NULL;
  int algo = 0;

  if (argc)
    { argc--; argv++; }

  while (argc && last_argc != argc)
    {
      last_argc = argc;
      if (!strcmp (*argv, "--"))
        {
          argc--; argv++;
          break;
        }
      else if (!strcmp (*argv, "--help"))
        {
          fputs ("usage: " PGM " [options]\n"
                 "Options:\n"
                 "  --verbose       print timings etc.\n"
                 "  --debug         flyswatter\n"
                 "  --data FNAME    take test data from file FNAME\n"
                 "  --512           specify Kyber-512\n"
                 "  --768           specify Kyber-768\n"
                 "  --512           specify Kyber-1024\n",
                 stdout);
          exit (0);
        }
      else if (!strcmp (*argv, "--verbose"))
        {
          verbose++;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--debug"))
        {
          verbose += 2;
          debug++;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--512"))
        {
          algo = GCRY_KEM_MLKEM512;
          debug++;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--768"))
        {
          algo = GCRY_KEM_MLKEM768;
          debug++;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--1024"))
        {
          algo = GCRY_KEM_MLKEM1024;
          debug++;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--data"))
        {
          argc--; argv++;
          if (argc)
            {
              xfree (fname);
              fname = xstrdup (*argv);
              argc--; argv++;
            }
        }
      else if (!strncmp (*argv, "--", 2))
        die ("unknown option '%s'", *argv);
    }

  if (!fname)
    fname = prepend_srcdir ("t-mlkem.inp");
  else
    custom_data_file = 1;

  xgcry_control ((GCRYCTL_DISABLE_SECMEM, 0));
  if (!gcry_check_version (GCRYPT_VERSION))
    die ("version mismatch\n");
  if (debug)
    xgcry_control ((GCRYCTL_SET_DEBUG_FLAGS, 1u , 0));
  xgcry_control ((GCRYCTL_ENABLE_QUICK_RANDOM, 0));
  xgcry_control ((GCRYCTL_INITIALIZATION_FINISHED, 0));

  start_timer ();
  check_mlkem_kat (algo, fname);
  stop_timer ();

  xfree (fname);

  info ("All tests completed in %s.  Errors: %d\n",
        elapsed_time (1), error_count);
  return !!error_count;
}
