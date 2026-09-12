/* Common data for erf/erfc.

Copyright (c) 2023-2025 Paul Zimmermann

The original version of this file was copied from the CORE-MATH
project (file src/binary64/erf/erf.c, revision 384ed01d).

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef _S_ERF_DATA_H
#define _S_ERF_DATA_H

extern const double __erf_data_c0[] attribute_hidden;
#define C0 __erf_data_c0

extern const double __erf_data_C[94][13] attribute_hidden;
#define C __erf_data_C
extern const double __erf_data_C2[47][27] attribute_hidden;
#define C2 __erf_data_C2
extern const double __erf_data_T1[][2] attribute_hidden;
#define T1 __erf_data_T1
extern const double __erf_data_T2[][2] attribute_hidden;
#define T2 __erf_data_T2
extern const double __erf_data_Q_1[] attribute_hidden;
#define Q_1 __erf_data_Q_1

extern const double __erf_data_exceptions_tiny[][3] attribute_hidden;
#define EXCEPTIONS_TINY __erf_data_exceptions_tiny
extern const size_t __erf_data_exceptions_tiny_len attribute_hidden;
#define EXCEPTIONS_TINY_LEN __erf_data_exceptions_tiny_len
extern const double __erf_data_p[] attribute_hidden;
#define P __erf_data_p

extern const double __erf_data_exceptions[][3] attribute_hidden;
#define EXCEPTIONS __erf_data_exceptions
extern const size_t __erf_data_exceptions_len attribute_hidden;
#define EXCEPTIONS_LEN __erf_data_exceptions_len

#endif
