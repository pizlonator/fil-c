# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

package MkSrc::Info;

BEGIN {
  use Exporter;
  our @ISA = qw(Exporter);
  our @EXPORT = qw(%info %types %methods);
}

use strict;
use warnings;
no warnings qw(uninitialized);
no locale;

=head1 MkSrc::Info

=head2 %info

The info array contains information on how to process the info in 
mk-src.pl.  It has the following layout

   <category> => options => [] 
                 groups => [] # if undef than anything is accepted
                 creates_type => "" # the object will create a new type
                                    # as specified
                 proc => <impl type> => sub {}

where <impl type> is one of:

  cc: for "aspell.h" header file
  cxx: for C++ interface implemented on top of cc interface
  native: for creation of header files used internally by aspell
  impl: for definition of functions declared in cc interface.
        the definitions use the native header files
  native_impl: for implementations of stuff declared in the native
                header files

each proc sub should take the following argv

   $data: a subtree of $master_data
   $accum: 

<options> is one of:

  desc: description of the object
  prefix:
  posib err: the method may return an error condition
  c func:
  const: the method is a const member
  c only: only include in the external interface
  c impl headers: extra headers that need to be included in the C impl
  c impl: use this as the c impl instead of the default
  cxx impl: use this as the cxx impl instead of the default
  returns alt type: the constructor returns some type other than
    the object from which it is a member of
  no native: do not attempt to create a native implementation
  treat as object: treat as an object rather than a pointer
  no conv: do not converted an encoded string

The %info structure is initialized as follows:

=cut

#pod
 our %info =
 (
  root => { 
    options => [],
    groups => ['methods', 'group']},
  methods => {
    # methods is a collection of methods which will be inserted into
    # a class after some simple substation rules.  A $ will be
    # replaced with name of the class.
    options => ['strip', 'prefix', 'c impl headers'],
    groups => undef},
  group => {
    # a group is a collection of objects which should be grouped together
    # this generally means they will be in the same source file
    options => ['no native'],
    groups => ['enum', 'struct', 'union', 'func', 'class', 'errors']},
  enum => {
    # basic C enum
    options => ['desc', 'prefix'],
    creates_type => 'enum'},
  struct => {
    # basic c struct
    options => ['desc', 'treat as object'],
    groups => undef,
    creates_type => 'struct',},
  union => {
    # basic C union
    options => ['desc', 'treat as object'],
    groups => undef,
    creates_type => 'union'},
  class => {
    # C++ class
    options => ['c impl headers'],
    groups => undef,
    creates_type => 'class'},
  errors => {}, # possible errors
  method => {
    # A class method
    options => ['desc', 'posib err', 'c func', 'const', 'no conv', 'on conv error',
		'c only', 'c impl', 'cxx impl', 'cc extra'],
    groups => undef},
  constructor => {
    # A class constructor
    options => ['returns alt type', 'c impl', 'desc'],
    groups => 'types'},
  destructor => {
    # A class destructor
    options => [],
    groups => undef},
  );
#cut

=pod

In addition to the categories listed above a "methods" category by
be specified in under the class category.  A "methods" category is
created for each methods group under the name "<methods name> methods"
When groups is undefined a type name may be specified in place of
a category

=head2 %types

types contains a master list of all types.  This includes basic types
and ones created in mk-src.in. The basic types include:

=cut

my @types = 
    (
#pod
     'void', 'bool', 'pointer', 'double',
     'string', 'encoded string', 'string obj',
     'char', 'unsigned char',
     'short', 'unsigned short',
     'int', 'unsigned int',
     'long', 'unsigned long'
#cut
     );

our %types;
use MkSrc::Type;
foreach my $t (@types) {
  update_type $t, {type=>'basic'}}

=head2 %methods

%methods is used for holding the "methods" information

=cut

our %methods;
    
1;
