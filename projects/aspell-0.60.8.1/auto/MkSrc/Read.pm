# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

package MkSrc::Read;

BEGIN {
  use Exporter;
  @ISA = qw(Exporter);
  @EXPORT = qw(read);
}

use strict;
use warnings;
no warnings qw(uninitialized);
no locale;

use MkSrc::Util;
use MkSrc::Type;
use MkSrc::Info;
use MkSrc::Methods;

my $line = '';
my $level = 0;
my $base_level = 0;
my $in_pod = undef;

my $master_data = {type=>'root', name=>undef};
# master_data contains all the information in mk-src.in and then some.
# It has the following structure
#   <tree>
#   <tree> := <options>
#             data => <tree>
# where each tree represents an entry in mk-src.in.  
# The following two options are always provided:
#   name: the name of the entry
#   type: the category or type name
# Additional options are the same as specified in %info

=head1 MkSrc::Read

=over

=item read

Read in "mk-src.in" and returns a data structure which has the
following format:

    <tree>
    <tree> := <options>
              data => <tree>
  where each tree represents an entry in mk-src.in.  
  The following two options are always provided:
    name: the name of the entry
    type: the category or type name
  Additional options are the same as specified in %info

=back

=cut

sub advance ( );
sub parse ( $ $ );
sub prep ( $ $ $ );

sub read ( )
{
  open IN, "mk-src.in" or die;
  advance;
  parse $master_data, 0;
  close IN;
  prep $master_data, '', {};
  return $master_data;
}

sub need_options ( $ );
sub valid_option ( $ $ );
sub valid_group ( $ $ );
sub store_group ( $ $ );

sub advance ( ) {
  $line = undef;
  do {
    $line = <IN>;
    return unless defined $line;
    $in_pod = $1 if $line =~ /^\=(\w+)/;
    $line = '' if $in_pod;
    $in_pod = undef if $in_pod && $in_pod eq 'cut';
    $line =~ s/(?<!\\)\#.*$//;
    $line =~ s/^(\t*)//;
    $level = $base_level + length($1);
      $line =~ s/\s*$//;
    ++$base_level if $line =~ s/^\{$//;
    --$base_level if $line =~ s/^\}$//;
    $line =~ s/\\([{}#\\])/$1/g;
  } while ($line eq '');
  #print "$level:$line\n";
}
  
sub parse ( $ $ ) {
  my ($data, $this_level) = @_;
  if (need_options $data) {
    for (;;) {
      return          if $level < $this_level;
      return          unless defined $line;
      die             if $level > $this_level;
      last            if $line eq '/';
      my $k;
      ($k, $line) = split /\=\>/, $line;
      $k =~ s/^\s*(.+?)\s*$/$1/;
      my $v = $line;
      print STDERR "The option \"$k\" is invalid for the group \"$data->{type}\"\n"
	  unless valid_option $data, $k;
      advance;
      for (;;) {
	return unless defined $line;
	last if $level <= $this_level;
	$v .= "\n$line";
	advance;
      }
      $v =~ s/^[ \t\n]+//;
      $v =~ s/[ \t\n]+$//;
      $data->{$k} = $v;
    }
    return unless $line eq '/';
    advance;
  } else {
    advance if $line eq '/';
  }
  $data->{data} = [];
  for (;;) {
    return if $level < $this_level;
    return unless defined $line;
    die    if $level > $this_level;
    my ($type, $name) = split /:/, $line;
    $type =~ s/^\s*(.+?)\s*$/$1/;
    $name =~ s/^\s*(.+?)\s*$/$1/;
    print STDERR "The subgroup \"$type\" is invalid in the group \"$data->{type}\"\n"
	unless valid_group $data, $type;
    my $d = {type=>$type, name=>$name};
    store_group $d, $data;
    advance;
    parse($d, $this_level + 1);
  }
}

sub prep ( $ $ $ ) {
  my ($data, $group, $stack) = @_;
  my $d = creates_type $data;
  update_type $d->{name}, {%$d,created_in=>$group} if (defined $d);
  $group = $data->{name} if $data->{type} eq 'group';
  $stack = {%$stack, prev=>$data, $data->{type}=>$data};
  if ($data->{type} eq 'method') {
    die unless $data->{data};
    $data->{data}[0]{'posib err'} = true if exists $data->{'posib err'};
  }
  my $i = 0;
  my $lst = $data->{data};
  return unless defined $lst;
  while ($i != @$lst) {
    my $d = $lst->[$i];
    if (exists $methods{$d->{type}}) {
      splice @$lst, $i, 1, copy_methods($d, $data, $stack->{class}{name});
    } else {
      prep $d, $group, $stack;
      ++$i;
    }
  }
}

#
# Parser helper functions
#

sub need_options ( $ ) {
  my ($i, $o) = @_;
  my $options = $info{$i->{type}}{options};
  return true unless ref $options eq 'ARRAY';
  return true unless @$options == 0;
  return false;
}

sub valid_option ( $ $ ) {
  my ($i, $o) = @_;
  my $options = $info{$i->{type}}{options};
  return true unless ref $options eq 'ARRAY';
  return scalar ( grep {$_ eq $o} @$options);
}

sub valid_group ( $ $ ) {
  my ($i, $t) = @_;
  my $groups = $info{$i->{type}}{groups};
  return true unless ref $groups eq 'ARRAY';
  return scalar ( grep {$_ eq $t} @$groups);
}

sub store_group ( $ $ ) {
  my ($d, $data) = @_;
  if ($d->{type} eq 'methods') {
    $methods{"$d->{name} methods"} = $d;
#   Don't usa as groups is now undef
#   push @{$info{class}{groups}}, "$d->{name} methods";
  } else {
    push @{$data->{data}}, $d;
  }
}

1;
