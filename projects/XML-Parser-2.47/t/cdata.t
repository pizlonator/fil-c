BEGIN { print "1..2\n"; }
END { print "not ok 1\n" unless $loaded; }
use XML::Parser;
$loaded = 1;
print "ok 1\n";

my $count = 0;

my $cdata_part = "<<< & > '' << &&&>&&&&;<";

my $doc = "<foo> hello <![CDATA[$cdata_part]]> there</foo>";

my $acc = '';

sub ch {
    my ( $xp, $data ) = @_;

    $acc .= $data;
}

sub stcd {
    my $xp = shift;
    $xp->setHandlers( Char => \&ch );
}

sub ecd {
    my $xp = shift;
    $xp->setHandlers( Char => 0 );
}

$parser = new XML::Parser(
    ErrorContext => 2,
    Handlers     => {
        CdataStart => \&stcd,
        CdataEnd   => \&ecd
    }
);

$parser->parse($doc);

print "not "
  unless ( $acc eq $cdata_part );
print "ok 2\n";

