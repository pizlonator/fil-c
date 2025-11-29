#include <string.h>
#include <float.h>
#include <math.h>
#include <glib.h>
#include <assert.h>

/* See Golub and Reinsch,
 * "Handbook for Automatic Computation vol II - Linear Algebra",
 * Springer, 1971
 */


#define MAX_ITERATION_COUNT 30

/* Perform Householder reduction to bidiagonal form
 *
 * Input: Matrix A of size nrows x ncols
 *
 * Output: Matrices and vectors such that
 * A = U*Bidiag(diagonal, superdiagonal)*Vt
 *
 * All matrices are allocated by the caller
 *
 * Sizes:
 *  A, U: nrows x ncols
 *  diagonal, superdiagonal: ncols
 *  V: ncols x ncols
 */
static void
householder_reduction (double *A,
                       int     nrows,
                       int     ncols,
                       double *U,
                       double *V,
                       double *diagonal,
                       double *superdiagonal)
{
  int i, j, k, ip1;
  double s, s2, si, scale;
  double *pu, *pui, *pv, *pvi;
  double half_norm_squared;

  assert (nrows >= 2);
  assert (ncols >= 2);

  memcpy (U, A, sizeof (double) * nrows * ncols);

  diagonal[0] = 0.0;
  s = 0.0;
  scale = 0.0;
  for (i = 0, pui = U, ip1 = 1;
       i < ncols;
       pui += ncols, i++, ip1++)
    {
      superdiagonal[i] = scale * s;

      for (j = i, pu = pui, scale = 0.0;
           j < nrows;
           j++, pu += ncols)
        scale += fabs( *(pu + i) );

      if (scale > 0.0)
        {
          for (j = i, pu = pui, s2 = 0.0; j < nrows; j++, pu += ncols)
            {
              *(pu + i) /= scale;
              s2 += *(pu + i) * *(pu + i);
            }
          s = *(pui + i) < 0.0 ? sqrt (s2) : -sqrt (s2);
          half_norm_squared = *(pui + i) * s - s2;
          *(pui + i) -= s;

          for (j = ip1; j < ncols; j++)
            {
              for (k = i, si = 0.0, pu = pui; k < nrows; k++, pu += ncols)
                si += *(pu + i) * *(pu + j);
              si /= half_norm_squared;
              for (k = i, pu = pui; k < nrows; k++, pu += ncols)
                *(pu + j) += si * *(pu + i);
            }
        }
      for (j = i, pu = pui; j < nrows; j++, pu += ncols)
        *(pu + i) *= scale;
      diagonal[i] = s * scale;
      s = 0.0;
      scale = 0.0;
      if (i >= nrows || i == ncols - 1)
        continue;
      for (j = ip1; j < ncols; j++)
        scale += fabs (*(pui + j));
      if (scale > 0.0)
        {
          for (j = ip1, s2 = 0.0; j < ncols; j++)
            {
              *(pui + j) /= scale;
              s2 += *(pui + j) * *(pui + j);
            }
          s = *(pui + ip1) < 0.0 ? sqrt (s2) : -sqrt (s2);
          half_norm_squared = *(pui + ip1) * s - s2;
          *(pui + ip1) -= s;
          for (k = ip1; k < ncols; k++)
            superdiagonal[k] = *(pui + k) / half_norm_squared;
          if (i < (nrows - 1))
            {
              for (j = ip1, pu = pui + ncols; j < nrows; j++, pu += ncols)
                {
                  for (k = ip1, si = 0.0; k < ncols; k++)
                    si += *(pui + k) * *(pu + k);
                  for (k = ip1; k < ncols; k++)
                    *(pu + k) += si * superdiagonal[k];
                }
            }
          for (k = ip1; k < ncols; k++)
            *(pui + k) *= scale;
        }
    }

  pui = U + ncols * (ncols - 2);
  pvi = V + ncols * (ncols - 1);
  *(pvi + ncols - 1) = 1.0;
  s = superdiagonal[ncols - 1];
  pvi -= ncols;
  for (i = ncols - 2, ip1 = ncols - 1;
       i >= 0;
       i--, pui -= ncols, pvi -= ncols, ip1--)
    {
      if (s != 0.0)
        {
          pv = pvi + ncols;
          for (j = ip1; j < ncols; j++, pv += ncols)
            *(pv + i) = ( *(pui + j) / *(pui + ip1) ) / s;
          for (j = ip1; j < ncols; j++)
            {
              si = 0.0;
              for (k = ip1, pv = pvi + ncols; k < ncols; k++, pv += ncols)
                si += *(pui + k) * *(pv + j);
              for (k = ip1, pv = pvi + ncols; k < ncols; k++, pv += ncols)
                *(pv + j) += si * *(pv + i);
            }
        }
      pv = pvi + ncols;
      for (j = ip1; j < ncols; j++, pv += ncols)
        {
          *(pvi + j) = 0.0;
          *(pv + i) = 0.0;
        }
      *(pvi + i) = 1.0;
      s = superdiagonal[i];
    }

  pui = U + ncols * (ncols - 1);
  for (i = ncols - 1, ip1 = ncols;
       i >= 0;
       ip1 = i, i--, pui -= ncols)
    {
      s = diagonal[i];
      for (j = ip1; j < ncols; j++)
        *(pui + j) = 0.0;
      if (s != 0.0)
        {
          for (j = ip1; j < ncols; j++)
            {
              si = 0.0;
              pu = pui + ncols;
              for (k = ip1; k < nrows; k++, pu += ncols)
                si += *(pu + i) * *(pu + j);
              si = (si / *(pui + i)) / s;
              for (k = i, pu = pui; k < nrows; k++, pu += ncols)
                *(pu + j) += si * *(pu + i);
            }
          for (j = i, pu = pui; j < nrows; j++, pu += ncols)
            *(pu + i) /= s;
        }
      else
        for (j = i, pu = pui; j < nrows; j++, pu += ncols)
          *(pu + i) = 0.0;
      *(pui + i) += 1.0;
    }
}

/* Perform Givens reduction
 *
 * Input: Matrices such that
 * A = U*Bidiag(diagonal,superdiagonal)*Vt
 *
 * Output: The same, with superdiagonal = 0
 *
 * All matrices are allocated by the caller
 *
 * Sizes:
 *  U: nrows x ncols
 *  diagonal, superdiagonal: ncols
 *  V: ncols x ncols
 */
static int
givens_reduction (int nrows,
                  int ncols,
                  double *U,
                  double *V,
                  double *diagonal,
                  double *superdiagonal)
{
  double epsilon;
  double c, s;
  double f,g,h;
  double x,y,z;
  double *pu, *pv;
  int i,j,k,m;
  int rotation_test;
  int iteration_count;

  assert (nrows >= 2);
  assert (ncols >= 2);

  for (i = 0, x = 0.0; i < ncols; i++)
    {
      y = fabs (diagonal[i]) + fabs (superdiagonal[i]);
      if (x < y)
        x = y;
    }
  epsilon = x * DBL_EPSILON;
  for (k = ncols - 1; k >= 0; k--)
    {
      iteration_count = 0;
      while (1)
        {
          rotation_test = 1;
          for (m = k; m >= 0; m--)
            {
              if (fabs (superdiagonal[m]) <= epsilon)
                {
                  rotation_test = 0;
                  break;
                }
              if (fabs (diagonal[m-1]) <= epsilon)
                break;
            }
          if (rotation_test)
            {
              c = 0.0;
              s = 1.0;
              for (i = m; i <= k; i++)
                {
                  f = s * superdiagonal[i];
                  superdiagonal[i] *= c;
                  if (fabs (f) <= epsilon)
                    break;
                  g = diagonal[i];
                  h = sqrt (f*f + g*g);
                  diagonal[i] = h;
                  c = g / h;
                  s = -f / h;
                  for (j = 0, pu = U; j < nrows; j++, pu += ncols)
                    {
                      y = *(pu + m - 1);
                      z = *(pu + i);
                      *(pu + m - 1 ) = y * c + z * s;
                      *(pu + i) = -y * s + z * c;
                    }
                }
            }
          z = diagonal[k];
          if (m == k)
            {
              if (z < 0.0)
                {
                  diagonal[k] = -z;
                  for (j = 0, pv = V; j < ncols; j++, pv += ncols)
                    *(pv + k) = - *(pv + k);
                }
              break;
            }
          else
            {
              if (iteration_count >= MAX_ITERATION_COUNT)
                return -1;
              iteration_count++;
              x = diagonal[m];
              y = diagonal[k-1];
              g = superdiagonal[k-1];
              h = superdiagonal[k];
              f = ((y - z) * ( y + z ) + (g - h) * (g + h))/(2.0 * h * y);
              g = sqrt (f * f + 1.0);
              if (f < 0.0)
                g = -g;
              f = ((x - z) * (x + z) + h * (y / (f + g) - h)) / x;
              c = 1.0;
              s = 1.0;
              for (i = m + 1; i <= k; i++)
                {
                  g = superdiagonal[i];
                  y = diagonal[i];
                  h = s * g;
                  g *= c;
                  z = sqrt (f * f + h * h);
                  superdiagonal[i-1] = z;
                  c = f / z;
                  s = h / z;
                  f =  x * c + g * s;
                  g = -x * s + g * c;
                  h = y * s;
                  y *= c;
                  for (j = 0, pv = V; j < ncols; j++, pv += ncols)
                    {
                      x = *(pv + i - 1);
                      z = *(pv + i);
                      *(pv + i - 1) = x * c + z * s;
                      *(pv + i) = -x * s + z * c;
                    }
                  z = sqrt (f * f + h * h);
                  diagonal[i - 1] = z;
                  if (z != 0.0)
                    {
                      c = f / z;
                      s = h / z;
                    }
                  f = c * g + s * y;
                  x = -s * g + c * y;
                  for (j = 0, pu = U; j < nrows; j++, pu += ncols)
                    {
                      y = *(pu + i - 1);
                      z = *(pu + i);
                      *(pu + i - 1) = c * y + s * z;
                      *(pu + i) = -s * y + c * z;
                    }
                }
              superdiagonal[m] = 0.0;
              superdiagonal[k] = f;
              diagonal[k] = x;
            }
        }
    }
  return 0;
}

/* Given a singular value decomposition
 * of an nrows x ncols matrix A = U*Diag(S)*Vt,
 * sort the values of S by decreasing value,
 * permuting V to match.
 */
static void
sort_singular_values (int     nrows,
                      int     ncols,
                      double *S,
                      double *U,
                      double *V)
{
  int i, j, max_index;
  double temp;
  double *p1, *p2;

  assert (nrows >= 2);
  assert (ncols >= 2);

  for (i = 0; i < ncols - 1; i++)
    {
      max_index = i;
      for (j = i + 1; j < ncols; j++)
        if (S[j] > S[max_index])
          max_index = j;
      if (max_index == i)
        continue;
      temp = S[i];
      S[i] = S[max_index];
      S[max_index] = temp;
      p1 = U + max_index;
      p2 = U + i;
      for (j = 0; j < nrows; j++, p1 += ncols, p2 += ncols)
        {
          temp = *p1;
          *p1 = *p2;
          *p2 = temp;
        }
      p1 = V + max_index;
      p2 = V + i;
      for (j = 0; j < ncols; j++, p1 += ncols, p2 += ncols)
        {
          temp = *p1;
          *p1 = *p2;
          *p2 = temp;
        }
    }
}

/* Compute a singular value decomposition of A,
 * A = U*Diag(S)*Vt
 *
 * All matrices are allocated by the caller
 *
 * Sizes:
 *  A, U: nrows x ncols
 *  S: ncols
 *  V: ncols x ncols
 */
int
singular_value_decomposition (double *A,
                              int     nrows,
                              int     ncols,
                              double *U,
                              double *S,
                              double *V)
{
  double *superdiagonal;

  superdiagonal = g_alloca (sizeof (double) * ncols);

  if (nrows < ncols)
    return -1;

  householder_reduction (A, nrows, ncols, U, V, S, superdiagonal);

  if (givens_reduction (nrows, ncols, U, V, S, superdiagonal) < 0)
    return -1;

  sort_singular_values (nrows, ncols, S, U, V);

  return 0;
}

/*
 * Given a singular value decomposition of A = U*Diag(S)*Vt,
 * compute the best approximation x to A*x = B.
 *
 * All matrices are allocated by the caller
 *
 * Sizes:
 *  U: nrows x ncols
 *  S: ncols
 *  V: ncols x ncols
 *  B, x: ncols
 */
void
singular_value_decomposition_solve (double *U,
                                    double *S,
                                    double *V,
                                    int     nrows,
                                    int     ncols,
                                    double *B,
                                    double *x)
{
  int i, j, k;
  double *pu, *pv;
  double d;
  double tolerance;

  assert (nrows >= 2);
  assert (ncols >= 2);

  tolerance = DBL_EPSILON * S[0] * (double) ncols;

  for (i = 0, pv = V; i < ncols; i++, pv += ncols)
    {
      x[i] = 0.0;
      for (j = 0; j < ncols; j++)
        {
          if (S[j] > tolerance)
            {
              for (k = 0, d = 0.0, pu = U; k < nrows; k++, pu += ncols)
                d += *(pu + j) * B[k];
              x[i] += d * *(pv + j) / S[j];
            }
        }
    }
}
