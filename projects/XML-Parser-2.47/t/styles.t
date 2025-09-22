use Test;
BEGIN { plan tests => 13 }
use XML::Parser;
use IO::File;

my $xmlstr = '<foo>bar</foo>';

{
    # Debug style
    my $parser = XML::Parser->new( Style => 'Debug' );
    ok($parser);

    my $tmpfile = IO::File->new_tmpfile();
    open( OLDERR, ">&STDERR" );
    open( STDERR, ">&" . $tmpfile->fileno ) || die "Cannot re-open STDERR : $!";

    $parser->parse($xmlstr);

    close(STDERR);
    open( STDERR, ">&OLDERR" );
    close(OLDERR);

    seek( $tmpfile, 0, 0 );
    my $warn = 0;
    $warn++ while (<$tmpfile>);
    ok( $warn, 3, "Check we got three warnings out" );
}

{
    # Object style
    my $parser = XML::Parser->new( Style => 'Objects' );
    ok($parser);

    my $tree = $parser->parse($xmlstr);
    ok($tree);
}

{
    # Stream style
    my $parser = XML::Parser->new( Style => 'Stream' );
    ok($parser);
}

{
    # Subs style
    my $parser = XML::Parser->new( Style => 'Subs' );
    ok($parser);
}

{
    # Tree style
    my $parser = XML::Parser->new( Style => 'Tree' );
    ok($parser);

    my $tree = $parser->parse($xmlstr);
    ok( ref($tree),             'ARRAY' );
    ok( $tree->[0],             'foo' );
    ok( ref( $tree->[1] ),      'ARRAY' );
    ok( ref( $tree->[1]->[0] ), 'HASH' );
    ok( $tree->[1][1],          '0' );
    ok( $tree->[1][2],          'bar' );
}
