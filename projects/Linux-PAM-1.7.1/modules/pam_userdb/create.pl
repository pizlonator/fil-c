#!/usr/bin/perl
# this program creates a database in ARGV[1] from pairs given on
# stdandard input
#
# $Id$

use DB_File;

my $database = $ARGV[0];
die "Use: create.pl <database>\n" unless ($database);
print "Using database: $database\n";

my %lusers = ();

tie %lusers, 'DB_File', $database, O_RDWR|O_CREAT, 0644, $DB_HASH ;
while (<STDIN>) {
  my ($user, $pass) = split;

  $lusers{$user} = $pass;
}
untie %lusers;
