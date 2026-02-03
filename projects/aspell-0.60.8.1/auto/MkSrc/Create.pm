# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

package MkSrc::Create;

BEGIN {
  use Exporter;
  our @ISA = qw(Exporter);
  our @EXPORT = qw(create_cc_file create_file);
}

use strict;
use warnings;
no warnings qw(uninitialized);
no locale;

use MkSrc::Util;
use MkSrc::Info;

=head1 MKSrc::Create

=over

=item create_cc_file PARMS

Create a source file.

  Required Parms: type, dir, name, data
   Boolean Parms: header, cxx
  Optional Parms: namespace (required if cxx), pre_ext, accum

=item create_file FILENAME DATA

Writes DATA to FILENAME but only if DATA differs from the content of
the file and the string:

    Automatically generated file.

is present in the existing file if it already exists.

=back

=cut

sub create_cc_file ( % );

sub create_file ( $ $ );

sub create_cc_file ( % )  {
  my (%p) = @_;
  $p{name} = $p{data}{name} unless exists $p{name};
  $p{ext} = $p{cxx} ? ($p{header} ? 'hpp' : 'cpp') : 'h';
  my $body;
  my %accum = exists $p{accum} ? (%{$p{accum}}) : ();
  foreach my $d (@{$p{data}{data}}) {
    next unless exists $info{$d->{type}}{proc}{$p{type}};
    $body .= $info{$d->{type}}{proc}{$p{type}}->($d, \%accum);
  }
  return unless length($body) > 0;
  my $file = <<'---';
/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

---
  my $hm = "ASPELL_". to_upper($p{name})."__".to_upper($p{ext});
  $file .= "#ifndef $hm\n#define $hm\n\n" if $p{header} && $p{name} ne 'errors';
  # hack to avoid including aerror_* symbols as two different type of symbols
  $file .= "#if !defined($hm) && !defined(ASPELL_ASPELL__H)\n#define $hm\n\n" if $p{header} && $p{name} eq 'errors';
  $file .= "#include \"aspell.h\"\n" if $p{type} eq 'cxx';
  $file .= "#include \"settings.h\"\n" if $p{type} eq 'native_impl' && $p{name} eq 'errors';
  $file .= "#include \"gettext.h\"\n" if $p{type} eq 'native_impl' && $p{name} eq 'errors';
  $file .= cmap {"#include <$_>\n"} sort keys %{$accum{sys_headers}};
  $file .= cmap {"#include \"".to_lower($_).".hpp\"\n"} sort keys %{$accum{headers}};
  $file .= "\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n" if $p{header} && !$p{cxx};
  $file .= join('', grep {defined $_} @{$accum{prefix}});
  $file .= "\nnamespace $p{namespace} {\n\n" if $p{cxx};
  if (defined $info{forward}{proc}{$p{type}}) {
    my @types = sort {$a->{name} cmp $b->{name}} (values %{$accum{types}});
    $file .= cmap {$info{forward}{proc}{$p{type}}->($_)} @types;
  }
  $file .= "\n";
  $file .= $body;
  $file .= join('', grep {defined $_} @{$accum{suffix}});
  $file .= "\n\n}\n\n" if $p{cxx};
  $file .= "#ifdef __cplusplus\n}\n#endif\n" if $p{header} && !$p{cxx};
  $file .= "#endif /* $hm */\n" if $p{header};
  create_file $p{dir}.'/'.to_lower($p{name}).$p{pre_ext}.'.'.$p{ext}, $file;
}

my $MKMK;

sub create_file ( $ $ ) {
  my ($filename, $to_write) = @_;
  unless (defined $MKMK) {
    open $MKMK, ">auto" or die;
  }
  local $/ = undef;
  my $existing = '';
  open F,"../$filename" and $existing=<F>;
  if ($to_write eq $existing) {
    print $MKMK "\$(srcdir)/$filename ";
    print "File \"$filename\" unchanged.\n";
  } elsif (length $existing > 0 && $existing !~ /Automatically generated file\./) {
    print "Will not write over \"$filename\".\n";
  } else {
    print $MKMK "\$(srcdir)/$filename ";
    print "Creating \"$filename\".\n";
    open F, ">../$filename" or die;
    print F $to_write;
  }
}

END {
  if (defined $MKMK) {
    print $MKMK ": \$(srcdir)/auto/auto ;\n";
    #print $MKMK "\ttouch \$@\n";
  }
}

1;
