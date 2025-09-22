
use if $] < 5.006, Test::More => skip_all => 'syntax requires perl 5.6';

#tests behaviour on perls 5.10? .. 5.10.1
package Some::Fake::Packege;

sub fake_sub {
    require FileHandle;
}

package main;

use Test::More tests => 1;
use XML::Parser;
use strict;

my $count = 0;

my $parser = XML::Parser->new( ErrorContext => 2 );
$parser->setHandlers( Comment => sub { $count++; } );

open my $fh, '<', 'samples/REC-xml-19980210.xml' or die;

#on 5.10 $fh would be a FileHandle object without a real FileHandle class

$parser->parse($fh);

is( $count, 37 );
