/* Handling of color output.
   Copyright (C) 2017 The Qt Company
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2011.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */


#ifndef COLOR_H
#define COLOR_H 1

/* Command line parser.  */
extern const struct argp color_argp;

/* Coloring mode.  */
enum color_enum
  {
    color_never = 0,
    color_always,
    color_auto
  } __attribute__ ((packed));
extern enum color_enum color_mode;

/* Colors to use for the various components.  */
extern char *color_address;
extern char *color_bytes;
extern char *color_mnemonic;
extern char *color_operand1;
extern char *color_operand2;
extern char *color_operand3;
extern char *color_operand4;
extern char *color_operand5;
extern char *color_label;
extern char *color_undef;
extern char *color_undef_tls;
extern char *color_undef_weak;
extern char *color_symbol;
extern char *color_tls;
extern char *color_weak;

extern const char color_off[];

#endif /* color.h */
