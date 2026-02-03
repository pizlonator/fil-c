# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

package MkSrc::Methods;

BEGIN {
  use Exporter;
  our @ISA = qw(Exporter);
  our @EXPORT = qw(copy_methods);
}

use strict;
use warnings;
no warnings qw(uninitialized);
no locale;

use MkSrc::Info;

sub copy_n_sub ( $ $ );
sub copy_methods ( $ $ $ ) {
  my ($d, $data, $class_name) = @_;
  my $ms = $methods{$d->{type}};
  if (not defined $d->{name}) {
    $d->{name} = $class_name;
    $d->{name} =~ s/ [^ ]+$// if $ms->{strip} == 1;
  }
  my @lst;
  if (defined $ms->{'c impl headers'}) {
    $data->{'c impl headers'} .= ",$ms->{'c impl headers'}";
  }
  foreach my $m (@{$ms->{data}}) {
    push @lst, copy_n_sub($m, $d->{name});
    $lst[-1]{prefix} = $m->{prefix} if exists $d->{prefix};
  }
  return @lst
}

sub copy_n_sub ( $ $ ) {
  my ($d, $name) = @_;
  my $new_d = {};
  foreach my $k (keys %$d) {
    if ($k eq 'data') {
      $new_d->{data} = [];
      foreach my $d0 (@{$d->{data}}) {
	push @{$new_d->{data}}, copy_n_sub($d0, $name);
      }
    } else {
      $new_d->{$k} = $d->{$k};
      $new_d->{$k} =~ s/\$/$name/g unless ref $new_d->{$k};
    }
  }
  return $new_d;
}

1;
