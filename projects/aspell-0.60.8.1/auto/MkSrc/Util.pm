# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

package MkSrc::Util;

BEGIN {
  use Exporter;
  our @ISA = qw(Exporter);
  our @EXPORT = qw(false true
		   cmap one_of
		   to_upper to_lower to_mixed);
}

use strict;
use warnings;
no warnings qw(uninitialized);
no locale;

=head1 MkSrc::Util

This module contains various useful utility functions:

=over

=cut

=item false

Returns 0.

=item true

Returns 1.

=cut

sub false () {return 0}
sub true () {return 1}

=item cmap EXPR LIST

Apply EXPR to each item in LIST and than concatenate the result into
a string

=cut

sub cmap ( & @ ) 
{
  my ($sub, @d) = @_; 
  return join '', map &$sub, @d;
}

=item one_of STR LIST

Returns true if LIST contains at least one of STR.

=cut

sub one_of ( $ @ ) {
  my ($v, @l) = @_; 
  return scalar grep {$_ eq $v} @l;
}

=item to_upper STR

Convert STR to all uppercase and substitute spaces with underscores.

=cut

sub to_upper( $ ) {
  local ($_) = @_;
  s/\-/ /g;
  s/^\s+//; s/\s+$//; s/(\S)\s+(\S)/$1_$2/g;
  return uc($_);
}

=item to_lower STR

Convert STR to all lowercase and substitute spaces with underscores.

=cut

sub to_lower( $ ) {
  local ($_) = @_;
  s/\-/ /g;
  s/^\s+//; s/\s+$//; s/(\S)\s+(\S)/$1_$2/g;
  return lc($_);
}

=item to_mixed STR

Convert STR to mixed case where each new word startes with a
uppercase letter.  For example "feed me" would become "FeedMe".

=cut

sub to_mixed( $ ) {
  local ($_) = @_;
  s/\-/ /g;
  s/\s*(\S)(\S*)/\u$1\l$2\E/g;
  return $_;
}

=back

=cut

1;
