
use Test::More tests => 1;
use XML::Parser;

my $count = 0;

$parser = XML::Parser->new( ErrorContext => 2 );
$parser->setHandlers( Comment => sub { $count++; } );

$parser->parsefile('samples/REC-xml-19980210.xml');

is( $count, 37 );
