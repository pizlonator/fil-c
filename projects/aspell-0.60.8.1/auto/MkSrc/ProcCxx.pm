# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

package MkSrc::ProcCxx;

BEGIN {
  use Exporter;
  our @ISA = qw(Exporter);
  our @EXPORT = qw(%info %types %methods);
}

use strict;
use warnings;
no warnings qw(uninitialized);
no locale;

use MkSrc::Util;
use MkSrc::CcHelper;
use MkSrc::Info;

$info{forward}{proc}{cxx} = sub {
  my ($type) = @_;
  my $n = to_mixed($_->{name});
  if ($type->{type} eq 'class') {
    return "typedef Aspell$n * ${n}Ptr;\n";
  } elsif (one_of $type->{type}, 'struct', 'union') {
    return "typedef Aspell$n $n;\n";
  } else {
    return "";
  }
};

$info{group}{proc}{cxx} = sub {
  my ($data, $accum) = @_;
  my $str;
  foreach my $d (@{$data->{data}}) {
    my $s = $info{$d->{type}}{proc}{cxx}->($d, $accum);
    $str .= "\n\n$s" if defined $s;
  }
  my $ret = "";
  if (defined $str) {
    my $stars = (70 - length $data->{name})/2;
    $ret .= "/".('*'x$stars)." $data->{name} ".('*'x$stars)."/\n";
    $ret .= $str;
    $ret .= "\n\n";
  }
  return $ret;
};

$info{enum}{proc}{cxx} = sub {
  my ($d) = @_;
  my $n = to_mixed($d->{name});
  return ("\n".
	  make_desc($d->{desc}).
	  "typedef Aspell$n $n;\n".
	  join('',
	       map {my $en = to_mixed($d->{prefix}).to_mixed($_->{type});
		    "static const $n $en = Aspell$en;\n"}
	       @{$d->{data}})
	  );
};

$info{struct}{proc}{cxx} = sub {
  my ($d, $accum) = @_;
  my $n = to_mixed($d->{name});
  $accum->{types}{$d->{name}} = $d;
  return undef;
};

$info{union}{proc}{cxx} = sub {
  my ($d, $accum) = @_;
  my $n = to_mixed($d->{name});
  $accum->{types}{$d->{name}} = $d;
  return undef;
};

$info{class}{proc}{cxx} = sub {
  my ($d, $accum) = @_;
  my $class = $d->{name};
  my $cn = to_mixed($class);
  my $cl = to_lower($class);
  my $cp = "${cn}Ptr";
  $accum->{types}{$d->{name}} = $d;
  my $ret;
  $ret = <<"---";
struct ${cn}Ref {
  $cp ptr;
  ${cn}Ref ($cp rhs) : ptr(rhs) {}
};

class $cn {
  $cp ptr;
public:

  explicit $cn($cp p = 0) : ptr(p) {}

  $cn($cn & other) : ptr (other.release()) {}
  ~$cn() {del();}

  $cn & operator=($cn & other) 
    {reset(other.release()); return *this;}

  $cp get()         const {return ptr;}
  operator $cp ()   const {return ptr;}

  $cp release () {$cp p = ptr; ptr = 0; return p;}

  void del() {delete_aspell_$cl(ptr); ptr = 0;}
  void reset($cp p) {assert(ptr==0); ptr = p;}
  $cn & operator=($cp p) {reset(p); return *this;}
  
  $cn(${cn}Ref rhs) : ptr(rhs.ptr) {}

  $cn& operator= (${cn}Ref rhs) {reset(rhs.ptr); return *this;}

  operator ${cn}Ref() {return ${cn}Ref(release());}

---
  foreach (@{$d->{data}}) {
    next unless $_->{type} eq 'method';
    $ret .= "  ".make_cxx_method($_, {mode=>'cxx'})."\n";
    $ret .= "  {\n";
    $ret .= "    ".call_c_method($class, $_, {mode=>'cc', this_name=>'ptr'}).";\n";
    $ret .= "  }\n\n";
  }
  $ret .= "};\n";
  foreach (@{$d->{data}}) {
    next unless $_->{type} eq 'constructor';
    $ret .= "static inline ";
    $ret .= make_c_method $class, $_, {mode=>'cxx', no_aspell=>true};
    $ret .= "{\n";
    $ret .= "  ".call_c_method($class, $_, {mode=>'cc'}).";\n";
    $ret .= "}\n\n";
  }
  return $ret;
};

$info{errors}{proc}{cxx} = sub {
  my ($d) = @_;
  my $p;
  my $ret;
  $p = sub {
    my ($level, $data) = @_;
    return unless defined $data;
    foreach my $d (@$data) {
      $ret .= "static const ErrorInfo * const ";
      $ret .= ' 'x$level;
      $ret .= to_lower($d->{type});
      $ret .= "_error" if defined $d->{data} || $level == 0;
      $ret .= " = ";
      $ret .= "aerror_";
      $ret .= to_lower($d->{type});
      $ret .= ";\n";
      $p->($level + 2, $d->{data});
    }
  };
  $p->(0, $d->{data});
  return $ret;
};

1;
