/* Common data for sinpif/cospif implementations.

Copyright (c) 2022-2025 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (src/binary32/sinpi/sinpif.c, revision f786e13).

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

#ifndef _SINCOSPIF_H
#define _SINCOSPIF_H

extern const double __sincospi_sn[] attribute_hidden;
#define SN __sincospi_sn
extern const double __sincospi_cn[] attribute_hidden;
#define CN __sincospi_cn
extern const double __sincospi_S[] attribute_hidden;
#define S __sincospi_S

#endif
