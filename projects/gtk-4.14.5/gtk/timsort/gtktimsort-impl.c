/*
 * Copyright (C) 2020 Benjamin Otte
 * Copyright (C) 2011 Patrick O. Perry
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NAME
#define NAME WIDTH
#endif

#define DEFINE_TEMP(temp) gpointer temp = g_alloca (WIDTH)
#define ASSIGN(x, y) memcpy (x, y, WIDTH)
#define INCPTR(x) ((gpointer) ((char *) (x) + WIDTH))
#define DECPTR(x) ((gpointer) ((char *) (x) - WIDTH))
#define ELEM(a, i) ((char *) (a) + (i) * WIDTH)
#define LEN(n) ((n) * WIDTH)

#define CONCAT(x, y) gtk_tim_sort_ ## x ## _ ## y
#define MAKE_STR(x, y) CONCAT (x, y)
#define gtk_tim_sort(x) MAKE_STR (x, NAME)

/*
 * Reverse the specified range of the specified array.
 *
 * @param a the array in which a range is to be reversed
 * @param hi the index after the last element in the range to be reversed
 */
static void gtk_tim_sort(reverse_range) (GtkTimSort *self,
                                         gpointer    a,
                                         gsize       hi)
{
  DEFINE_TEMP (t);
  char *front = a;
  char *back = ELEM (a, hi - 1);

  g_assert (hi > 0);

  while (front < back)
    {
      ASSIGN (t, front);
      ASSIGN (front, back);
      ASSIGN (back, t);
      front = INCPTR (front);
      back = DECPTR (back);
    }
}

/*
 * Returns the length of the run beginning at the specified position in
 * the specified array and reverses the run if it is descending (ensuring
 * that the run will always be ascending when the method returns).
 *
 * A run is the longest ascending sequence with:
 *
 *    a[0] <= a[1] <= a[2] <= ...
 *
 * or the longest descending sequence with:
 *
 *    a[0] >  a[1] >  a[2] >  ...
 *
 * For its intended use in a stable mergesort, the strictness of the
 * definition of "descending" is needed so that the call can safely
 * reverse a descending sequence without violating stability.
 *
 * @param a the array in which a run is to be counted and possibly reversed
 * @param hi index after the last element that may be contained in the run.
 *        It is required that {@code 0 < hi}.
 * @param compare the comparator to used for the sort
 * @return  the length of the run beginning at the specified position in
 *          the specified array
 */
static gsize
gtk_tim_sort(prepare_run) (GtkTimSort    *self,
                           GtkTimSortRun *out_change)
{
  gsize run_hi = 1;
  char *cur;
  char *next;

  if (self->size <= run_hi)
    {
      gtk_tim_sort_set_change (out_change, NULL, 0);
      return self->size;
    }

  cur = INCPTR (self->base);
  next = INCPTR (cur);
  run_hi++;

  /* Find end of run, and reverse range if descending */
  if (gtk_tim_sort_compare (self, cur, self->base) < 0)          /* Descending */
    {
      while (run_hi < self->size && gtk_tim_sort_compare (self, next, cur) < 0)
        {
          run_hi++;
          cur = next;
          next = INCPTR (next);
        }
      gtk_tim_sort(reverse_range) (self, self->base, run_hi);
      gtk_tim_sort_set_change (out_change, self->base, run_hi);
    }
  else                          /* Ascending */
    {
      while (run_hi < self->size && gtk_tim_sort_compare (self, next, cur) >= 0)
        {
          run_hi++;
          cur = next;
          next = INCPTR (next);
        }
      gtk_tim_sort_set_change (out_change, NULL, 0);
    }

  return run_hi;
}

/*
 * Sorts the specified portion of the specified array using a binary
 * insertion sort.  This is the best method for sorting small numbers
 * of elements.  It requires O(n log n) compares, but O(n^2) data
 * movement (worst case).
 *
 * If the initial part of the specified range is already sorted,
 * this method can take advantage of it: the method assumes that the
 * elements from index {@code lo}, inclusive, to {@code start},
 * exclusive are already sorted.
 *
 * @param a the array in which a range is to be sorted
 * @param hi the index after the last element in the range to be sorted
 * @param start the index of the first element in the range that is
 *        not already known to be sorted ({@code lo <= start <= hi})
 */
static void gtk_tim_sort(binary_sort) (GtkTimSort    *self,
                                       gpointer       a,
                                       gsize          hi,
                                       gsize          start,
                                       GtkTimSortRun *inout_change)
{
  DEFINE_TEMP (pivot);
  char *startp;
  char *change_min = ELEM (a, hi);
  char *change_max = a;

  g_assert (start <= hi);

  if (start == 0)
    start++;

  startp = ELEM (a, start);

  for (; start < hi; start++, startp = INCPTR (startp))
    {
      /* Set left (and right) to the index where a[start] (pivot) belongs */
      char *leftp = a;
      gsize right = start;
      gsize n;

      /*
       * Invariants:
       *   pivot >= all in [0, left).
       *   pivot <  all in [right, start).
       */
      while (0 < right)
        {
          gsize mid = right >> 1;
          gpointer midp = ELEM (leftp, mid);
          if (gtk_tim_sort_compare (self, startp, midp) < 0)
            {
              right = mid;
            }
          else
            {
              leftp = INCPTR (midp);
              right -= (mid + 1);
            }
        }
      g_assert (0 == right);

      /*
       * The invariants still hold: pivot >= all in [lo, left) and
       * pivot < all in [left, start), so pivot belongs at left.  Note
       * that if there are elements equal to pivot, left points to the
       * first slot after them -- that's why this sort is stable.
       * Slide elements over to make room to make room for pivot.
       */
      n = startp - leftp;               /* The number of bytes to move */
      if (n == 0)
        continue;

      ASSIGN (pivot, startp);
      memmove (INCPTR (leftp), leftp, n);         /* POP: overlaps */

      /* a[left] = pivot; */
      ASSIGN (leftp, pivot);

      change_min = MIN (change_min, leftp);
      change_max = MAX (change_max, ELEM (startp, 1));
    }

  if (change_max > (char *) a)
    {
      g_assert (change_min < ELEM (a, hi));
      if (inout_change && inout_change->len)
        {
          change_max = MAX (change_max, ELEM (inout_change->base, inout_change->len));
          change_min = MIN (change_min, (char *) inout_change->base);
        }
      gtk_tim_sort_set_change (inout_change, change_min, (change_max - change_min) / WIDTH);
    }
}

static gboolean
gtk_tim_sort(merge_append) (GtkTimSort    *self,
                            GtkTimSortRun *out_change)
{
  /* Identify next run */
  gsize run_len;

  run_len = gtk_tim_sort(prepare_run) (self, out_change);
  if (run_len == 0)
    return FALSE;

  /* If run is short, extend to min(self->min_run, self->size) */
  if (run_len < self->min_run)
    {
      gsize force = MIN (self->size, self->min_run);
      gtk_tim_sort(binary_sort) (self, self->base, force, run_len, out_change);
      run_len = force;
    }
  /* Push run onto pending-run stack, and maybe merge */
  gtk_tim_sort_push_run (self, self->base, run_len);

  return TRUE;
}

/*
 * Locates the position at which to insert the specified key into the
 * specified sorted range; if the range contains an element equal to key,
 * returns the index of the leftmost equal element.
 *
 * @param key the key whose insertion point to search for
 * @param base the array in which to search
 * @param len the length of the range; must be > 0
 * @param hint the index at which to begin the search, 0 <= hint < n.
 *     The closer hint is to the result, the faster this method will run.
 * @param c the comparator used to order the range, and to search
 * @return the int k,  0 <= k <= n such that a[b + k - 1] < key <= a[b + k],
 *    pretending that a[b - 1] is minus infinity and a[b + n] is infinity.
 *    In other words, key belongs at index b + k; or in other words,
 *    the first k elements of a should precede key, and the last n - k
 *    should follow it.
 */
static gsize
gtk_tim_sort(gallop_left) (GtkTimSort *self,
                           gpointer    key,
                           gpointer    base,
                           gsize       len,
                           gsize       hint)
{
  char *hintp = ELEM (base, hint);
  gsize last_ofs = 0;
  gsize ofs = 1;

  g_assert (len > 0 && hint < len);
  if (gtk_tim_sort_compare (self, key, hintp) > 0)
    {
      /* Gallop right until a[hint+last_ofs] < key <= a[hint+ofs] */
      gsize max_ofs = len - hint;
      while (ofs < max_ofs
             && gtk_tim_sort_compare (self, key, ELEM (hintp, ofs)) > 0)
        {
          last_ofs = ofs;
          ofs = (ofs << 1) + 1;                 /* eventually this becomes SIZE_MAX */
        }
      if (ofs > max_ofs)
        ofs = max_ofs;

      /* Make offsets relative to base */
      last_ofs += hint + 1;              /* POP: we add 1 here so last_ofs stays non-negative */
      ofs += hint;
    }
  else                          /* key <= a[hint] */
  /* Gallop left until a[hint-ofs] < key <= a[hint-last_ofs] */
    {
      const gsize max_ofs = hint + 1;
      gsize tmp;
      while (ofs < max_ofs
             && gtk_tim_sort_compare (self, key, ELEM (hintp, -ofs)) <= 0)
        {
          last_ofs = ofs;
          ofs = (ofs << 1) + 1;                 /* no need to check for overflow */
        }
      if (ofs > max_ofs)
        ofs = max_ofs;

      /* Make offsets relative to base */
      tmp = last_ofs;
      last_ofs = hint + 1 - ofs;                 /* POP: we add 1 here so last_ofs stays non-negative */
      ofs = hint - tmp;
    }
  g_assert (last_ofs <= ofs && ofs <= len);

  /*
   * Now a[last_ofs-1] < key <= a[ofs], so key belongs somewhere
   * to the right of last_ofs but no farther right than ofs.  Do a binary
   * search, with invariant a[last_ofs - 1] < key <= a[ofs].
   */
  /* last_ofs++; POP: we added 1 above to keep last_ofs non-negative */
  while (last_ofs < ofs)
    {
      /*gsize m = last_ofs + ((ofs - last_ofs) >> 1); */
      /* http://stackoverflow.com/questions/4844165/safe-integer-middle-value-formula */
      gsize m = (last_ofs & ofs) + ((last_ofs ^ ofs) >> 1);

      if (gtk_tim_sort_compare (self, key, ELEM (base, m)) > 0)
        last_ofs = m + 1;                        /* a[m] < key */
      else
        ofs = m;                        /* key <= a[m] */
    }
  g_assert (last_ofs == ofs);      /* so a[ofs - 1] < key <= a[ofs] */
  return ofs;
}

/*
 * Like gallop_left, except that if the range contains an element equal to
 * key, gallop_right returns the index after the rightmost equal element.
 *
 * @param key the key whose insertion point to search for
 * @param base the array in which to search
 * @param len the length of the range; must be > 0
 * @param hint the index at which to begin the search, 0 <= hint < n.
 *     The closer hint is to the result, the faster this method will run.
 * @param c the comparator used to order the range, and to search
 * @return the int k,  0 <= k <= n such that a[b + k - 1] <= key < a[b + k]
 */
static gsize
gtk_tim_sort(gallop_right) (GtkTimSort *self,
                            gpointer    key,
                            gpointer    base,
                            gsize       len,
                            gsize       hint)
{
  char *hintp = ELEM (base, hint);
  gsize ofs = 1;
  gsize last_ofs = 0;

  g_assert (len > 0 && hint < len);

  if (gtk_tim_sort_compare (self, key, hintp) < 0)
    {
      /* Gallop left until a[hint - ofs] <= key < a[hint - last_ofs] */
      gsize max_ofs = hint + 1;
      gsize tmp;
      while (ofs < max_ofs
             && gtk_tim_sort_compare (self, key, ELEM (hintp, -ofs)) < 0)
        {
          last_ofs = ofs;
          ofs = (ofs << 1) + 1;                 /* no need to check for overflow */
        }
      if (ofs > max_ofs)
        ofs = max_ofs;

      /* Make offsets relative to base */
      tmp = last_ofs;
      last_ofs = hint + 1 - ofs;
      ofs = hint - tmp;
    }
  else                          /* a[hint] <= key */
  /* Gallop right until a[hint + last_ofs] <= key < a[hint + ofs] */
    {
      gsize max_ofs = len - hint;
      while (ofs < max_ofs
             && gtk_tim_sort_compare (self, key, ELEM (hintp, ofs)) >= 0)
        {
          last_ofs = ofs;
          ofs = (ofs << 1) + 1;                 /* no need to check for overflow */
        }
      if (ofs > max_ofs)
        ofs = max_ofs;

      /* Make offsets relative to base */
      last_ofs += hint + 1;
      ofs += hint;
    }
  g_assert (last_ofs <= ofs && ofs <= len);

  /*
   * Now a[last_ofs - 1] <= key < a[ofs], so key belongs somewhere to
   * the right of last_ofs but no farther right than ofs.  Do a binary
   * search, with invariant a[last_ofs - 1] <= key < a[ofs].
   */
  while (last_ofs < ofs)
    {
      /* gsize m = last_ofs + ((ofs - last_ofs) >> 1); */
      gsize m = (last_ofs & ofs) + ((last_ofs ^ ofs) >> 1);

      if (gtk_tim_sort_compare (self, key, ELEM (base, m)) < 0)
        ofs = m;                        /* key < a[m] */
      else
        last_ofs = m + 1;                        /* a[m] <= key */
    }
  g_assert (last_ofs == ofs);      /* so a[ofs - 1] <= key < a[ofs] */
  return ofs;
}

/*
 * Merges two adjacent runs in place, in a stable fashion.  The first
 * element of the first run must be greater than the first element of the
 * second run (a[base1] > a[base2]), and the last element of the first run
 * (a[base1 + len1-1]) must be greater than all elements of the second run.
 *
 * For performance, this method should be called only when len1 <= len2;
 * its twin, merge_hi should be called if len1 >= len2.  (Either method
 * may be called if len1 == len2.)
 *
 * @param base1 first element in first run to be merged
 * @param len1  length of first run to be merged (must be > 0)
 * @param base2 first element in second run to be merged
 *        (must be aBase + aLen)
 * @param len2  length of second run to be merged (must be > 0)
 */
static void
gtk_tim_sort(merge_lo) (GtkTimSort *self,
                        gpointer    base1,
                        gsize       len1,
                        gpointer    base2,
                        gsize       len2)
{
  /* Copy first run into temp array */
  gpointer tmp = gtk_tim_sort_ensure_capacity (self, len1);
  char *cursor1;
  char *cursor2;
  char *dest;
  gsize min_gallop;

  g_assert (len1 > 0 && len2 > 0 && ELEM (base1, len1) == base2);

  /* System.arraycopy(a, base1, tmp, 0, len1); */
  memcpy (tmp, base1, LEN (len1));     /* POP: can't overlap */

  cursor1 = tmp;                /* Indexes into tmp array */
  cursor2 = base2;              /* Indexes int a */
  dest = base1;                 /* Indexes int a */

  /* Move first element of second run and deal with degenerate cases */
  /* a[dest++] = a[cursor2++]; */
  ASSIGN (dest, cursor2);
  dest = INCPTR (dest);
  cursor2 = INCPTR (cursor2);

  if (--len2 == 0)
    {
      memcpy (dest, cursor1, LEN (len1));         /* POP: can't overlap */
      return;
    }
  if (len1 == 1)
    {
      memmove (dest, cursor2, LEN (len2));         /* POP: overlaps */

      /* a[dest + len2] = tmp[cursor1]; // Last elt of run 1 to end of merge */
      ASSIGN (ELEM (dest, len2), cursor1);
      return;
    }

  /* Use local variable for performance */
  min_gallop = self->min_gallop;

  while (TRUE)
    {
      gsize count1 = 0;                 /* Number of times in a row that first run won */
      gsize count2 = 0;                 /* Number of times in a row that second run won */

      /*
       * Do the straightforward thing until (if ever) one run starts
       * winning consistently.
       */
      do
        {
          g_assert (len1 > 1 && len2 > 0);
          if (gtk_tim_sort_compare (self, cursor2, cursor1) < 0)
            {
              ASSIGN (dest, cursor2);
              dest = INCPTR (dest);
              cursor2 = INCPTR (cursor2);
              count2++;
              count1 = 0;
              if (--len2 == 0)
                goto outer;
              if (count2 >= min_gallop)
                break;
            }
          else
            {
              ASSIGN (dest, cursor1);
              dest = INCPTR (dest);
              cursor1 = INCPTR (cursor1);
              count1++;
              count2 = 0;
              if (--len1 == 1)
                goto outer;
              if (count1 >= min_gallop)
                break;
            }
        }
      while (TRUE);                /* (count1 | count2) < min_gallop); */

      /*
       * One run is winning so consistently that galloping may be a
       * huge win. So try that, and continue galloping until (if ever)
       * neither run appears to be winning consistently anymore.
       */
      do
        {
          g_assert (len1 > 1 && len2 > 0);
          count1 = gtk_tim_sort(gallop_right) (self, cursor2, cursor1, len1, 0);
          if (count1 != 0)
            {
              memcpy (dest, cursor1, LEN (count1));                 /* POP: can't overlap */
              dest = ELEM (dest, count1);
              cursor1 = ELEM (cursor1, count1);
              len1 -= count1;
              if (len1 <= 1)                    /* len1 == 1 || len1 == 0 */
                goto outer;
            }
          ASSIGN (dest, cursor2);
          dest = INCPTR (dest);
          cursor2 = INCPTR (cursor2);
          if (--len2 == 0)
            goto outer;

          count2 = gtk_tim_sort(gallop_left) (self, cursor1, cursor2, len2, 0);
          if (count2 != 0)
            {
              memmove (dest, cursor2, LEN (count2));                 /* POP: might overlap */
              dest = ELEM (dest, count2);
              cursor2 = ELEM (cursor2, count2);
              len2 -= count2;
              if (len2 == 0)
                goto outer;
            }
          ASSIGN (dest, cursor1);
          dest = INCPTR (dest);
          cursor1 = INCPTR (cursor1);
          if (--len1 == 1)
            goto outer;
          if (min_gallop > 0)
            min_gallop--;
        }
      while (count1 >= MIN_GALLOP || count2 >= MIN_GALLOP);
      min_gallop += 2;           /* Penalize for leaving gallop mode */
    }                           /* End of "outer" loop */
outer:
  self->min_gallop = min_gallop < 1 ? 1 : min_gallop;        /* Write back to field */

  if (len1 == 1)
    {
      g_assert (len2 > 0);
      memmove (dest, cursor2, LEN (len2));              /*  POP: might overlap */
      ASSIGN (ELEM (dest, len2), cursor1);              /*  Last elt of run 1 to end of merge */
    }
  else if (len1 == 0)
    {
      g_critical ("Comparison method violates its general contract");
      return;
    }
  else
    {
      g_assert (len2 == 0);
      g_assert (len1 > 1);
      memcpy (dest, cursor1, LEN (len1));         /* POP: can't overlap */
    }
}

/*
 * Like merge_lo, except that this method should be called only if
 * len1 >= len2; merge_lo should be called if len1 <= len2.  (Either method
 * may be called if len1 == len2.)
 *
 * @param base1 first element in first run to be merged
 * @param len1  length of first run to be merged (must be > 0)
 * @param base2 first element in second run to be merged
 *        (must be aBase + aLen)
 * @param len2  length of second run to be merged (must be > 0)
 */
static void
gtk_tim_sort(merge_hi) (GtkTimSort *self,
                        gpointer    base1,
                        gsize       len1,
                        gpointer    base2,
                        gsize       len2)
{
  /* Copy second run into temp array */
  gpointer tmp = gtk_tim_sort_ensure_capacity (self, len2);
  char *cursor1;        /* Indexes into a */
  char *cursor2;        /* Indexes into tmp array */
  char *dest;           /* Indexes into a */
  gsize min_gallop;

  g_assert (len1 > 0 && len2 > 0 && ELEM (base1, len1) == base2);

  memcpy (tmp, base2, LEN (len2));     /* POP: can't overlap */

  cursor1 = ELEM (base1, len1 - 1);     /* Indexes into a */
  cursor2 = ELEM (tmp, len2 - 1);       /* Indexes into tmp array */
  dest = ELEM (base2, len2 - 1);        /* Indexes into a */

  /* Move last element of first run and deal with degenerate cases */
  /* a[dest--] = a[cursor1--]; */
  ASSIGN (dest, cursor1);
  dest = DECPTR (dest);
  cursor1 = DECPTR (cursor1);
  if (--len1 == 0)
    {
      memcpy (ELEM (dest, -(len2 - 1)), tmp, LEN (len2));        /* POP: can't overlap */
      return;
    }
  if (len2 == 1)
    {
      dest = ELEM (dest, -len1);
      cursor1 = ELEM (cursor1, -len1);
      memmove (ELEM (dest, 1), ELEM (cursor1, 1), LEN (len1));       /* POP: overlaps */
      /* a[dest] = tmp[cursor2]; */
      ASSIGN (dest, cursor2);
      return;
    }

  /* Use local variable for performance */
  min_gallop = self->min_gallop;

  while (TRUE)
    {
      gsize count1 = 0;                 /* Number of times in a row that first run won */
      gsize count2 = 0;                 /* Number of times in a row that second run won */

      /*
       * Do the straightforward thing until (if ever) one run
       * appears to win consistently.
       */
      do
        {
          g_assert (len1 > 0 && len2 > 1);
          if (gtk_tim_sort_compare (self, cursor2, cursor1) < 0)
            {
              ASSIGN (dest, cursor1);
              dest = DECPTR (dest);
              cursor1 = DECPTR (cursor1);
              count1++;
              count2 = 0;
              if (--len1 == 0)
                goto outer;
            }
          else
            {
              ASSIGN (dest, cursor2);
              dest = DECPTR (dest);
              cursor2 = DECPTR (cursor2);
              count2++;
              count1 = 0;
              if (--len2 == 1)
                goto outer;
            }
        }
      while ((count1 | count2) < min_gallop);

      /*
       * One run is winning so consistently that galloping may be a
       * huge win. So try that, and continue galloping until (if ever)
       * neither run appears to be winning consistently anymore.
       */
      do
        {
          g_assert (len1 > 0 && len2 > 1);
          count1 = len1 - gtk_tim_sort(gallop_right) (self, cursor2, base1, len1, len1 - 1);
          if (count1 != 0)
            {
              dest = ELEM (dest, -count1);
              cursor1 = ELEM (cursor1, -count1);
              len1 -= count1;
              memmove (INCPTR (dest), INCPTR (cursor1),
                       LEN (count1));                /* POP: might overlap */
              if (len1 == 0)
                goto outer;
            }
          ASSIGN (dest, cursor2);
          dest = DECPTR (dest);
          cursor2 = DECPTR (cursor2);
          if (--len2 == 1)
            goto outer;

          count2 = len2 - gtk_tim_sort(gallop_left) (self, cursor1, tmp, len2, len2 - 1);
          if (count2 != 0)
            {
              dest = ELEM (dest, -count2);
              cursor2 = ELEM (cursor2, -count2);
              len2 -= count2;
              memcpy (INCPTR (dest), INCPTR (cursor2), LEN (count2));               /* POP: can't overlap */
              if (len2 <= 1)                    /* len2 == 1 || len2 == 0 */
                goto outer;
            }
          ASSIGN (dest, cursor1);
          dest = DECPTR (dest);
          cursor1 = DECPTR (cursor1);
          if (--len1 == 0)
            goto outer;
          if (min_gallop > 0)
            min_gallop--;
        }
      while (count1 >= MIN_GALLOP || count2 >= MIN_GALLOP);
      min_gallop += 2;           /* Penalize for leaving gallop mode */
    }                           /* End of "outer" loop */
outer:
  self->min_gallop = min_gallop < 1 ? 1 : min_gallop;        /* Write back to field */

  if (len2 == 1)
    {
      g_assert (len1 > 0);
      dest = ELEM (dest, -len1);
      cursor1 = ELEM (cursor1, -len1);
      memmove (INCPTR (dest), INCPTR (cursor1), LEN (len1));       /* POP: might overlap */
      /* a[dest] = tmp[cursor2];  // Move first elt of run2 to front of merge */
      ASSIGN (dest, cursor2);
    }
  else if (len2 == 0)
    {
      g_critical ("Comparison method violates its general contract");
      return;
    }
  else
    {
      g_assert (len1 == 0);
      g_assert (len2 > 0);
      memcpy (ELEM (dest, -(len2 - 1)), tmp, LEN (len2));        /* POP: can't overlap */
    }
}

/*
 * Merges the two runs at stack indices i and i+1.  Run i must be
 * the penultimate or antepenultimate run on the stack.  In other words,
 * i must be equal to pending_runs-2 or pending_runs-3.
 *
 * @param i stack index of the first of the two runs to merge
 */
static void
gtk_tim_sort(merge_at) (GtkTimSort    *self,
                        gsize          i,
                        GtkTimSortRun *out_change)
{
  gpointer base1 = self->run[i].base;
  gsize len1 = self->run[i].len;
  gpointer base2 = self->run[i + 1].base;
  gsize len2 = self->run[i + 1].len;
  gsize k;

  g_assert (self->pending_runs >= 2);
  g_assert (i == self->pending_runs - 2 || i == self->pending_runs - 3);
  g_assert (len1 > 0 && len2 > 0);
  g_assert (ELEM (base1, len1) == base2);

  /*
   * Find where the first element of run2 goes in run1. Prior elements
   * in run1 can be ignored (because they're already in place).
   */
  k = gtk_tim_sort(gallop_right) (self, base2, base1, len1, 0);
  base1 = ELEM (base1, k);
  len1 -= k;
  if (len1 == 0)
    {
      gtk_tim_sort_set_change (out_change, NULL, 0);
      goto done;
    }

  /*
   * Find where the last element of run1 goes in run2. Subsequent elements
   * in run2 can be ignored (because they're already in place).
   */
  len2 = gtk_tim_sort(gallop_left) (self,
                                    ELEM (base1, len1 - 1),
                                    base2, len2, len2 - 1);
  if (len2 == 0)
    {
      gtk_tim_sort_set_change (out_change, NULL, 0);
      goto done;
    }

  /* Merge remaining runs, using tmp array with min(len1, len2) elements */
  if (len1 <= len2)
    {
      if (len1 > self->max_merge_size)
        {
          base1 = ELEM (self->run[i].base, self->run[i].len - self->max_merge_size);
          gtk_tim_sort(merge_lo) (self, base1, self->max_merge_size, base2, len2);
          gtk_tim_sort_set_change (out_change, base1, self->max_merge_size + len2);
          self->run[i].len -= self->max_merge_size;
          self->run[i + 1].base = ELEM (self->run[i + 1].base, - self->max_merge_size);
          self->run[i + 1].len += self->max_merge_size;
          g_assert (ELEM (self->run[i].base, self->run[i].len) == self->run[i + 1].base);
          return;
        }
      else
        {
          gtk_tim_sort(merge_lo) (self, base1, len1, base2, len2);
          gtk_tim_sort_set_change (out_change, base1, len1 + len2);
        }
    }
  else
    {
      if (len2 > self->max_merge_size)
        {
          gtk_tim_sort(merge_hi) (self, base1, len1, base2, self->max_merge_size);
          gtk_tim_sort_set_change (out_change, base1, len1 + self->max_merge_size);
          self->run[i].len += self->max_merge_size;
          self->run[i + 1].base = ELEM (self->run[i + 1].base, self->max_merge_size);
          self->run[i + 1].len -= self->max_merge_size;
          g_assert (ELEM (self->run[i].base, self->run[i].len) == self->run[i + 1].base);
          return;
        }
      else
        {
          gtk_tim_sort(merge_hi) (self, base1, len1, base2, len2);
          gtk_tim_sort_set_change (out_change, base1, len1 + len2);
        }
    }

done:
  /*
   * Record the length of the combined runs; if i is the 3rd-last
   * run now, also slide over the last run (which isn't involved
   * in this merge).  The current run (i+1) goes away in any case.
   */
  self->run[i].len += self->run[i + 1].len;
  if (i == self->pending_runs - 3)
    self->run[i + 1] = self->run[i + 2];
  self->pending_runs--;
}


/*
 * Examines the stack of runs waiting to be merged and merges adjacent runs
 * until the stack invariants are reestablished:
 *
 *     1. run_len[i - 3] > run_len[i - 2] + run_len[i - 1]
 *     2. run_len[i - 2] > run_len[i - 1]
 *
 * This method is called each time a new run is pushed onto the stack,
 * so the invariants are guaranteed to hold for i < pending_runs upon
 * entry to the method.
 *
 * POP:
 * Modified according to http://envisage-project.eu/wp-content/uploads/2015/02/sorting.pdf
 *
 * and
 *
 * https://bugs.openjdk.java.net/browse/JDK-8072909 (suggestion 2)
 *
 */
static gboolean
gtk_tim_sort(merge_collapse) (GtkTimSort *self,
                              GtkTimSortRun *out_change)
{
  GtkTimSortRun *run = self->run;
  gsize n;

  if (self->pending_runs <= 1)
    return FALSE;

  n = self->pending_runs - 2;
  if ((n > 0 && run[n - 1].len <= run[n].len + run[n + 1].len) ||
      (n > 1 && run[n - 2].len <= run[n].len + run[n - 1].len))
    {
      if (run[n - 1].len < run[n + 1].len)
        n--;
    }
  else if (run[n].len > run[n + 1].len)
    {
      return FALSE; /* Invariant is established */
    }
  
  gtk_tim_sort(merge_at) (self, n, out_change);
  return TRUE;
}

/*
 * Merges all runs on the stack until only one remains.  This method is
 * called once, to complete the sort.
 */
static gboolean
gtk_tim_sort(merge_force_collapse) (GtkTimSort    *self,
                                    GtkTimSortRun *out_change)
{
  gsize n;

  if (self->pending_runs <= 1)
    return FALSE;

  n = self->pending_runs - 2;
  if (n > 0 && self->run[n - 1].len < self->run[n + 1].len)
    n--;
  gtk_tim_sort(merge_at) (self, n, out_change);
  return TRUE;
}

static gboolean
gtk_tim_sort(step) (GtkTimSort    *self,
                    GtkTimSortRun *out_change)
{
  g_assert (self);

  if (gtk_tim_sort(merge_collapse) (self, out_change))
    return TRUE;

  if (gtk_tim_sort(merge_append) (self, out_change))
    return TRUE;

  if (gtk_tim_sort(merge_force_collapse) (self, out_change))
    return TRUE;

  return FALSE;
}

#undef DEFINE_TEMP
#undef ASSIGN
#undef INCPTR
#undef DECPTR
#undef ELEM
#undef LEN

#undef CONCAT
#undef MAKE_STR
#undef gtk_tim_sort

#undef WIDTH
#undef NAME
