# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

package MkSrc::ProcCc;

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

sub make_c_object ( $ @ );

$info{group}{proc}{cc} = sub {
  my ($data,@rest) = @_;
  my $ret;
  my $stars = (70 - length $data->{name})/2;
  $ret .= "/";
  $ret .= '*'x$stars;
  $ret .= " $data->{name} ";
  $ret .= '*'x$stars;
  $ret .= "/\n";
  foreach my $d (@{$data->{data}}) {
    $ret .= "\n\n";
    $ret .= $info{$d->{type}}{proc}{cc}->($d,@rest);
  }
  $ret .= "\n\n";
  return $ret;
};

$info{enum}{proc}{cc} = sub {
  my ($d,@rest) = @_;
  my $n = "Aspell".to_mixed($d->{name});
  return ("\n".
	  make_desc($d->{desc}).
	  "enum $n {" .
	  join(', ',
	       map {"Aspell".to_mixed($d->{prefix}).to_mixed($_->{type})}
	       @{$d->{data}}).
	  "};\n" .
	  "typedef enum $n $n;\n"
	  );
};

$info{struct}{proc}{cc} = sub {
  return make_c_object "struct", @_;
};

$info{union}{proc}{cc} = sub {
  return make_c_object "union", @_;
};

$info{class}{proc}{cc} = sub {
  my ($d,$accum) = @_;
  my $class = $d->{name};
  my $classname = "Aspell".to_mixed($class);
  my $ret = "";
  $ret .= "typedef struct $classname $classname;\n\n";
  foreach (@{$d->{data}}) {
    my $s = make_c_method($class, $_, {mode=>'cc'}, %$accum);
    next unless defined $s;
    $ret .= "\n";
    $ret .= make_desc($_->{desc});
    $ret .= make_c_method($class, $_, {mode=>'cc'}, %$accum).";\n";
    if (grep {$_->{type} eq 'encoded string'} @{$_->{data}}) {
      $ret .= make_c_method($class, $_, {mode=>'cc', wide=>true}, %$accum).";\n";
      $ret .= make_wide_macro($class, $_, {mode=>'cc'}, %$accum);
    }
    $ret .= "\n".$_->{'cc extra'}."\n" if defined $_->{'cc extra'};
  }
  $ret .= "\n";
  return $ret;
};

$info{func}{proc}{cc} = sub {
  my ($d) = @_;
  return (make_desc($d->{desc}).
          make_func("aspell ".$d->{name}, @{$d->{data}}, {mode => 'cc'}).';');
};

$info{errors}{proc}{cc} = sub {
  my ($d) = @_;
  my $p;
  my $ret;
  $p = sub {
    my ($level, $data) = @_;
    return unless defined $data;
    foreach my $d (@$data) {
      $ret .= "extern const struct AspellErrorInfo * const ";
      $ret .= ' 'x$level;
      $ret .= "aerror_";
      $ret .= to_lower($d->{type});
      $ret .= ";\n";
      $p->($level + 2, $d->{data});
    }
  };
  $p->(0, $d->{data});
  return $ret;
};

sub make_c_object ( $ @ ) {
  my ($t, $d, $accum) = @_;
  $accum = {} unless defined $accum;
  my $struct;
  $struct .= "Aspell";
  $struct .= to_mixed($d->{name});
  return (join "\n\n", grep {$_ ne ''}
	  join ('',
		"$t $struct {\n",
		(map {"\n".make_desc($_->{desc},2).
			  "  ".to_type_name($_, {mode=>'cc'}). ";\n"}
		 grep {$_->{type} ne 'method'
			   && $_->{type} ne 'cxx constructor'}
		 @{$d->{data}}),
		"\n};\n"),
	  "typedef $t $struct $struct;",
	  join ("\n",
		map {make_c_method($d->{name}, $_, {mode=>'cc'}, %$accum).";"}
		grep {$_->{type} eq 'method'}
		@{$d->{data}})
	  )."\n";
}

1;
