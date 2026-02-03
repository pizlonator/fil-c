# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

package MkSrc::ProcNative;

BEGIN {
  use Exporter;
  our @ISA = qw(Exporter);
}

use strict;
use warnings;
no warnings qw(uninitialized);
no locale;

use MkSrc::Util;
use MkSrc::CcHelper;
use MkSrc::Info;
use MkSrc::Create;
use MkSrc::Type;

sub make_native_obj ( $ @ );

$info{forward}{proc}{native} = sub {
  my ($type) = @_;
  return "$type->{type} ".to_mixed($type->{name}).";\n";
};

$info{group}{proc}{native} = sub {
  my ($data) = @_;
  return if exists $data->{'no native'};
  create_cc_file (type => 'native',
		  cxx => true,
		  namespace => 'acommon',
		  dir => "common",
		  header => true,
		  data => $data);
};

$info{enum}{proc}{native} = sub {
  my ($data) = @_;
  my $n = to_mixed($data->{name});
  return ("enum $n {" .
	  join (',',
		map {to_mixed($data->{prefix}).to_mixed($_->{type})}
		@{$data->{data}}).
	  "};\n");
};


$info{struct}{proc}{native} = sub {
  return make_native_obj 'struct', @_;
};

$info{union}{proc}{native} = sub {
  return make_native_obj 'union', @_;
};

$info{class}{proc}{native} = sub {
  return make_native_obj 'class', @_;
};

$info{errors}{proc}{native} = sub {
  my ($data, $accum) = @_;
  my $ret;
  $accum->{types}{"error info"} = finalized_type "error info";
  my $p0;
  $p0 = sub {
    my ($level, $data) = @_;
    return unless defined $data;
    foreach my $d (@$data) {
      $ret .= "extern \"C\" const ErrorInfo * const ";
      $ret .= ' 'x$level;
      $ret .= "aerror_";
      $ret .= to_lower($d->{type});
      $ret .= ";\n";
      $p0->($level + 2, $d->{data});
    }
  };
  my $p1;
  $p1 = sub {
    my ($level, $data) = @_;
    return unless defined $data;
    foreach my $d (@$data) {
      $ret .= "static const ErrorInfo * const ";
      $ret .= ' 'x$level;
      $ret .= to_lower($d->{type});
      $ret .= "_error" if defined $d->{data} || $level == 0;
      $ret .= " = aerror_";
      $ret .= to_lower($d->{type});
      $ret .= ";\n";
      $p1->($level + 2, $d->{data});
    }
  };
  $p0->(0, $data->{data});
  $ret .= "\n\n";
  $p1->(0, $data->{data});
  return $ret;
};

sub make_cxx_constructor ( $ $ ; \% ) {
  my ($class, $p, $accum) = @_;
  my $ret;
  $ret .= to_mixed($class);
  $ret .= "(";
  $ret .= join ', ', map {to_type_name $_, {mode=>'native',pos=>'parm'}, %$accum} @$p;
  $ret .= ")";
  return $ret;
}

sub make_native_obj ( $ @ ) {
  my ($t, $data, $accum) = @_;
  my $obj = to_mixed($data->{name});
  my @defaults;
  my @public;
  foreach my $d (@{$data->{data}}) {
    next unless $d->{type} eq 'public';
    next if $d->{name} eq $data->{name};
    push @public, to_mixed($d->{name});
    my $typ = finalized_type $d->{name};
    $accum->{headers}{$typ->{created_in}} = true;
  }
  my $ret;
  $ret .= "$t $obj ";
  $ret .= ": ".join(', ', map {"public $_"} @public).' ' if @public;
  $ret .= "{\n";
  $ret .= " public:\n" if $t eq 'class';
  foreach my $d (@{$data->{data}}) {
    next if exists $d->{'c only'};
    next if one_of $d->{type}, qw(constructor destructor public);
    $ret .= "  ";
    if ($d->{type} eq 'method') {
      my $is_vir = $t eq 'class' && !exists $d->{'cxx impl'};
      $ret .= "virtual " if $is_vir;
      $ret .= make_cxx_method $d, {mode=>'native'}, %$accum;
      $ret .= $is_vir ? " = 0;\n" 
	  : exists $d->{'cxx impl'} ? " { $d->{'cxx impl'}; }\n"
	  : ";\n";
    } elsif ($d->{type} eq 'cxx constructor') {
      $ret .= make_cxx_constructor $data->{name}, $d->{data}, %$accum;
      $ret .= exists $d->{'cxx impl'} ? " $d->{'cxx impl'}\n"
	  : ";\n";
    } else { # is a type
      if (exists $d->{default}) {
	push @defaults, $d;
      }
      if ($d->{type} eq 'cxx member') {
	foreach (split /\s*,\s*/, $d->{'headers'}) {
	  $accum->{headers}{$_} = true;
	}
	$ret .= $d->{what};
      } elsif ($t eq 'class') {
	$ret .= to_type_name $d, {mode=>'native'}, %$accum;
	$ret .= "_";
      } else {
	$ret .= to_type_name $d, {mode=>'cc_cxx'}, %$accum;
      }
      $ret .= ";\n";
    }
  }
  if (@defaults || $t eq 'class') {
    $ret .= "  $obj()";
    if (@defaults) {
      $ret .= " : ";
      $ret .= join ', ', map {to_lower($_->{name}).($t eq 'class'?'_':'')."($_->{default})"} @defaults;
    }
    $ret .= " {}\n";
  }
  $ret .= "  virtual ~${obj}() {}\n" if $t eq 'class';
  $ret .= "};\n";
  foreach my $d (@{$data->{data}}) {
    next unless $d->{type} eq 'constructor';
    $ret .= make_c_method $data->{name}, $d, {mode=>'native',no_aspell=>false}, %$accum;
    $ret .= ";\n";
  }
  return $ret;
}

1;
