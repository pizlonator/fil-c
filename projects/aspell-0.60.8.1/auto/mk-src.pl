#!/usr/bin/perl

#
# mk-src.pl -- Perl program to automatically generate interface code.
#
# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.

######################################################################
#
# Prologue
#

use strict;
use warnings;
no warnings qw(uninitialized);
no locale;

use Data::Dumper;

use MkSrc::Info;
use MkSrc::Read;
use MkSrc::Util;
use MkSrc::Type;
use MkSrc::Create;


######################################################################
#
# Main
#

my $master_data = read;

# Pure C
use MkSrc::ProcCc;
create_cc_file (type => 'cc',
		dir => 'interfaces/cc',
		name => 'aspell',
		header => true,
		data => $master_data);

# C++ over C
# Currently incomplete and broken
#use MkSrc::ProcCxx;
#create_cc_file (type => 'cxx',
#		cxx => true,
#		namespace => 'aspell',
#		dir => 'interfaces/cxx',
#		name => 'aspell-cxx',
#		header => true,
#		data => $master_data);

# Native
use MkSrc::ProcNative;
foreach my $d (@{$master_data->{data}}) {
  $info{group}{proc}{native}->($d);
}

# C for C++
use MkSrc::ProcImpl;
foreach my $d (@{$master_data->{data}}) {
  $info{group}{proc}{impl}->($d);
}

# Impl
use MkSrc::ProcNativeImpl;
foreach my $d (@{$master_data->{data}}) {
  $info{group}{proc}{native_impl}->($d);
}
