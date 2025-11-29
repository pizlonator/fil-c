/*
 * Copyright © 2003 Davide Libenzi
 *             2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Davide Libenzi <davidel@xmailserver.org>
 *          Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gskdiffprivate.h"


#define XDL_MAX_COST_MIN 256
#define XDL_HEUR_MIN_COST 256
#define XDL_LINE_MAX G_MAXSSIZE
#define XDL_SNAKE_CNT 20
#define XDL_K_HEUR 4
#define MAXCOST 20

struct _GskDiffSettings {
  GCompareDataFunc        compare_func;
  GskKeepFunc             keep_func;
  GskDeleteFunc           delete_func;
  GskInsertFunc           insert_func;

  guint allow_abort : 1;
};

typedef struct _SplitResult {
  long i1, i2;
  int min_lo, min_hi;
} SplitResult;

GskDiffSettings *
gsk_diff_settings_new (GCompareDataFunc compare_func,
                       GskKeepFunc      keep_func,
                       GskDeleteFunc    delete_func,
                       GskInsertFunc    insert_func)
{
  GskDiffSettings *settings;

  settings = g_new0 (GskDiffSettings, 1);

  settings->compare_func = compare_func;
  settings->keep_func = keep_func;
  settings->delete_func = delete_func;
  settings->insert_func = insert_func;

  return settings;
}

void
gsk_diff_settings_set_allow_abort (GskDiffSettings *settings,
                                   gboolean         allow_abort)
{
  settings->allow_abort = allow_abort;
}

void
gsk_diff_settings_free (GskDiffSettings *settings)
{
  g_free (settings);
}

/*
 * See "An O(ND) Difference Algorithm and its Variations", by Eugene Myers.
 * Basically considers a "box" (off1, off2, lim1, lim2) and scan from both
 * the forward diagonal starting from (off1, off2) and the backward diagonal
 * starting from (lim1, lim2). If the K values on the same diagonal crosses
 * returns the furthest point of reach. We might end up having to expensive
 * cases using this algorithm is full, so a little bit of heuristic is needed
 * to cut the search and to return a suboptimal point.
 */
static GskDiffResult
split (gconstpointer         *elem1,
       gssize                 off1,
       gssize                 lim1,
       gconstpointer         *elem2,
       gssize                 off2,
       gssize                 lim2,
       gssize                *kvdf,
       gssize                *kvdb,
       gboolean               need_min,
       const GskDiffSettings *settings,
       gpointer               data,
       SplitResult           *spl)
{
  gssize dmin = off1 - lim2, dmax = lim1 - off2;
  gssize fmid = off1 - off2, bmid = lim1 - lim2;
  gboolean odd = (fmid - bmid) & 1;
  gssize fmin = fmid, fmax = fmid;
  gssize bmin = bmid, bmax = bmid;
  gssize ec, d, i1, i2, prev1, best, dd, v, k;

  /*
   * Set initial diagonal values for both forward and backward path.
   */
  kvdf[fmid] = off1;
  kvdb[bmid] = lim1;

  for (ec = 1;; ec++)
    {
      gboolean got_snake = FALSE;

      /*
       * We need to extent the diagonal "domain" by one. If the next
       * values exits the box boundaries we need to change it in the
       * opposite direction because (max - min) must be a power of two.
       * Also we initialize the external K value to -1 so that we can
       * avoid extra conditions check inside the core loop.
       */
      if (fmin > dmin)
        kvdf[--fmin - 1] = -1;
      else
        ++fmin;
      if (fmax < dmax)
        kvdf[++fmax + 1] = -1;
      else
        --fmax;

      for (d = fmax; d >= fmin; d -= 2)
        {
          if (kvdf[d - 1] >= kvdf[d + 1])
            i1 = kvdf[d - 1] + 1;
          else
            i1 = kvdf[d + 1];
          prev1 = i1;
          i2 = i1 - d;
          for (; i1 < lim1 && i2 < lim2; i1++, i2++)
            {
              if (settings->compare_func (elem1[i1], elem2[i2], data) != 0)
                break;
            }
          if (i1 - prev1 > XDL_SNAKE_CNT)
            got_snake = TRUE;
          kvdf[d] = i1;
          if (odd && bmin <= d && d <= bmax && kvdb[d] <= i1)
            {
              spl->i1 = i1;
              spl->i2 = i2;
              spl->min_lo = spl->min_hi = 1;
              return GSK_DIFF_OK;
            }
        }

      /*
       * We need to extent the diagonal "domain" by one. If the next
       * values exits the box boundaries we need to change it in the
       * opposite direction because (max - min) must be a power of two.
       * Also we initialize the external K value to -1 so that we can
       * avoid extra conditions check inside the core loop.
       */
      if (bmin > dmin)
        kvdb[--bmin - 1] = XDL_LINE_MAX;
      else
        ++bmin;
      if (bmax < dmax)
        kvdb[++bmax + 1] = XDL_LINE_MAX;
      else
        --bmax;

      for (d = bmax; d >= bmin; d -= 2)
        {
          if (kvdb[d - 1] < kvdb[d + 1])
            i1 = kvdb[d - 1];
          else
            i1 = kvdb[d + 1] - 1;
          prev1 = i1;
          i2 = i1 - d;
          for (; i1 > off1 && i2 > off2; i1--, i2--)
            {
              if (settings->compare_func (elem1[i1 - 1], elem2[i2 - 1], data) != 0)
                break;
            }
          if (prev1 - i1 > XDL_SNAKE_CNT)
            got_snake = TRUE;
          kvdb[d] = i1;
          if (!odd && fmin <= d && d <= fmax && i1 <= kvdf[d])
            {
              spl->i1 = i1;
              spl->i2 = i2;
              spl->min_lo = spl->min_hi = 1;
              return GSK_DIFF_OK;
            }
        }

      if (need_min)
        continue;

      /*
       * If the edit cost is above the heuristic trigger and if
       * we got a good snake, we sample current diagonals to see
       * if some of them have reached an "interesting" path. Our
       * measure is a function of the distance from the diagonal
       * corner (i1 + i2) penalized with the distance from the
       * mid diagonal itself. If this value is above the current
       * edit cost times a magic factor (XDL_K_HEUR) we consider
       * it interesting.
       */
      if (got_snake && ec > XDL_HEUR_MIN_COST)
        {
          for (best = 0, d = fmax; d >= fmin; d -= 2)
            {
              dd = d > fmid ? d - fmid: fmid - d;
              i1 = kvdf[d];
              i2 = i1 - d;
              v = (i1 - off1) + (i2 - off2) - dd;

              if (v > XDL_K_HEUR * ec && v > best &&
                  off1 + XDL_SNAKE_CNT <= i1 && i1 < lim1 &&
                  off2 + XDL_SNAKE_CNT <= i2 && i2 < lim2)
                {
                  for (k = 1; ; k++)
                    {
                      if (settings->compare_func (elem1[i1 - k], elem2[i2 - k], data) != 0)
                        break;
                      if (k == XDL_SNAKE_CNT)
                        {
                          best = v;
                          spl->i1 = i1;
                          spl->i2 = i2;
                          break;
                        }
                    }
                }
            }
          if (best > 0)
            {
              spl->min_lo = 1;
              spl->min_hi = 0;
              return GSK_DIFF_OK;
            }

          for (best = 0, d = bmax; d >= bmin; d -= 2)
            {
              dd = d > bmid ? d - bmid: bmid - d;
              i1 = kvdb[d];
              i2 = i1 - d;
              v = (lim1 - i1) + (lim2 - i2) - dd;

              if (v > XDL_K_HEUR * ec && v > best &&
                  off1 < i1 && i1 <= lim1 - XDL_SNAKE_CNT &&
                  off2 < i2 && i2 <= lim2 - XDL_SNAKE_CNT)
                {
                  for (k = 0; ; k++)
                    {
                      if (settings->compare_func (elem1[i1 + k], elem2[i2 + k], data) != 0)
                        break;

                      if (k == XDL_SNAKE_CNT - 1)
                        {
                          best = v;
                          spl->i1 = i1;
                          spl->i2 = i2;
                          break;
                        }
                    }
                }
            }
          if (best > 0)
            {
              spl->min_lo = 0;
              spl->min_hi = 1;
              return GSK_DIFF_OK;
            }
        }

      /*
       * Enough is enough. We spent too much time here and now we collect
       * the furthest reaching path using the (i1 + i2) measure.
       */
      if (ec >= MAXCOST)
        {
          gssize fbest, fbest1, bbest, bbest1;

          if (settings->allow_abort)
            return GSK_DIFF_ABORTED;

          fbest = fbest1 = -1;
          for (d = fmax; d >= fmin; d -= 2)
            {
              i1 = MIN (kvdf[d], lim1);
              i2 = i1 - d;
              if (lim2 < i2)
                i1 = lim2 + d, i2 = lim2;
              if (fbest < i1 + i2)
                {
                  fbest = i1 + i2;
                  fbest1 = i1;
                }
            }

          bbest = bbest1 = XDL_LINE_MAX;
          for (d = bmax; d >= bmin; d -= 2)
            {
              i1 = MAX (off1, kvdb[d]);
              i2 = i1 - d;
              if (i2 < off2)
                i1 = off2 + d, i2 = off2;
              if (i1 + i2 < bbest)
                {
                  bbest = i1 + i2;
                  bbest1 = i1;
                }
            }

          if ((lim1 + lim2) - bbest < fbest - (off1 + off2))
            {
              spl->i1 = fbest1;
              spl->i2 = fbest - fbest1;
              spl->min_lo = 1;
              spl->min_hi = 0;
            }
          else
            {
              spl->i1 = bbest1;
              spl->i2 = bbest - bbest1;
              spl->min_lo = 0;
              spl->min_hi = 1;
            }

          return GSK_DIFF_OK;
        }
    }
}

/*
 * Rule: "Divide et Impera". Recursively split the box in sub-boxes by calling
 * the box splitting function. Note that the real job (marking changed lines)
 * is done in the two boundary reaching checks.
 */
static GskDiffResult
compare (gconstpointer             *elem1,
         gssize                     off1,
         gssize                     lim1,
         gconstpointer             *elem2,
         gssize                     off2,
         gssize                     lim2,
         gssize                    *kvdf,
         gssize                    *kvdb,
         gboolean                   need_min,
         const GskDiffSettings     *settings,
         gpointer                   data)
{
  GskDiffResult res;

  /*
   * Shrink the box by walking through each diagonal snake (SW and NE).
   */
  for (; off1 < lim1 && off2 < lim2; off1++, off2++)
    {
      if (settings->compare_func (elem1[off1], elem2[off2], data) != 0)
        break;

      res = settings->keep_func (elem1[off1], elem2[off2], data);
      if (res != GSK_DIFF_OK)
        return res;
    }

  for (; off1 < lim1 && off2 < lim2; lim1--, lim2--)
    {
      if (settings->compare_func (elem1[lim1 - 1], elem2[lim2 - 1], data) != 0)
        break;

      res = settings->keep_func (elem1[lim1 - 1], elem2[lim2 - 1], data);
      if (res != GSK_DIFF_OK)
        return res;
    }

  /*
   * If one dimension is empty, then all records on the other one must
   * be obviously changed.
   */
  if (off1 == lim1)
    {
      for (; off2 < lim2; off2++)
        {
          res = settings->insert_func (elem2[off2], off2, data);
          if (res != GSK_DIFF_OK)
            return res;
        }
    }
  else if (off2 == lim2)
    {
      for (; off1 < lim1; off1++)
        {
          res = settings->delete_func (elem1[off1], off1, data);
          if (res != GSK_DIFF_OK)
            return res;
        }
    }
  else
    {
      SplitResult spl = { 0, };

      /*
       * Divide ...
       */
      res = split (elem1, off1, lim1,
                   elem2, off2, lim2,
                   kvdf, kvdb, need_min,
                   settings, data,
                   &spl);
      if (res != GSK_DIFF_OK)
        return res;

      /*
       * ... et Impera.
       */
      res = compare (elem1, off1, spl.i1,
                     elem2, off2, spl.i2,
                     kvdf, kvdb, spl.min_lo,
                     settings, data);
      if (res != GSK_DIFF_OK)
        return res;
      res = compare (elem1, spl.i1, lim1,
                     elem2, spl.i2, lim2,
                     kvdf, kvdb, spl.min_hi,
                     settings, data);
      if (res != GSK_DIFF_OK)
        return res;
    }

  return GSK_DIFF_OK;
}

#if 0
  ndiags = xe->xdf1.nreff + xe->xdf2.nreff + 3;
  if (!(kvd = (long *) xdl_malloc((2 * ndiags + 2) * sizeof(long)))) {

    xdl_free_env(xe);
    return -1;
  }
  kvdf = kvd;
  kvdb = kvdf + ndiags;
  kvdf += xe->xdf2.nreff + 1;
  kvdb += xe->xdf2.nreff + 1;

  xenv.mxcost = xdl_bogosqrt(ndiags);
  if (xenv.mxcost < XDL_MAX_COST_MIN)
    xenv.mxcost = XDL_MAX_COST_MIN;
  xenv.snake_cnt = XDL_SNAKE_CNT;
  xenv.heur_min = XDL_HEUR_MIN_COST;

  dd1.nrec = xe->xdf1.nreff;
  dd1.ha = xe->xdf1.ha;
  dd1.rchg = xe->xdf1.rchg;
  dd1.rindex = xe->xdf1.rindex;
  dd2.nrec = xe->xdf2.nreff;
  dd2.ha = xe->xdf2.ha;
  dd2.rchg = xe->xdf2.rchg;
  dd2.rindex = xe->xdf2.rindex;
#endif

GskDiffResult
gsk_diff (gconstpointer             *elem1,
          gsize                      n1,
          gconstpointer             *elem2,
          gsize                      n2,
          const GskDiffSettings     *settings,
          gpointer                   data)
{
  gsize ndiags;
  gssize *kvd, *kvdf, *kvdb;
  GskDiffResult res;

  ndiags = n1 + n2 + 3;

  kvd = g_new (gssize, 2 * ndiags + 2);
  kvdf = kvd;
  kvdb = kvd + ndiags;
  kvdf += n2 + 1;
  kvdb += n2 + 1;

  res = compare (elem1, 0, n1,
                 elem2, 0, n2,
                 kvdf, kvdb, FALSE,
                 settings, data);

  g_free (kvd);

  return res;
}

