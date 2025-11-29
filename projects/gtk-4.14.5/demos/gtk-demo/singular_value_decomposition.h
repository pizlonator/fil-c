#pragma once

int singular_value_decomposition (double *A,
                                  int     nrows,
                                  int     ncols,
                                  double *U,
                                  double *S,
                                  double *V);

void singular_value_decomposition_solve (double *U,
                                         double *S,
                                         double *V,
                                         int     nrows,
                                         int     ncols,
                                         double *B,
                                         double *x);

