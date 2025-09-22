#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 40;
use XML::Parser;

ok("loaded");

my $bigval = <<'End_of_bigval;';
This is a large string value to test whether the declaration parser still
works when the entity or attribute default value may be broken into multiple
calls to the default handler.
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
01234567890123456789012345678901234567890123456789012345678901234567890123456789
End_of_bigval;

$bigval =~ s/\n/ /g;

my $docstr = <<"End_of_Doc;";
<?xml version="1.0" encoding="ISO-8859-1" ?>
<!DOCTYPE foo SYSTEM 't/foo.dtd'
  [
   <!ENTITY alpha 'a'>
   <!ELEMENT junk ((bar|foo|xyz+), zebra*)>
   <!ELEMENT xyz (#PCDATA)>
   <!ELEMENT zebra (#PCDATA|em|strong)*>	
   <!ATTLIST junk
         id ID #REQUIRED
         version CDATA #FIXED '1.0'
         color (red|green|blue) 'green'
         foo NOTATION (x|y|z) #IMPLIED>
   <!ENTITY skunk "stinky animal">
   <!ENTITY big "$bigval">
   <!-- a comment -->
   <!NOTATION gif SYSTEM 'http://www.somebody.com/specs/GIF31.TXT'>
   <!ENTITY logo PUBLIC '//Widgets Corp/Logo' 'logo.gif' NDATA gif>
   <?DWIM a useless processing instruction ?>
   <!ELEMENT bar ANY>
   <!ATTLIST bar big CDATA '$bigval'>
  ]>
<foo/>
End_of_Doc;

my $entcnt = 0;
my %ents;

sub enth1 {
    my ( $p, $name, $val, $sys, $pub, $notation ) = @_;

    is( $val, 'a' )             if ( $name eq 'alpha' );
    is( $val, 'stinky animal' ) if ( $name eq 'skunk' );
    if ( $name eq 'logo' ) {
        ok( !defined($val) );
        is( $sys,      'logo.gif' );
        is( $pub,      '//Widgets Corp/Logo' );
        is( $notation, 'gif' );
    }
}

my $parser = new XML::Parser(
    ErrorContext  => 2,
    NoLWP         => 1,
    ParseParamEnt => 1,
    Handlers      => { Entity => \&enth1 }
);

eval { $parser->parse($docstr) };

sub eleh {
    my ( $p, $name, $model ) = @_;

    if ( $name eq 'junk' ) {
        is( $model, '((bar|foo|xyz+),zebra*)' );
        ok $model->isseq;
        my @parts = $model->children;
        ok( $parts[0]->ischoice );
        my @cparts = $parts[0]->children;
        is( $cparts[0],       'bar' );
        is( $cparts[1],       'foo' );
        is( $cparts[2],       'xyz+' );
        is( $cparts[2]->name, 'xyz' );
        is( $parts[1]->name,  'zebra' );
        is( $parts[1]->quant, '*' );
    }

    if ( $name eq 'xyz' ) {
        ok( $model->ismixed );
        ok( !defined( $model->children ) );
    }

    if ( $name eq 'zebra' ) {
        ok( $model->ismixed );
        is( ( $model->children )[1], 'strong' );
    }

    if ( $name eq 'bar' ) {
        ok( $model->isany );
    }
}

sub enth2 {
    my ( $p, $name, $val, $sys, $pub, $notation ) = @_;

    is( $val, 'a' )             if ( $name eq 'alpha' );
    is( $val, 'stinky animal' ) if ( $name eq 'skunk' );
    is( $val, $bigval )         if ( $name eq 'big' );
    ok( !defined($val) and $sys eq 'logo.gif' and $pub eq '//Widgets Corp/Logo' and $notation eq 'gif' )
      if ( $name eq 'logo' );
}

sub doc {
    my ( $p, $name, $sys, $pub, $intdecl ) = @_;

    is( $name, 'foo' );
    is( $sys,  't/foo.dtd' );
    ok($intdecl);
}

sub att {
    my ( $p, $elname, $attname, $type, $default, $fixed ) = @_;

    if ( $elname eq 'junk' ) {
        if ( $attname eq 'id' and $type eq 'ID' ) {
            is( $default, '#REQUIRED' );
            ok( !$fixed );
        }
        elsif ( $attname eq 'version' and $type eq 'CDATA' ) {
            is( $default, "'1.0'" );
            ok($fixed);
        }
        elsif ( $attname eq 'color' and $type eq '(red|green|blue)' ) {
            is( $default, "'green'" );
        }
        elsif ( $attname eq 'foo' and $type eq 'NOTATION(x|y|z)' ) {
            is( $default, '#IMPLIED' );
        }
    }
    elsif ( $elname eq 'bar' ) {
        is( $attname, 'big' );
        is( $default, "'$bigval'" );
    }
}

sub xd {
    my ( $p, $version, $enc, $stand ) = @_;

    if ( defined($version) ) {
        is( $version, '1.0' );
        is( $enc,     'ISO-8859-1' );
        ok( !defined($stand) );
    }
    else {
        is( $enc, 'x-sjis-unicode' );
    }
}

$parser->setHandlers(
    Entity  => \&enth2,
    Element => \&eleh,
    Attlist => \&att,
    Doctype => \&doc,
    XMLDecl => \&xd
);

$| = 1;
$parser->parse($docstr);

