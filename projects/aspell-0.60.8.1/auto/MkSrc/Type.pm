# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

package MkSrc::Type;

BEGIN {
  use Exporter;
  our @ISA = qw(Exporter);
  our @EXPORT = qw(creates_type update_type finalized_type);
}

use strict;
use warnings;
no warnings qw(uninitialized);
no locale;

sub creates_type ( $ );
sub update_type ( $ ; $ );
sub finalized_type ( $ );

use MkSrc::Util;
use MkSrc::Info;

#
# Type Functions
#


sub creates_type ( $ ) 
{
  my ($i) = @_;
  my $d;
  $d->{type} = $info{$i->{type}}{creates_type};
  return undef unless defined $d->{type};
  $d->{name} = $i->{name};
  $d->{treat_as} =
    ($i->{type} eq 'basic'                                    ? 'special'
     : exists $i->{'treat as object'} || $i->{type} eq 'enum' ? 'object'
     :                                                          'pointer');
  if (my $name = $info{$i->{type}}{creates_name}) {
    $d->{name} = $name->($i);
  }
  return $d;
}

sub update_type ( $ ; $ ) 
{
  my ($name, $data) = @_;
  my $d = $types{$name};
  $types{$name} = $d = {} unless defined $d;
  $d->{data} = $data if defined $data;
  $d->{data} = {} unless defined $d->{data};
  return $d;
}

sub finalized_type ( $ ) 
{
  my ($name) = @_;

  my $d = $types{$name};
  $types{$name} = $d = {data=>{}} unless defined $d;
  return $d unless exists $d->{data};

  while (my ($k,$v) = each %{$d->{data}}) {
    $d->{$k} = defined $v ? $v : true;
  }
  delete $d->{data};

  local $_ = $name;

  s/^const //       and $d->{const}   = true;
  s/^array (\d+) // and $d->{array}   = $1;
  s/ ?pointer$//    and $d->{pointer} = true;
  s/ ?object$//     and $d->{pointer} = false;

  $_ = 'void' if length $_ == 0;

  my $r = finalized_type $_;

  $d->{type} = exists $r->{type} ? $r->{type} : 'unknown';
  $d->{name} = $_;
  $d->{orig_name} = $name;
  $d->{pointer} = ($r->{treat_as} eq 'pointer')
    unless exists $d->{pointer};
  $d->{const} = false unless $d->{pointer};
  $d->{created_in} = $r->{created_in};

  return $d;

}
1;
