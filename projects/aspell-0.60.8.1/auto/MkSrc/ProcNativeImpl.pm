# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

package MkSrc::ProcNativeImpl;

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
use MkSrc::Create;

$info{forward}{proc}{native_impl} = sub {
  my ($type) = @_;
  return "$type->{type} ".to_mixed($type->{name}).";\n";
};

$info{group}{proc}{native_impl} = sub {
  my ($data) = @_;
  create_cc_file (type => 'native_impl',
		  cxx => true,
		  namespace => 'acommon',
		  dir => "common",
		  header => false,
		  data => $data,
		  accum => {headers => {$data->{name} => true} }
		  );
};

$info{errors}{proc}{native_impl} = sub {
  my $ret;
  my $p;
  $p = sub {
    my ($isa, $parms, $data) = @_;
    my @parms = (@$parms, (split /, */, $data->{parms}));
    my $parm_idx = sub {
      my ($p) = @_;
      return 0             if $p eq 'prim';
      for (my $i = 0; $i != @parms; ++$i) {
	return $i+1 if $parms[$i] eq $p;
      }
      die "can't find parm for \"$p\"";
    };
    my $proc_mesg = sub {
      my @mesg = split /\%(\w+)/, $_[0];
      my $mesg = '';
      while (true) {
	my $m = shift @mesg;
	$m =~ s/\"/\\\"/g;
	$mesg .= $m;
	my $p = shift @mesg;
	last unless defined $p;
	$mesg .= "%$p:";
	$mesg .= $parm_idx->($p);
      }
      if (length $mesg == 0) {
	$mesg = 0;
      } else {
	$mesg = "N_(\"$mesg\")";
      }
      return $mesg;
    };
    my $mesg = $proc_mesg->($data->{mesg});
    my $name = "aerror_".to_lower($data->{type});
    $ret .= "static const ErrorInfo $name\_obj = {\n";
    $ret .= "  ".(defined $isa ? "$isa": 0).", // isa\n";
    $ret .= "  $mesg, // mesg\n";
    $ret .= "  ".scalar @parms.", // num_parms\n";
    $ret .= "  {".(join ', ', map {"\"$_\""} (@parms ? @parms : ("")))."} // parms\n";
    $ret .= "};\n";
    $ret .= "extern \"C\" const ErrorInfo * const $name = &$name\_obj;\n";
    $ret .= "\n";
    foreach my $d (@{$data->{data}}) {
      $ret .= $p->($name, \@parms, $d);
    }
  };
  my ($data, $accum) = @_;
  $accum->{headers}{'error'} = true;
  foreach my $d (@{$data->{data}}) {
    $ret .= $p->(undef, [], $d);
  }
  return $ret;
};

1;
