# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

package MkSrc::CcHelper;

BEGIN {
  use Exporter;
  our @ISA = qw(Exporter);
  our @EXPORT = qw(to_c_return_type c_error_cond
		   to_type_name make_desc make_func call_func get_c_func_name
		   make_c_method make_wide_macro call_c_method form_c_method
		   make_cxx_method);
}

use strict;
use warnings;
no warnings qw(uninitialized);
no locale;

use MkSrc::Util;
use MkSrc::Type;

sub to_type_name ( $ $ ; \% );

=head1 Code Generation Modes

The code generation modes are currently one of the following:

  cc: Mode used to create types suitable for C interface
  cc_cxx: Like cc but typenames don't have a leading Aspell prefix
  cxx: Mode used to create types suitable for CXX interface
  native: Mode in which types are suitable for the internal implementation
  native_no_err: Like Native but with out PosibErr return types

=head1 MkSrc::CcHelper

Helper functions used by interface generation code:

=over

=item to_c_return_type ITEM

.

=cut

sub to_c_return_type ( $ ) {
  my ($d) = @_;
  return $d->{type} unless exists $d->{'posib err'};
  return 'int' if one_of $d->{type}, ('void', 'bool', 'unsigned int');
  return $d->{type};
}

=item c_error_cond ITEM

.

=cut

sub c_error_cond ( $ ) {
  my ($d) = @_;
  die unless exists $d->{'posib err'};
  return '-1' if one_of $d->{type}, ('bool', 'unsigned int', 'int');
  return '0';
}

=item make_func NAME @TYPES PARMS ; %ACCUM

Creates a function prototype

Parms can be any of:

  mode: code generation mode

=cut

sub make_func ( $ \@ $ ; \% ) {
  my ($name, $d, $p, $accum) = @_;
  $accum = {} unless defined $accum;
  my @d = @$d;
  return (join '', 
	  (to_type_name(shift @d, {%$p,pos=>'return'}, %$accum),
	   ' ',
	   to_lower $name,
	   '(',
	   (join ', ', map {to_type_name $_, {%$p,pos=>'parm'}, %$accum} @d),
	   ')'));
}

=item make_wide_version NAME @TYPES PARMS ; %ACCUM

Creates the wide character version of the function if needed

=cut

sub make_wide_version ( $ \@ $ ; \% ) {
  my ($name, $d, $p, $accum) = @_;
  my @d = @$d;
  shift @d;
  return '' unless grep {$_->{type} eq 'encoded string'} @d;
  $accum->{sys_headers}{'stddef.h'} = true;
  $accum->{suffix}[5] = <<'---';

/******************* private implemantion details *********************/

#ifdef __cplusplus
#  define aspell_cast_(type, expr) (static_cast<type>(expr))
#  define aspell_cast_from_wide_(str) (static_cast<const void *>(str))
#else
#  define aspell_cast_(type, expr) ((type)(expr))
#  define aspell_cast_from_wide_(str) ((const char *)(str))
#endif
---
  my @parms = map {$_->{type} eq 'encoded string'
                       ? ($_->{name}, $_->{name}.'_size')
                       : $_->{name}} @d;
  $name = to_lower $name;
  $accum->{suffix}[0] = <<'---';
/**********************************************************************/

#ifdef ASPELL_ENCODE_SETTING_SECURE
---
  $accum->{suffix}[2] = "#endif\n";
  my @args = map  {$_->{type} eq 'encoded string'
                       ? ($_->{name}, "$_->{name}_size", '-1')
                       : $_->{name}} @d;
  $accum->{suffix}[1] .=
      (join '',
       "#define $name",
       '(', join(', ', @parms), ')',
       "\\\n    ",
       $name, '_wide',
       '(', join(', ', @args), ')',
       "\n");
  @args = map  {$_->{type} eq 'encoded string'
                    ? ("aspell_cast_from_wide_($_->{name})",
                       "$_->{name}_size*aspell_cast_(int,sizeof(*($_->{name})))",
                       "sizeof(*($_->{name}))")
                    : $_->{name}} @d;
  return (join '',
          "\n",
          "/* version of $name that is safe to use with (null terminated) wide characters */\n",
          '#define ',
          $name, '_w',
          '(', join(', ', @parms), ')', 
          "\\\n    ",
          $name, '_wide',
          '(', join(', ', @args), ')',
          "\n");
}


=item call_func NAME @TYPES PARMS ; %ACCUM

Return a string to call a func.  Will prefix the function with return
if the functions returns a non-void type;

Parms can be any of:

  mode: code generation mode

=cut

sub call_func ( $ \@ $ ; \% ) {
  my ($name, $d, $p, $accum) = @_;
  my @d = @$d;
  my $func_ret = to_type_name(shift @d, {%$p,pos=>'return'}, %$accum);
  return (join '',
	  (($func_ret eq 'void' ? '' : 'return '),
	   to_lower $name,
	   '(',
	   (join ', ', map {to_type_name $_, 
			    {%$p,pos=>'parm',use_type=>false}, %$accum} @d),
	   ')'));
}

=item to_type_name ITEM PARMS ; %ACCUM

Converts item into a type name.

Parms can be any of:

  mode: code generation mode
  use_type: include the actual type
  use_name: include the name on the type
  pos: either "return" or "other"

=cut

sub to_type_name ( $ $ ; \% ) {
  my ($d, $p, $accum) = @_;
  $accum = {} unless defined $accum;

  my $mode = $p->{mode};
  die unless one_of $mode, qw(cc cc_cxx cxx native native_no_err);
  my $is_cc = one_of $mode, qw(cc cc_cxx cxx);
  my $is_native = one_of $mode, qw(native native_no_err);

  my $pos  = $p->{pos};
  my $t = finalized_type($pos eq 'return' && $is_cc
			 ? to_c_return_type $d
			 : $d->{type});
  $p->{use_type} = true    unless exists $p->{use_type};
  $p->{use_name} = true    unless exists $p->{use_name};
  $p->{pos}      = 'other' unless exists $p->{pos};

  my $name = $t->{name};
  my $type = $t->{type};

  if ($name eq 'encoded string' && $is_cc && $pos eq 'parm') {
    my @types = ((to_type_name {%$d, type=>($p->{wide}?'const void pointer':'string')}, $p, %$accum),
                 (to_type_name {%$d, type=>'int', name=>"$d->{name}_size"}, $p, %$accum));
    push @types, (to_type_name {%$d, type=>'int', name=>"$d->{name}_type_width"}, $p, %$accum) if $p->{wide};
    return @types;
  }
  return ( (to_type_name {%$d, type=>($p->{wide}?'const void pointer':'string')}, $p, %$accum) ,
           (to_type_name {%$d, type=>'int', name=>"$d->{name}_size"}, $p, %$accum) )
      if $name eq 'encoded string' && $is_cc && $pos eq 'parm';

  my $str;

  if ($p->{use_type}) 
  {
    $str .= "const " if $t->{const};

    if ($name eq 'string') {
      if ($is_native && $pos eq 'parm') {
	$accum->{headers}{'parm string'} = true;
	$str .= "ParmString";
      } else {
	$str .= "const char *";
      }
    } elsif ($name eq 'string obj') {
      die unless $pos eq 'return';
      if ($is_cc) {
	$str .= "const char *";
      } else {
	$accum->{headers}{'string'} = true;
	$str .= "String";
      }
    } elsif ($name eq 'encoded string') {
      $str .= $p->{wide} ? "const void *" : "const char *";
    } elsif ($name eq '') {
      $str .= "void";
    } elsif ($name eq 'bool' && $is_cc) {
      $str .= "int";
    } elsif ($type eq 'basic') {
      $str .= $name;
    } elsif (one_of $type, qw(enum class struct union)) {
      my $c_type = $type eq 'class' ? 'struct' : $type;
      if ($t->{pointer}) {
	$accum->{types}->{$name} = $t;
      } else {
        $accum->{headers}->{$t->{created_in}} = true unless $mode eq 'cc';
      }
      $str .= "$c_type Aspell" if $mode eq 'cc';
      $str .= to_mixed($name);
    } else {
      print STDERR "Warning: Unknown Type: $name\n";
      $str .= "{unknown type: $name}";
    }

    if ($t->{pointer} && $type eq 'class' && $mode eq 'cxx') {
      $str .= "Ptr";
    } elsif ($t->{pointer}) {
      $str .= " *";
    }

  }

  if (defined $d->{name} && $p->{use_name})
  {
    $str .= " " unless $str eq '';
    $str .= to_lower($d->{name});
  }

  $str .= "[$t->{array}]" if $t->{array} && $p->{use_type};

  return $str;
}


=item make_desc DESC ; LEVEL

Make a C comment out of DESC optionally indenting it LEVEL spaces.

=cut

sub make_desc ( $ ; $ ) {
  my ($desc, $indent) = @_;
  return '' unless defined $desc;
  my @desc = split /\n/, $desc;
  $indent = 0 unless defined $indent;
  $indent = ' 'x$indent;
  return ("$indent/* ".
	  join("\n$indent * ", @desc).
	  " */\n");
}

=item make_c_method CLASS ITEM PARMS ; %ACCUM

Create the phototype for a C method which is really a function.

Parms is any of:

  mode: code generation mode
  no_aspell: if true do not include aspell in the name
  this_name: name for the parameter representing the current object

=item call_c_method CLASS ITEM PARMS ; %ACCUM

Like make_c_method but instead returns the appropriate string to call
the function.  If the function returns a non-void type the string will
be prefixed with a return statement.

=item form_c_method CLASS ITEM PARMS ; %ACCUM

Like make_c_method except that it returns the array:

  ($func, $data, $parms, $accum)

which is suitable for passing into make_func.  It will return an 
empty array if it can not make a method from ITEM.

=cut

sub form_c_method ($ $ $ ; \% ) 
{
  my ($class, $d, $p, $accum) = @_;
  $accum = {} unless defined $accum;
  my $mode = $p->{mode};
  my $this_name = defined $p->{this_name} ? $p->{this_name} : 'ths';
  my $name = $d->{name};
  my $func = '';
  my @data = ();
  @data = @{$d->{data}} if defined $d->{data};
  if ($d->{type} eq 'constructor') {
    if (defined $name) {
      $func = $name;
    } else {
      $func = "new aspell $class";
    }
    splice @data, 0, 0, {type => $class} unless exists $d->{'returns alt type'};
  } elsif ($d->{type} eq 'destructor') {
    $func = "delete aspell $class";
    splice @data, 0, 0, ({type => 'void'}, {type=>$class, name=>$this_name});
  } elsif ($d->{type} eq 'method') {
    if (exists $d->{'c func'}) {
      $func = $d->{'c func'};
    } elsif (exists $d->{'prefix'}) {
      $func = "$d->{prefix} $name";
    } else {
      $func = "aspell $class $name";
    }
    $func .= " wide" if $p->{wide};
    if (exists $d->{'const'}) {
      splice @data, 1, 0, {type => "const $class", name=> $this_name};
    } else {
      splice @data, 1, 0, {type => "$class", name=> $this_name};
    }
  } else {
    return ();
  }
  $func = "aspell $func" unless $func =~ /aspell/;
  $func =~ s/aspell\ ?// if exists $p->{no_aspell};
  return ($func, \@data, $p, $accum);
}

sub make_c_method ($ $ $ ; \%)
{
  my @ret = &form_c_method(@_);
  return undef unless @ret > 0;
  return &make_func(@ret);
}

sub get_c_func_name ($ $ $)
{
  my @ret = &form_c_method(@_);
  return undef unless @ret > 0;
  return to_lower $ret[0];
}

sub make_wide_macro ($ $ $ ; \%)
{
  my @ret = &form_c_method(@_);
  return undef unless @ret > 0;
  my $str = &make_wide_version(@ret);
  return $str;
}

sub call_c_method ($ $ $ ; \%)
{
  my @ret = &form_c_method(@_);
  return undef unless @ret > 0;
  return &call_func(@ret);
}

=item make_cxx_method ITEM PARMS ; %ACCUM

Create the phototype for a C++ method.

Parms is one of:

  mode: code generation mode

=cut

sub make_cxx_method ( $ $ ; \% ) {
  my ($d, $p, $accum) = @_;
  my $ret;
  $ret .= make_func $d->{name}, @{$d->{data}}, $p, %$accum;
  $ret .= " const" if exists $d->{const};
  return $ret;
}

=back

=cut


1;
