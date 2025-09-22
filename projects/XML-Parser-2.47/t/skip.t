BEGIN { print "1..4\n"; }
END { print "not ok 1\n" unless $loaded; }
use XML::Parser;
$loaded = 1;
print "ok 1\n";

my $cmnt_count    = 0;
my $pi_count      = 0;
my $between_count = 0;
my $authseen      = 0;

sub init {
    my $xp = shift;
    $xp->skip_until(1);    # Skip through prolog
}

sub proc {
    $pi_count++;
}

sub cmnt {
    $cmnt_count++;
}

sub start {
    my ( $xp, $el ) = @_;
    my $ndx = $xp->element_index;
    if ( !$authseen and $el eq 'authlist' ) {
        $authseen = 1;
        $xp->skip_until(2000);
    }
    elsif ( $authseen and $ndx < 2000 ) {
        $between_count++;
    }
}

my $p = new XML::Parser(
    Handlers => {
        Init    => \&init,
        Start   => \&start,
        Comment => \&cmnt,
        Proc    => \&proc
    }
);

$p->parsefile('samples/REC-xml-19980210.xml');

print "not " if $between_count;
print "ok 2\n";

print "not " if $pi_count;
print "ok 3\n";

print "not " unless $cmnt_count == 5;
print "ok 4\n";

