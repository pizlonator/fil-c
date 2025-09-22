BEGIN { print "1..3\n"; }
END { print "not ok 1\n" unless $loaded; }
use XML::Parser;
$loaded = 1;
print "ok 1\n";

my $cnt = 0;
my $str;

sub tmpchar {
    my ( $xp, $data ) = @_;

    if ( $xp->current_element eq 'day' ) {
        $str = $xp->original_string;
        $xp->setHandlers( Char => 0 );
    }
}

my $p = new XML::Parser(
    Handlers => {
        Comment => sub { $cnt++; },
        Char => \&tmpchar
    }
);

my $xpnb = $p->parse_start;

open( my $rec, '<', 'samples/REC-xml-19980210.xml' );

while (<$rec>) {
    $xpnb->parse_more($_);
}

close($rec);

$xpnb->parse_done;

print "not " unless $cnt == 37;
print "ok 2\n";

print "not " unless $str eq '&draft.day;';
print "ok 3\n";

