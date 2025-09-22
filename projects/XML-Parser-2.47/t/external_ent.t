#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 4;
use XML::Parser;

################################################################
# Check default external entity handler

my $txt = '';

sub txt {
    my ( $xp, $data ) = @_;

    $txt .= $data;
}

my $docstring = <<'End_of_XML;';
<!DOCTYPE foo [
  <!ENTITY a SYSTEM "a.ent">
  <!ENTITY b SYSTEM "b.ent">
  <!ENTITY c SYSTEM "c.ent">
]>
<foo>
a = "&a;"
b = "&b;"


And here they are again in reverse order:
b = "&b;"
a = "&a;"

</foo>
End_of_XML;

my $ent_fh;
open( $ent_fh, '>', 'a.ent' ) or die "Couldn't open a.ent for writing";
print $ent_fh "This ('&c;') is a quote of c";
close($ent_fh);

open( $ent_fh, '>', 'b.ent' ) or die "Couldn't open b.ent for writing";
print $ent_fh "Hello, I'm B";
close($ent_fh);

open( $ent_fh, '>', 'c.ent' ) or die "Couldn't open c.ent for writing";
print $ent_fh "Hurrah for C";
close($ent_fh);

my $p = new XML::Parser( Handlers => { Char => \&txt } );

$p->parse($docstring);

my %check = (
    a => "This ('Hurrah for C') is a quote of c",
    b => "Hello, I'm B"
);

while ( $txt =~ /([ab]) = "(.*)"/g ) {
    my ( $k, $v ) = ( $1, $2 );

    is($check{$k}, $v);
}

unlink('a.ent');
unlink('b.ent');
unlink('c.ent');
