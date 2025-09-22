#! /usr/bin/env perl

# Generate a release announcement message.

# Copyright (C) 2007-2021 Free Software Foundation, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# Written by Akim Demaille.

use warnings;
use 5.005;
use strict;

my %option;
my %directive;
my $scanner = `grep -i '"%[a-z]' $ARGV[0]`;
$scanner =~ s/"\[-_\]"/-/g;
while (<STDIN>)
{
    if (/^\s*             # Initial spaces.
        (?:(-\w),\s+)?    # $1: $short: Possible short option.
        (--[-\w]+)        # $2: $long:  Mandatory long option.
        (\[?)             # $3: $opt:   '[' iff the argument is optional.
        (?:=(\S+))?       # $4: $arg:   Possible argument name.
        \s                # Spaces.
        /x)
    {
        my ($short, $long, $opt, $arg) = ($1, $2, $3, $4);
        $short = '' if ! defined $short;
        $short = '-d' if $long eq '--defines' && ! $short;
        my $dir = '%' . substr($long, 2);
        if (index ($scanner, "\"$dir\"") < 0)
        {
          if ($long eq '--force-define') { $dir = '%define'; }
          else { $dir = ''; }
        }
        if ($arg)
        {
            # if $opt, $arg contains the closing ].
            substr ($arg, -1) = ''
                if $opt eq '[';
            $arg = lc ($arg);
            my $dir_arg = $arg;
            # If the argument is complete (e.g., for --define[=NAME[=VALUE]]),
            # put each word in @var, to build @var{name}[=@var{value}], not
            # @var{name[=value]}].
            $arg =~ s/(\w+)/\@var{$1}/g;
            my $long_arg = "=$arg";
            if ($opt eq '[') {
              $long_arg = "[$long_arg]";
              $arg = "[$arg]";
            }
            # For arguments of directives: this only works if all arguments
            # are strings and have the same syntax as on the command line.
            if ($dir_arg eq 'name[=value]')
            {
                # -D/-F do not add quotes to the argument.
                $dir_arg =
                    $dir eq "%define"
                    ? '@var{name} [@var{value}]'
                    : '@var{name} ["@var{value}"]';
            }
            else
            {
                $dir_arg =~ s/(\w+)/\@var{"$1"}/g;
                $dir_arg = '[' . $dir_arg . ']'
                    if $opt eq '[';
            }
            $long = "$long$long_arg";
            $short = "$short $arg" if $short && $short ne '-d';
            $dir = "$dir $dir_arg" if $dir;
        }
        $option{$long} = $short;
        $directive{$long} = $dir;
    }
}

my $sep = '';
foreach my $long (sort keys %option)
{
    # Couldn't find a means to escape @ in the format (for @item, @tab), so
    # pass it as a literal to print.
format STDOUT =
@item @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< @tab @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< @tab @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
{
  '@', '@option{' . $long . '}',
  '@', $option{$long} ? ('@option{' . $option{$long} . '}') : '',
  '@', $directive{$long} ? ('@code{' . $directive{$long} . '}') : ''
}
.
    write;
}
