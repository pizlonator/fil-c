# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

package MkSrc::ProcImpl;

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

$info{forward}{proc}{impl} = sub {
  my ($type) = @_;
  return "$type->{type} ".to_mixed($type->{name}).";\n";
};

$info{group}{proc}{impl} = sub {
  my ($data) = @_;
  create_cc_file (type => 'impl',
		  cxx => true,
		  namespace => 'acommon',
		  dir => "lib",
		  pre_ext => "-c",
		  header => false,
		  data => $data,
		  accum => {headers => {$data->{name} => true} }
		  );
};

$info{class}{proc}{impl} = sub {
  my ($data, $accum) = @_;
  my $ret;
  foreach (grep {$_ ne ''} split /\s*,\s*/, $data->{'c impl headers'}) {
    $accum->{headers}{$_} = true;
  }
  my @d = @{$data->{data}};
  while (@d) {
    my $d = shift @d;
    my $need_wide = false;
    next unless one_of $d->{type}, qw(method constructor destructor);
    my @parms = @{$d->{data}} if exists $d->{data};
    my $m = make_c_method $data->{name}, $d, {mode=>'cc_cxx', use_name=>true, wide=>$d->{wide}}, %$accum;
    next unless defined $m;
    $ret .= "extern \"C\" $m\n";
    $ret .= "{\n";
    if (exists $d->{'c impl'}) {
      $ret .= cmap {"  $_\n"} split /\n/, $d->{'c impl'};
    } else {
      if ($d->{type} eq 'method') {
	my $ret_type = shift @parms;
	my $ret_native = to_type_name $ret_type, {mode=>'native_no_err', pos=>'return', wide=>$d->{wide}}, %$accum;
	my $snum = 0;
        my $call_fun = $d->{name};
        my @call_parms;
	foreach (@parms) {
	  my $n = to_lower($_->{name});
	  if ($_->{type} eq 'encoded string' && !exists($d->{'no conv'})) {
            $need_wide = true unless $d->{wide};
            die unless exists $d->{'posib err'};
            $accum->{headers}{'mutable string'} = true;
            $accum->{headers}{'convert'} = true;
            my $name = get_c_func_name $data->{name}, $d, {mode=>'cc_cxx', use_name=>true, wide=>$d->{wide}};
            $ret .= "  ths->temp_str_$snum.clear();\n";
            if ($d->{wide}) {
              $ret .= "  ${n}_size = get_correct_size(\"$name\", ths->to_internal_->in_type_width(), ${n}_size, ${n}_type_width);\n";
            } else {
              $ret .= "  PosibErr<int> ${n}_fixed_size = get_correct_size(\"$name\", ths->to_internal_->in_type_width(), ${n}_size);\n";
              if (exists($d->{'on conv error'})) {
                $ret .= "  if (${n}_fixed_size.get_err()) {\n";
                $ret .= "    ".$d->{'on conv error'}."\n";
                $ret .= "  } else {\n";
                $ret .= "    ${n}_size = ${n}_fixed_size;\n";
                $ret .= "  }\n";
              } else {
                $ret .= "  ths->err_.reset(${n}_fixed_size.release_err());\n";
                $ret .= "  if (ths->err_ != 0) return ".(c_error_cond $ret_type).";\n";
              }
            }
            $ret .= "  ths->to_internal_->convert($n, ${n}_size, ths->temp_str_$snum);\n";
            $ret .= "  unsigned int s$snum = ths->temp_str_$snum.size();\n";
            push @call_parms, "MutableString(ths->temp_str_$snum.mstr(), s$snum)";
            $snum++;
          } elsif ($_->{type} eq 'encoded string') {
            $need_wide = true unless $d->{wide};
            push @call_parms, $n, "${n}_size";
            push @call_parms, "${n}_type_width" if $d->{wide};
            $call_fun .= " wide" if $d->{wide};
	  } else {
	    push @call_parms, $n;
	  }
	}
	my $parms = '('.(join ', ', @call_parms).')';
	my $exp = "ths->".to_lower($call_fun)."$parms";
	if (exists $d->{'posib err'}) {
	  $accum->{headers}{'posib err'} = true;
	  $ret .= "  PosibErr<$ret_native> ret = $exp;\n";
	  $ret .= "  ths->err_.reset(ret.release_err());\n";
	  $ret .= "  if (ths->err_ != 0) return ".(c_error_cond $ret_type).";\n";
	  if ($ret_type->{type} eq 'void') {
	    $ret_type = {type=>'special'};
	    $exp = "1";
	  } else {
	    $exp = "ret.data";
	  }
	}
	if ($ret_type->{type} eq 'string obj') {
	  $ret .= "  ths->temp_str = $exp;\n";
	  $exp = "ths->temp_str.c_str()";
	} elsif ($ret_type->{type} eq 'encoded string') {
	  die; 
	  # This is not used and also not implemented right
	  $ret .= "  if (to_encoded_ != 0) (*to_encoded)($exp,temp_str_);\n";
	  $ret .= "  else                  temp_str_ = $exp;\n";
	  $exp = "temp_str_0.data()";
	}
	if ($ret_type->{type} eq 'const word list') {
	  $accum->{headers}{'word list'} = true;
          $ret .= "  if (ret.data)\n";
	  $ret .= "    const_cast<WordList *>(ret.data)->from_internal_ = ths->from_internal_;\n";
	}
	$ret .= "  ";
	$ret .= "return " unless $ret_type->{type} eq 'void';
	$ret .= $exp;
	$ret .= ";\n";
      } elsif ($d->{type} eq 'constructor') {
	my $name = $d->{name} ? $d->{name} : "new $data->{name}";
	$name =~ s/aspell\ ?//; # FIXME: Abstract this in a function
	$name = to_lower($name);
	shift @parms if exists $d->{'returns alt type'}; # FIXME: Abstract this in a function
	my $parms = '('.(join ', ', map {$_->{name}} @parms).')';
	$ret .= "  return $name$parms;\n";
      } elsif ($d->{type} eq 'destructor') {
	$ret .= "  delete ths;\n";
      }
    }
    $ret .= "}\n\n";
    unshift @d,{%$d, wide=>true} if $need_wide;
  }
  return $ret;
};

$info{struct}{proc}{impl} = $info{class}{proc}{impl};

$info{union}{proc}{impl} = $info{class}{proc}{impl};

1;
