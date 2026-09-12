/* Data definitions for erfc implementation.

Copyright (c) 2022-2025 Alexei Sibidanov, Paul Zimmermann, Tom Hubrecht and
Claude-Pierre Jeannerod.

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

#ifndef _S_ERFC_DATA_H
#define _S_ERFC_DATA_H

extern const double __erfc_data_T[6][13] attribute_hidden;
#define T __erfc_data_T

extern const double __erfc_data_E2[] attribute_hidden;
#define E2 __erfc_data_E2

extern const double __erfc_data_Tacc[10][30] attribute_hidden;
#define Tacc __erfc_data_Tacc

extern const double __erfc_data_exceptions[22][3] attribute_hidden;
#define EXCEPTIONS_ERFC __erfc_data_exceptions

extern const double __erfc_data_exceptions_accurate[17][3] attribute_hidden;
#define EXCEPTIONS_ACCURATE_ERFC __erfc_data_exceptions_accurate

extern const double __erfc_data_exceptions_accurate_2[29][3] attribute_hidden;
#define EXCEPTIONS_ACCURATE_ERFC_2 __erfc_data_exceptions_accurate_2

#endif
