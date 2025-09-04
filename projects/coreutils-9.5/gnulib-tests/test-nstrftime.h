/* Test of nstrftime-like functions.
   Copyright (C) 2011-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Jim Meyering and Bruno Haible.  */

#define FUNC_NAME STRINGIFY(FUNC)
#define STRINGIFY(x) #x

/* Support for settings like TZ='<+00>0' was added in IEEE Std 1003.1-2001.  */
#define TZ_ANGLE_BRACKETS_SHOULD_WORK (200112 <= _POSIX_VERSION)

/* A wrapper around FUNC that checks the return value.  */
static size_t
FUNC_CHECKED (char *restrict s, size_t maxsize,
              char const *format,
              struct tm const *tp, timezone_t tz, int ns)
{
  size_t ret = FUNC (s, maxsize, format, tp, tz, ns);
  if (ret > 0)
    {
      ASSERT (ret < maxsize);
      ASSERT (strlen (s) == ret);
    }
  return ret;
}

/* -------------------------------------------------------------------------- */

struct posixtm_test
{
  time_t in;
  int in_ns;
  char const *fmt;
  char const *exp;
};

static struct posixtm_test const T[] =
  {
    { 1300000000, 0,            "%F", "2011-03-13" },
    { 0,          10,           "%T.%N", "00:00:00.000000010" },
    { 56,         123456789,    "%T.%12N", "00:00:56.123456789000" },
    { 0,          0,            NULL, NULL }
  };

static int
posixtm_test (void)
{
  int fail = 0;
  unsigned int i;

  for (i = 0; T[i].fmt; i++)
    {
      char buf[1000];
      time_t t = T[i].in;
      struct tm *tm = gmtime (&t);
      size_t n;

      ASSERT (tm);

      n = FUNC_CHECKED (buf, sizeof buf, T[i].fmt, tm, 0, T[i].in_ns);
      if (n == 0)
        {
          fail = 1;
          printf ("%s failed with format %s\n", FUNC_NAME, T[i].fmt);
        }

      if (! STREQ (buf, T[i].exp))
        {
          fail = 1;
          printf ("%s: result mismatch: got %s, expected %s\n",
                  T[i].fmt, buf, T[i].exp);
        }
    }

  return fail;
}

/* -------------------------------------------------------------------------- */

struct tzalloc_test
{
  timezone_t tz;
  char const *setting;
};

static struct tzalloc_test TZ[] =
  {
#define Pacific 0
    { 0, "PST8PDT,M3.2.0,M11.1.0"      },
#define Arizona 1
    { 0, "MST7"                        },
#define UTC 2
    { 0, 0                             },
#define CentEur 3
    { 0, "CET-1CEST,M3.5.0,M10.5.0/3"  },
#define Japan 4
    { 0, "JST-9"                       },
#define NZ 5
    { 0, "NZST-12NZDT,M9.5.0,M4.1.0/3" },
#define Unknown 6
    { 0, "<-00>0" },
    { 0 }
  };

struct localtime_rz_test
{
  /* Input parameters.  */
  struct tzalloc_test *tza;
  time_t t;

  /* Expected result.  */
  char const *exp;

  /* Determines if an incorrectly unset tm_isdst
     results in failure or just a warning.  */
  int ahistorical;
};

static struct localtime_rz_test LT[] =
  {
#if !(defined __DragonFly__ || defined __NetBSD__ || defined __ANDROID__)
    { TZ+Pacific,          0, "1969-12-31 16:00:00 -0800 (PST)",  0 },
#endif
    { TZ+Arizona,          0, "1969-12-31 17:00:00 -0700 (MST)",  0 },
    { TZ+UTC    ,          0, "1970-01-01 00:00:00 +0000 (UTC)",  0 },
#if !(defined __DragonFly__ || defined __NetBSD__ || defined __ANDROID__)
    { TZ+CentEur,          0, "1970-01-01 01:00:00 +0100 (CET)",  0 },
#endif
    { TZ+Japan  ,          0, "1970-01-01 09:00:00 +0900 (JST)",  0 },
#if !defined __FreeBSD__
    { TZ+NZ     ,          0, "1970-01-01 13:00:00 +1300 (NZDT)", 1 },
#endif
    { TZ+Pacific,  500000001, "1985-11-04 16:53:21 -0800 (PST)",  0 },
    { TZ+Arizona,  500000001, "1985-11-04 17:53:21 -0700 (MST)",  0 },
    { TZ+UTC    ,  500000001, "1985-11-05 00:53:21 +0000 (UTC)",  0 },
    { TZ+CentEur,  500000001, "1985-11-05 01:53:21 +0100 (CET)",  1 },
    { TZ+Japan  ,  500000001, "1985-11-05 09:53:21 +0900 (JST)",  0 },
    { TZ+NZ     ,  500000001, "1985-11-05 13:53:21 +1300 (NZDT)", 0 },
    { TZ+Pacific, 1000000002, "2001-09-08 18:46:42 -0700 (PDT)",  0 },
    { TZ+Arizona, 1000000002, "2001-09-08 18:46:42 -0700 (MST)",  0 },
    { TZ+UTC    , 1000000002, "2001-09-09 01:46:42 +0000 (UTC)",  0 },
    { TZ+CentEur, 1000000002, "2001-09-09 03:46:42 +0200 (CEST)", 0 },
    { TZ+Japan  , 1000000002, "2001-09-09 10:46:42 +0900 (JST)",  0 },
    { TZ+NZ     , 1000000002, "2001-09-09 13:46:42 +1200 (NZST)", 0 },
#if TZ_ANGLE_BRACKETS_SHOULD_WORK && !defined __FreeBSD__
    { TZ+Unknown,          0, "1970-01-01 00:00:00 -0000 (-00)",  0 },
    { TZ+Unknown,  500000001, "1985-11-05 00:53:21 -0000 (-00)",  0 },
    { TZ+Unknown, 1000000002, "2001-09-09 01:46:42 -0000 (-00)",  0 },
#endif
    { 0 }
  };

static int
tzalloc_test (void)
{
  int fail = 0;
  int i;

  for (i = 0; LT[i].tza; i++)
    {
      struct tzalloc_test *tza = LT[i].tza;
      long lt = LT[i].t;
      timezone_t tz = tza->tz;
      char const *setting;
      static char const format[] = "%Y-%m-%d %H:%M:%S %z (%Z)";
      char buf[1000];
      struct tm tm;
      size_t n;

      if (!tz && tza->setting)
        {
          tz = tzalloc (tza->setting);
          if (!tz)
            {
              fail = 1;
              printf ("%s: tzalloc: %s\n", TZ[i].setting, strerror (errno));
              continue;
            }
          tza->tz = tz;
        }

      setting = tza->setting ? tza->setting : "UTC0";

      if (!localtime_rz (tz, &LT[i].t, &tm))
        {
          fail = 1;
          printf ("%s: %ld: localtime_rz: %s\n", setting, lt,
                  strerror (errno));
          continue;
        }

      n = FUNC_CHECKED (buf, sizeof buf, format, &tm, tz, 0);
      if (n == 0)
        {
          fail = 1;
          printf ("%s: %ld: %s failed\n", setting, lt, FUNC_NAME);
          continue;
        }

      if (! (STREQ (buf, LT[i].exp)
             || (!tz && n == strlen (LT[i].exp)
                 && memcmp (buf, LT[i].exp, n - sizeof "(GMT)" + 1) == 0
                 && STREQ (buf + n - sizeof "(GMT)" + 1, "(GMT)"))))
        {
          /* Don't fail for unhandled dst in ahistorical entries,
             as gnulib doesn't currently fix that issue, seen on Darwin 14.  */
          if (!LT[i].ahistorical || tm.tm_isdst)
            fail = 1;
          printf ("%s: expected \"%s\", got \"%s\"\n",
                  setting, LT[i].exp, buf);
        }
    }

  return fail;
}

/* -------------------------------------------------------------------------- */

static int
quarter_test (void)
{
  int fail = 0;
  size_t mon;

  /* Check %q.  */
  for (mon = 1; mon <= 12; mon++)
    {
      char out[2];
      char exp[2] = {0,};
      struct tm qtm = { .tm_mon = mon - 1 };
      char fmt[3] = {'%','q','\0'};

      size_t r = FUNC_CHECKED (out, sizeof (out), fmt, &qtm, 0, 0);
      if (r == 0)
        {
          printf ("%s(\"%%q\") failed\n", FUNC_NAME);
          fail = 1;
          break;
        }

      exp[0] = mon < 4 ? '1' : mon < 7 ? '2' : mon < 10 ? '3' : '4';
      if (strcmp (out, exp) != 0)
        {
          printf ("%s %%q: expected \"%s\", got \"%s\"\n", FUNC_NAME, exp, out);
          fail = 1;
          break;
        }
    }

  return fail;
}

/* -------------------------------------------------------------------------- */

static int
errno_test (void)
{
  int fail = 0;
  struct tm tm = { .tm_year = 2020 - 1900, .tm_mday = 1 };
  char buf[INT_BUFSIZE_BOUND (time_t)];
  size_t n;
  int bigyear = LLONG_MAX - 1900 < INT_MAX ? LLONG_MAX - 1900 : INT_MAX;

  errno = 0;
  n = FUNC_CHECKED (buf, 0, "%m", &tm, 0, 0);
  if (! (n == 0 && errno == ERANGE))
    {
      fail = 1;
      printf ("%s failed to set errno = ERANGE\n", FUNC_NAME);
    }

  errno = 0;
  n = FUNC_CHECKED (buf, sizeof buf, "", &tm, 0, 0);
  if (! (n == 0 && errno == 0))
    {
      fail = 1;
      printf ("%s failed to leave errno alone\n", FUNC_NAME);
    }


  tm.tm_year = bigyear;
  errno = 0;
  n = FUNC_CHECKED (buf, sizeof buf, "%s", &tm, 0, 0);
  if (n == 0)
    {
      if (errno != EOVERFLOW)
        {
          fail = 1;
          printf ("%s failed to set errno = EOVERFLOW\n", FUNC_NAME);
        }

      if (mktime_z (0, &tm) != (time_t) -1)
        {
          fail = 1;
          printf ("%s %%s failed but mktime_z worked for tm_year=%d\n",
                  FUNC_NAME, bigyear);
        }
    }
  else
    {
      long long int text_seconds = atoll (buf);
      if (text_seconds <= (LLONG_MAX - 1 < TYPE_MAXIMUM (time_t)
                           ? LLONG_MAX - 1 : TYPE_MAXIMUM (time_t)))
        {
          time_t bigtime = text_seconds;
          struct tm *tmp = gmtime (&bigtime);
          if (!tmp)
            {
              fail = 1;
              printf ("gmtime failed on %s result\n", FUNC_NAME);
            }
          else
            {
              char buf1[sizeof buf];
              size_t n1 = FUNC_CHECKED (buf1, sizeof buf1, "%s", tmp, 0, 0);
              buf1[n1] = '\0';
              if (! STREQ (buf, buf1))
                {
                  fail = 1;
                  printf ("%s %%s first returned '%s', then '%s'\n",
                          FUNC_NAME, buf, buf1);
                }
            }
        }
    }

  return fail;
}

/* -------------------------------------------------------------------------- */

/* Test various format directives.  */

typedef enum { english, french } language_t;

static int
locales_test (language_t language)
{
  int fail = 0;

  time_t t = 1509000003;
  struct tm *tm = gmtime (&t);
  int ns = 123456789;
  char buf[100];
  size_t n;

  n = FUNC_CHECKED (buf, sizeof buf, "%+4Y-%m-%d %H:%M:%S.%N", tm, 0, ns);
  ASSERT (n > 0);
  printf ("Format as ISO 8601: %s\n", buf);
  ASSERT (STREQ (buf, "2017-10-26 06:40:03.123456789"));

  /* Exercise various POSIX format directives.  */

  n = FUNC_CHECKED (buf, sizeof buf, "%a", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%a directive: %s\n", buf);
  switch (language)
    {
    case english:
      ASSERT (STREQ (buf, "Thu"));
      break;
    case french:
      ASSERT (STREQ (buf, "jeu.") /* glibc, FreeBSD, NetBSD, Solaris, Cygwin */
              || STREQ (buf, "Jeu") /* macOS, older FreeBSD */);
      break;
    }

  n = FUNC_CHECKED (buf, sizeof buf, "%A", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%A directive: %s\n", buf);
  switch (language)
    {
    case english:
      ASSERT (STREQ (buf, "Thursday"));
      break;
    case french:
      ASSERT (STREQ (buf, "jeudi") /* glibc, FreeBSD, NetBSD, Solaris, Cygwin */
              || STREQ (buf, "Jeudi") /* macOS, older FreeBSD */);
      break;
    }

  n = FUNC_CHECKED (buf, sizeof buf, "%b", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%b directive: %s\n", buf);
  switch (language)
    {
    case english:
      ASSERT (STREQ (buf, "Oct"));
      break;
    case french:
      ASSERT (STREQ (buf, "oct.") /* glibc, FreeBSD, NetBSD, Solaris, Cygwin */
              || STREQ (buf, "oct") /* macOS, older FreeBSD */);
      break;
    }

  n = FUNC_CHECKED (buf, sizeof buf, "%B", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%B directive: %s\n", buf);
  switch (language)
    {
    case english:
      ASSERT (STREQ (buf, "October"));
      break;
    case french:
      ASSERT (STREQ (buf, "octobre"));
      break;
    }

  n = FUNC_CHECKED (buf, sizeof buf, "%c", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%c directive: %s\n", buf);
  switch (language)
    {
    case english:
      ASSERT (STREQ (buf, "Thu Oct 26 06:40:03 2017"));
      break;
    case french:
      ASSERT (STREQ (buf, "jeu. 26 oct. 2017 06:40:03") /* glibc, Cygwin */
              || STREQ (buf, "jeu. 26 oct. 06:40:03 2017") /* FreeBSD */
              || STREQ (buf, "Jeu 26 oct 06:40:03 2017") /* macOS, older FreeBSD */
              || STREQ (buf, "26 octobre 2017 06:40:03") /* NetBSD */
              || STREQ (buf, "26 octobre 2017 à 06:40:03") /* Solaris (UTF-8) */
              || STREQ (buf, "26 octobre 2017 \340 06:40:03") /* Solaris (ISO-8859-1) */);
      break;
    }

  n = FUNC_CHECKED (buf, sizeof buf, "%C", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%C directive: %s\n", buf);
  ASSERT (STREQ (buf, "20"));

  n = FUNC_CHECKED (buf, sizeof buf, "%d", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%d directive: %s\n", buf);
  ASSERT (STREQ (buf, "26"));

  n = FUNC_CHECKED (buf, sizeof buf, "%D", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%D directive: %s\n", buf);
  ASSERT (STREQ (buf, "10/26/17"));

  n = FUNC_CHECKED (buf, sizeof buf, "%e", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%e directive: %s\n", buf);
  ASSERT (STREQ (buf, "26"));

  n = FUNC_CHECKED (buf, sizeof buf, "%F", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%F directive: %s\n", buf);
  ASSERT (STREQ (buf, "2017-10-26"));

  n = FUNC_CHECKED (buf, sizeof buf, "%g", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%g directive: %s\n", buf);
  ASSERT (STREQ (buf, "17"));

  n = FUNC_CHECKED (buf, sizeof buf, "%G", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%G directive: %s\n", buf);
  ASSERT (STREQ (buf, "2017"));

  n = FUNC_CHECKED (buf, sizeof buf, "%h", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%h directive: %s\n", buf);
  switch (language)
    {
    case english:
      ASSERT (STREQ (buf, "Oct"));
      break;
    case french:
      ASSERT (STREQ (buf, "oct.") /* glibc, FreeBSD, NetBSD, Solaris, Cygwin */
              || STREQ (buf, "oct") /* macOS, older FreeBSD */);
      break;
    }

  n = FUNC_CHECKED (buf, sizeof buf, "%H", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%H directive: %s\n", buf);
  ASSERT (STREQ (buf, "06"));

  n = FUNC_CHECKED (buf, sizeof buf, "%I", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%I directive: %s\n", buf);
  ASSERT (STREQ (buf, "06"));

  n = FUNC_CHECKED (buf, sizeof buf, "%j", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%j directive: %s\n", buf);
  ASSERT (STREQ (buf, "299"));

  n = FUNC_CHECKED (buf, sizeof buf, "%m", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%m directive: %s\n", buf);
  ASSERT (STREQ (buf, "10"));

  n = FUNC_CHECKED (buf, sizeof buf, "%M", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%M directive: %s\n", buf);
  ASSERT (STREQ (buf, "40"));

  n = FUNC_CHECKED (buf, sizeof buf, "%n", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%n directive: %s\n", buf);
  ASSERT (STREQ (buf, "\n"));

  n = FUNC_CHECKED (buf, sizeof buf, "%p", tm, 0, ns);
  switch (language)
    {
    case english:
      ASSERT (n > 0);
      printf ("%%p directive: %s\n", buf);
      ASSERT (STREQ (buf, "AM"));
      break;
    case french:
      ASSERT (n == 0);
      break;
    }

  n = FUNC_CHECKED (buf, sizeof buf, "%r", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%r directive: %s\n", buf);
  switch (language)
    {
    case english:
      ASSERT (STREQ (buf, "06:40:03 AM"));
      break;
    case french:
      ASSERT (STREQ (buf, "06:40:03 ") /* glibc */
              || STREQ (buf, "06:40:03") /* NetBSD, Cygwin */
              || STREQ (buf, " 6:40:03") /* Solaris */);
      break;
    }

  n = FUNC_CHECKED (buf, sizeof buf, "%R", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%R directive: %s\n", buf);
  ASSERT (STREQ (buf, "06:40"));

  n = FUNC_CHECKED (buf, sizeof buf, "%S", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%S directive: %s\n", buf);
  ASSERT (STREQ (buf, "03"));

  n = FUNC_CHECKED (buf, sizeof buf, "%t", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%t directive: %s\n", buf);
  ASSERT (STREQ (buf, "\t"));

  n = FUNC_CHECKED (buf, sizeof buf, "%T", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%T directive: %s\n", buf);
  ASSERT (STREQ (buf, "06:40:03"));

  n = FUNC_CHECKED (buf, sizeof buf, "%u", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%u directive: %s\n", buf);
  ASSERT (STREQ (buf, "4"));

  n = FUNC_CHECKED (buf, sizeof buf, "%U", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%U directive: %s\n", buf);
  ASSERT (STREQ (buf, "43"));

  n = FUNC_CHECKED (buf, sizeof buf, "%V", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%V directive: %s\n", buf);
  ASSERT (STREQ (buf, "43"));

  n = FUNC_CHECKED (buf, sizeof buf, "%w", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%w directive: %s\n", buf);
  ASSERT (STREQ (buf, "4"));

  n = FUNC_CHECKED (buf, sizeof buf, "%W", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%W directive: %s\n", buf);
  ASSERT (STREQ (buf, "43"));

  n = FUNC_CHECKED (buf, sizeof buf, "%x", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%x directive: %s\n", buf);
  switch (language)
    {
    case english:
      ASSERT (STREQ (buf, "10/26/17"));
      break;
    case french:
      ASSERT (STREQ (buf, "26/10/2017") /* glibc, NetBSD, Solaris, Cygwin */
              || STREQ (buf, "26.10.2017") /* FreeBSD, macOS */);
      break;
    }

  n = FUNC_CHECKED (buf, sizeof buf, "%X", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%X directive: %s\n", buf);
  ASSERT (STREQ (buf, "06:40:03"));

  n = FUNC_CHECKED (buf, sizeof buf, "%y", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%y directive: %s\n", buf);
  ASSERT (STREQ (buf, "17"));

  n = FUNC_CHECKED (buf, sizeof buf, "%Y", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%Y directive: %s\n", buf);
  ASSERT (STREQ (buf, "2017"));

  n = FUNC_CHECKED (buf, sizeof buf, "%z", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%z directive: %s\n", buf);
  ASSERT (STREQ (buf, "+0000"));

  n = FUNC_CHECKED (buf, sizeof buf, "%Z", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%Z directive: %s\n", buf);
  ASSERT (STREQ (buf, "GMT") /* glibc, NetBSD, OpenBSD, AIX, Solaris, Cygwin, Android */
          || STREQ (buf, "UTC") /* musl, macOS, FreeBSD */);

  n = FUNC_CHECKED (buf, sizeof buf, "%%", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%%% directive: %s\n", buf);
  ASSERT (STREQ (buf, "%"));

  /* Exercise various GNU extensions from glibc.  */

  n = FUNC_CHECKED (buf, sizeof buf, "%k", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%k directive: %s\n", buf);
  ASSERT (STREQ (buf, " 6"));

  n = FUNC_CHECKED (buf, sizeof buf, "%l", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%l directive: %s\n", buf);
  ASSERT (STREQ (buf, " 6"));

  n = FUNC_CHECKED (buf, sizeof buf, "%P", tm, 0, ns);
  switch (language)
    {
    case english:
      ASSERT (n > 0);
      printf ("%%P directive: %s\n", buf);
      ASSERT (STREQ (buf, "am"));
      break;
    case french:
      ASSERT (n == 0);
      break;
    }

  n = FUNC_CHECKED (buf, sizeof buf, "%s", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%s directive: %s\n", buf);
  ASSERT (STREQ (buf, "1509000003"));

  /* Exercise various GNU extensions from gnulib.  */

  n = FUNC_CHECKED (buf, sizeof buf, "%N", tm, 0, ns);
  ASSERT (n > 0);
  printf ("%%N directive: %s\n", buf);
  ASSERT (STREQ (buf, "123456789"));

  return fail;
}

/* -------------------------------------------------------------------------- */

/*
Local Variables:
indent-tabs-mode: nil
End:
*/
