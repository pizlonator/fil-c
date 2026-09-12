# Testing of printf conversions.
# Copyright (C) 2024-2026 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

BEGIN {
  FS = ":"
}

/^prec:/ {
  PREC = $2
  next
}

/^val:/ {
  val = $2
  # Prepend "+" for +Inf or +NaN value lacking a sign, because gawk
  # interpretes them as strings rather than numeric values in the
  # non-bignum mode unless a sign has been explicitly given.  Keep
  # original 'val' for reporting.
  value = gensub(/^(INF|NAN|inf|nan)/, "+\\1", 1, val)
  next
}

/^%/ {
  # Discard the trailing empty field, used to improve legibility of data.
  input = $--NF
  format = $1
  width = $2
  precision = "." $(NF - 1)
  # Discard any negative precision, which is to be taken as if omitted.
  sub(/\.-.*/, "", precision)
  # Simplify handling and paste the precision and width specified as
  # arguments to '*' directly into the format.
  sub(/\.\*/, precision, format)
  sub(/\*/, width, format)
  # Discard length modifiers.  They are only relevant to C data types.
  sub(/([DHLjhltz]|wf?[1-9][0-9]*)/, "", format)
  # Discard the '#' flag with the octal conversion if output starts with
  # 0 in the absence of this flag.  In that case no extra 0 is supposed
  # to be produced, but gawk prepends it anyway.
  if (format ~ /#.*o/)
    {
      tmpfmt = gensub(/#/, "", "g", format)
      tmpout = sprintf(tmpfmt, value)
      if (tmpout ~ /^ *0/)
	format = tmpfmt
    }
  # Likewise with the hexadecimal conversion where zero value with the
  # precision of zero is supposed to produce no characters, but gawk
  # outputs 0 instead.
  else if (format ~ /#.*[Xx]/)
    {
      tmpfmt = gensub(/#/, "", "g", format)
      tmpout = sprintf(tmpfmt, value)
      if (tmpout ~ /^ *$/)
	format = tmpfmt
    }
  # AWK interpretes input opportunistically as a number, which interferes
  # with how the 'c' conversion works: "a" input will result in "a" output
  # however "0" input will result in "^@" output rather than "0".  Force
  # the value to be interpreted as a string then, by appending "".
  output = sprintf(format, value "")
  # Make up for various anomalies with the handling of +/-Inf and +/-NaN
  # values and reprint the output produced using the string conversion,
  # with the field width carried over and the relevant flags handled by
  # hand.
  if (format ~ /[EFGefg]/ && value ~ /(INF|NAN|inf|nan)/)
    {
      minus = format ~ /-/ ? "-" : ""
      sign = value ~ /-/ ? "-" : format ~ /\+/ ? "+" : format ~ / / ? " " : ""
      if (format ~ /^%[^\.1-9]*[1-9][0-9]*/)
	width = gensub(/^%[^\.1-9]*([1-9][0-9]*).*$/, "\\1", 1, format)
      else
	width = ""
      output = gensub(/[-+ ]/, "", "g", output)
      output = sprintf("%" minus width "s", sign output)
    }
  # Produce "+" where the '+' flag has been used with a signed integer
  # conversion for zero value, observing any field width in effect.
  # In that case "+" is always supposed to be produced, but with the
  # precision of zero gawk in the non-bignum mode produces any padding
  # requested only.
  else if (format ~ /\+.*[di]/ && value == 0)
    {
      output = gensub(/^( *) $/, format ~ /-/ ? "+\\1" : "\\1+", 1, output)
      output = gensub(/^$/, "+", 1, output)
    }
  # Produce " " where the space flag has been used with a signed integer
  # conversion for zero value.  In that case at least one " " is
  # supposed to be produced, but with the precision of zero gawk in the
  # non-bignum mode produces nothing.
  else if (format ~ / .*[di]/ && value == 0)
    {
      output = gensub(/^$/, " ", 1, output)
    }
  if (output != input)
    {
      printf "(\"%s\"%s%s, %s) => \"%s\", expected \"%s\"\n", \
	     $1, (NF > 2 ? ", " $2 : ""), (NF > 3 ? ", " $3 : ""), val, \
	     input, output > "/dev/stderr"
      status = 1
    }
  next
}

{
  printf "unrecognized input: \"%s\"\n", $0 > "/dev/stderr"
  status = 1
}

END {
  exit status
}
