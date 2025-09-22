BEGIN { print "1..6\n"; }
END { print "not ok 1\n" unless $loaded; }
use XML::Parser;
$loaded = 1;
print "ok 1\n";

################################################################
# Check encoding

my $xmldec = "<?xml version='1.0' encoding='x-sjis-unicode' ?>\n";

my $docstring = <<"End_of_doc;";
<\x8e\x83>\x90\x46\x81\x41\x98\x61\x81\x41\x99\x44
</\x8e\x83>
End_of_doc;

my $doc = $xmldec . $docstring;

my @bytes;
my $lastel;

sub text {
    my ( $xp, $data ) = @_;

    push( @bytes, unpack( 'U0C*', $data ) );    # was fixed 5.10
}

sub start {
    my ( $xp, $el ) = @_;

    $lastel = $el;
}

my $p = XML::Parser->new( Handlers => { Start => \&start, Char => \&text } );

$p->parse($doc);

my $exptag = ( $] < 5.006 )
  ? "\xe7\xa5\x89"    # U+7949 blessings 0x8e83
  : chr(0x7949);

my @expected = (
    0xe8, 0x89, 0xb2,    # U+8272 beauty    0x9046
    0xe3, 0x80, 0x81,    # U+3001 comma     0x8141
    0xe5, 0x92, 0x8c,    # U+548C peace     0x9861
    0xe3, 0x80, 0x81,    # U+3001 comma     0x8141
    0xe5, 0x83, 0x96,    # U+50D6 joy       0x9944
    0x0a
);

if ( $lastel eq $exptag ) {
    print "ok 2\n";
}
else {
    print "not ok 2\n";
}

if ( @bytes != @expected ) {
    print "not ok 3\n";
}
else {
    my $i;
    for ( $i = 0; $i < @expected; $i++ ) {
        if ( $bytes[$i] != $expected[$i] ) {
            print "not ok 3\n";
            exit;
        }
    }
    print "ok 3\n";
}

$lastel = '';

$p->parse( $docstring, ProtocolEncoding => 'X-SJIS-UNICODE' );

if ( $lastel eq $exptag ) {
    print "ok 4\n";
}
else {
    print "not ok 4\n";
}

# Test the CP-1252 Win-Latin-1 mapping

$docstring = qq(<?xml version='1.0' encoding='WINDOWS-1252' ?>
<doc euro="\x80" lsq="\x91" rdq="\x94" />
);

my %attr;

sub get_attr {
    my ( $xp, $el, @list ) = @_;
    %attr = @list;
}

$p = XML::Parser->new( Handlers => { Start => \&get_attr } );

eval { $p->parse($docstring) };

if ($@) {
    print "not ";    # couldn't load the map
}
print "ok 5\n";

if (   $attr{euro} ne ( $] < 5.006 ? "\xE2\x82\xAC" : chr(0x20AC) )
    or $attr{lsq} ne ( $] < 5.006 ? "\xE2\x80\x98" : chr(0x2018) )
    or $attr{rdq} ne ( $] < 5.006 ? "\xE2\x80\x9D" : chr(0x201D) ) ) {
    print "not ";
}
print "ok 6\n";

