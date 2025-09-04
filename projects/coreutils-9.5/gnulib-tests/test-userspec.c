/* Test userspec.c
   Copyright (C) 2009-2024 Free Software Foundation, Inc.

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

/* Written by Jim Meyering.  */

#include <config.h>

#include "userspec.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "xalloc.h"

#define ARRAY_CARDINALITY(Array) (sizeof (Array) / sizeof *(Array))

struct test
{
  char in[100];
  uid_t uid;
  gid_t gid;
  const char *user_name;
  const char *group_name;
  const char *result;
};

static struct test T[] =
  {
    { "",                       -1, -1, "",   "",   NULL},
    { ":",                      -1, -1, "",   "",   NULL},
    { "+0:+0",                   0,  0, "",   "",   NULL},
    { "+0:+0",                   0,  0, "",   "",   NULL},
    { ":+1",                    -1,  1, "",   "",   NULL},
    { "+1",                      1, -1, "",   "",   NULL},
    { ":+0",                    -1,  0, "",   "",   NULL},
    { "+22:+42",                22, 42, "",   "",   NULL},
    /* (uint32_t)-1 should be invalid everywhere */
    { "+4294967295:+4294967295", 0,  0, NULL, NULL, "invalid user"},
    /* likewise, but with only the group being invalid */
    { "+0:+4294967295",          0,  0, NULL, NULL, "invalid group"},
    { ":+4294967295",            0,  0, NULL, NULL, "invalid group"},
    /* and only the user being invalid */
    { "+4294967295:+0",          0,  0, NULL, NULL, "invalid user"},
    /* and using 2^32 */
    { "+4294967296:+4294967296", 0,  0, NULL, NULL, "invalid user"},
    { "+0:+4294967296",          0,  0, NULL, NULL, "invalid group"},
    { ":+4294967296",            0,  0, NULL, NULL, "invalid group"},
    { "+4294967296:+0",          0,  0, NULL, NULL, "invalid user"},
    /* numeric user and no group is invalid */
    { "+4294967295:",            0,  0, NULL, NULL, "invalid spec"},
    { "+4294967296:",            0,  0, NULL, NULL, "invalid spec"},
    { "+1:",                     0,  0, NULL, NULL, "invalid spec"},
    { "+0:",                     0,  0, NULL, NULL, "invalid spec"},

    /* "username:" must expand to UID:GID where GID is username's login group */
    /* Add an entry like the following to the table, if possible.
    { "U_NAME:",              UID,GID, U_NAME, G_NAME, NULL}, */
    { "" /* placeholder */,    -1, -1,     "",     "", NULL},
  };

#define STREQ(a, b) (strcmp (a, b) == 0)

static char const *
maybe_null (char const *s)
{
  return s ? s : "NULL";
}

static bool
same_diag (char const *s, char const *t)
{
  if (s == NULL && t == NULL)
    return true;
  if (s == NULL || t == NULL)
    return false;
  return STREQ (s, t);
}

int
main (void)
{
  unsigned int i;
  int fail = 0;

  /* Find a UID that has both a user name and login group name,
     but skip UID 0.  */
  {
    uid_t uid;
    for (uid = 1200; 0 < uid; uid--)
      {
        struct group *gr;
        struct passwd *pw = getpwuid (uid);
        unsigned int j;
        size_t len;
        if (!pw || !pw->pw_name || !(gr = getgrgid (pw->pw_gid)) || !gr->gr_name)
          continue;
        j = ARRAY_CARDINALITY (T) - 1;
        len = strlen (pw->pw_name);
        if (sizeof T[j].in - 2 < len)
          continue;

        /* Store "username:" in T[j].in.  */
        memcpy(T[j].in, pw->pw_name, len);
        strcpy(T[j].in + len, ":");

        T[j].uid = uid;
        T[j].gid = gr->gr_gid;
        T[j].user_name = xstrdup (pw->pw_name);
        T[j].group_name = xstrdup (gr->gr_name);
        T[j].result = NULL;
        break;
      }
  }

  char *user_name = NULL;
  char *group_name = NULL;

  for (i = 0; i < ARRAY_CARDINALITY (T); i++)
    {
      uid_t uid = (uid_t) -1;
      gid_t gid = (gid_t) -1;
      free (user_name);
      free (group_name);
      char const *diag = parse_user_spec (T[i].in, &uid, &gid,
                                          &user_name, &group_name);
      if (!same_diag (diag, T[i].result))
        {
          printf ("%s return value mismatch: got %s, expected %s\n",
                  T[i].in, maybe_null (diag), maybe_null (T[i].result));
          fail = 1;
          continue;
        }

      if (diag)
        continue;

      if (uid != T[i].uid || gid != T[i].gid)
        {
          printf ("%s mismatch (-: expected uid,gid; +:actual)\n"
                  "-%3lu,%3lu\n+%3lu,%3lu\n",
                  T[i].in,
                  (unsigned long int) T[i].uid,
                  (unsigned long int) T[i].gid,
                  (unsigned long int) uid,
                  (unsigned long int) gid);
          fail = 1;
        }

      if (T[i].result)
        {
          /* Expected a non-NULL result diagnostic, yet got NULL.  */
          diag = "NULL";
          printf ("%s diagnostic mismatch (-: expected diagnostic; +:actual)\n"
                  "-%s\n+%s\n", T[i].in, T[i].result, diag);
          fail = 1;
        }
      else
        {
          /* Should get the same result, but with a warning, if replacing
             ':' with '.'.  */
          char *colon = strchr (T[i].in, ':');
          if (colon)
            {
              *colon = '.';
              uid_t uid2 = -1;
              gid_t gid2 = -1;
              char *user_name2 = NULL;
              char *group_name2 = NULL;
              bool warn;
              if (! (parse_user_spec_warn (T[i].in, &uid2, &gid2,
                                           &user_name2, &group_name2, &warn)
                     && warn))
                printf ("%s did not warn\n", T[i].in);
              else if (! (uid == uid2 && gid == gid2
                          && !!user_name == !!user_name2
                          && (!user_name || STREQ (user_name, user_name2))
                          && !!group_name == !!group_name2
                          && (!group_name || STREQ (group_name, group_name2))))
                printf ("%s treated differently than with colon\n", T[i].in);

              free (user_name2);
              free (group_name2);
            }
        }
    }

  /* Ensure NULL parameters are ignored.  */
  {
    uid_t uid = (uid_t) -1;
    char const *diag = parse_user_spec ("", &uid, NULL, NULL, NULL);
    if (diag)
      {
        printf ("unexpected error: %s\n", diag);
        fail = 1;
      }
  }

  return fail;
}

/*
Local Variables:
indent-tabs-mode: nil
End:
*/
