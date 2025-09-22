BEGIN { print "1..3\n"; }
END { print "not ok 1\n" unless $loaded; }
use XML::Parser;
$loaded = 1;
print "ok 1\n";

my $delim   = '------------123453As23lkjlklz877';
my $file    = 'samples/REC-xml-19980210.xml';
my $tmpfile = 'stream.tmp';

my $cnt = 0;

open( my $out_fh, '>', $tmpfile ) or die "Couldn't open $tmpfile for output";
open( my $in_fh,  '<', $file )    or die "Couldn't open $file for input";

while (<$in_fh>) {
    print $out_fh $_;
}

close($in_fh);
print $out_fh "$delim\n";

open( $in_fh, $file );
while (<$in_fh>) {
    print $out_fh $_;
}

close($in_fh);
close($out_fh);

my $parser = new XML::Parser(
    Stream_Delimiter => $delim,
    Handlers         => {
        Comment => sub { $cnt++; }
    }
);

open( my $fh, $tmpfile );

$parser->parse($fh);

print "not " if ( $cnt != 37 );
print "ok 2\n";

$cnt = 0;

$parser->parse($fh);

print "not " if ( $cnt != 37 );
print "ok 3\n";

close($fh);
unlink($tmpfile);
