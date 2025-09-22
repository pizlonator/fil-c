# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { print "1..27\n"; }
END { print "not ok 1\n" unless $loaded; }
use XML::Parser;
use FileHandle;    # Make 5.10.0 happy.
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

# Test 2

my $parser = new XML::Parser( ProtocolEncoding => 'ISO-8859-1' );
if ($parser) {
    print "ok 2\n";
}
else {
    print "not ok 2\n";
    exit;
}

my @ndxstack;
my $indexok = 1;

# Need this external entity

open( ZOE, '>zoe.ent' );
print ZOE "'cute'";
close(ZOE);

# XML string for tests

my $xmlstring = <<"End_of_XML;";
<!DOCTYPE foo
  [
    <!NOTATION bar PUBLIC "qrs">
    <!ENTITY zinger PUBLIC "xyz" "abc" NDATA bar>
    <!ENTITY fran SYSTEM "fran-def">
    <!ENTITY zoe  SYSTEM "zoe.ent">
   ]>
<foo>
  First line in foo
  <boom>Fran is &fran; and Zoe is &zoe;</boom>
  <bar id="jack" stomp="jill">
  <?line-noise *&*&^&<< ?>
    1st line in bar
    <blah> 2nd line in bar </blah>
    3rd line in bar <!-- Isn't this a doozy -->
  </bar>
  <zap ref="zing" />
  This, '\240', would be a bad character in UTF-8.
</foo>
End_of_XML;

# Handlers
my @tests;
my $pos = '';

sub ch {
    my ( $p, $str ) = @_;
    $tests[4]++;
    $tests[5]++ if ( $str =~ /2nd line/ and $p->in_element('blah') );
    if ( $p->in_element('boom') ) {
        $tests[17]++ if $str =~ /pretty/;
        $tests[18]++ if $str =~ /cute/;
    }
}

sub st {
    my ( $p, $el, %atts ) = @_;

    $ndxstack[ $p->depth ] = $p->element_index;
    $tests[6]++ if ( $el eq 'bar' and $atts{stomp} eq 'jill' );
    if ( $el eq 'zap' and $atts{'ref'} eq 'zing' ) {
        $tests[7]++;
        $p->default_current;
    }
    elsif ( $el eq 'bar' ) {
        $tests[22]++ if $p->recognized_string eq '<bar id="jack" stomp="jill">';
    }
}

sub eh {
    my ( $p, $el ) = @_;
    $indexok = 0 unless $p->element_index == $ndxstack[ $p->depth ];
    if ( $el eq 'zap' ) {
        $tests[8]++;
        my @old = $p->setHandlers( 'Char', \&newch );
        $tests[19]++ if $p->current_line == 17;
        $tests[20]++ if $p->current_column == 20;
        $tests[23]++ if ( $old[0] eq 'Char' and $old[1] == \&ch );
    }
    if ( $el eq 'boom' ) {
        $p->setHandlers( 'Default', \&dh );
    }
}

sub dh {
    my ( $p, $str ) = @_;
    if ( $str =~ /doozy/ ) {
        $tests[9]++;
        $pos = $p->position_in_context(1);
    }
    $tests[10]++ if $str =~ /^<zap/;
}

sub pi {
    my ( $p, $tar, $data ) = @_;

    $tests[11]++ if ( $tar eq 'line-noise' and $data =~ /&\^&<</ );
}

sub note {
    my ( $p, $name, $base, $sysid, $pubid ) = @_;

    $tests[12]++ if ( $name eq 'bar' and $pubid eq 'qrs' );
}

sub unp {
    my ( $p, $name, $base, $sysid, $pubid, $notation ) = @_;

    $tests[13]++ if ( $name eq 'zinger'
        and $pubid eq 'xyz'
        and $sysid eq 'abc'
        and $notation eq 'bar' );
}

sub newch {
    my ( $p, $str ) = @_;

    if ( $] < 5.007001 ) {
        $tests[14]++ if $str =~ /'\302\240'/;
    }
    else {
        $tests[14]++ if $str =~ /'\xa0'/;
    }
}

sub extent {
    my ( $p, $base, $sys, $pub ) = @_;

    if ( $sys eq 'fran-def' ) {
        $tests[15]++;
        return 'pretty';
    }
    elsif ( $sys eq 'zoe.ent' ) {
        $tests[16]++;

        open( FOO, $sys ) or die "Couldn't open $sys";
        return *FOO;
    }
}

eval {
    $parser->setHandlers(
        'Char'         => \&ch,
        'Start'        => \&st,
        'End'          => \&eh,
        'Proc'         => \&pi,
        'Notation'     => \&note,
        'Unparsed'     => \&unp,
        'ExternEnt'    => \&extent,
        'ExternEntFin' => sub { close(FOO); }
    );
};

if ($@) {
    print "not ok 3\n";
    exit;
}

print "ok 3\n";

# Test 4..20
eval { $parser->parsestring($xmlstring); };

if ($@) {
    print "Parse error:\n$@";
}
else {
    $tests[21]++;
}

unlink('zoe.ent') if ( -f 'zoe.ent' );

for ( 4 .. 23 ) {
    print "not " unless $tests[$_];
    print "ok $_\n";
}

$cmpstr = << 'End_of_Cmp;';
    <blah> 2nd line in bar </blah>
    3rd line in bar <!-- Isn't this a doozy -->
===================^
  </bar>
End_of_Cmp;

if ( $cmpstr ne $pos ) {
    print "not ";
}
print "ok 24\n";

print "not " unless $indexok;
print "ok 25\n";

# Test that memory leak through autovivifying symbol table entries is fixed.

my $count = 0;
$parser = new XML::Parser(
    Handlers => {
        Start => sub { $count++ }
    }
);

$xmlstring = '<a><b>Sea</b></a>';

eval { $parser->parsestring($xmlstring); };

if ( $count != 2 ) {
    print "not ";
}
print "ok 26\n";

if ( defined( *{$xmlstring} ) ) {
    print "not ";
}
print "ok 27\n";

