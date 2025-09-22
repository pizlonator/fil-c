BEGIN { print "1..3\n"; }
END { print "not ok 1\n" unless $loaded; }
use XML::Parser;
$loaded = 1;
print "ok 1\n";

my $stcount = 0;
my $encount = 0;

sub st {
    my ( $exp, $el ) = @_;
    $stcount++;
    $exp->finish if $el eq 'loc';
}

sub end {
    $encount++;
}

$parser = new XML::Parser(
    Handlers => {
        Start => \&st,
        End   => \&end
    },
    ErrorContext => 2
);

$parser->parsefile('samples/REC-xml-19980210.xml');

print "not " unless $stcount == 12;
print "ok 2\n";

print "not " unless $encount == 8;
print "ok 3\n";
