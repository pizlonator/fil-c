BEGIN { print "1..16\n"; }
END { print "not ok 1\n" unless $loaded; }
use XML::Parser;
$loaded = 1;
print "ok 1\n";

################################################################
# Check namespaces

$docstring = <<'End_of_doc;';
<foo xmlns="urn:blazing-saddles"
     xmlns:bar="urn:young-frankenstein"
     bar:alpha="17">
 <zebra xyz="nothing"/>
 <tango xmlns=""
	xmlns:zoo="urn:high-anxiety"
        beta="blue"
        zoo:beta="green"
        bar:beta="red">
   <?nscheck?>
   <zoo:here/>
   <there/>
 </tango>
 <everywhere/>
</foo>
End_of_doc;

my $gname;

sub init {
    my $xp = shift;
    $gname = $xp->generate_ns_name( 'alpha', 'urn:young-frankenstein' );
}

sub start {
    my $xp = shift;
    my $el = shift;

    if ( $el eq 'foo' ) {
        print "not " unless $xp->namespace($el) eq 'urn:blazing-saddles';
        print "ok 2\n";

        print "not " unless $xp->new_ns_prefixes == 2;
        print "ok 3\n";

        while (@_) {
            my $att = shift;
            my $val = shift;
            if ( $att eq 'alpha' ) {
                print "not " unless $xp->eq_name( $gname, $att );
                print "ok 4\n";
                last;
            }
        }
    }
    elsif ( $el eq 'zebra' ) {
        print "not " unless $xp->new_ns_prefixes == 0;
        print "ok 5\n";

        print "not " unless $xp->namespace($el) eq 'urn:blazing-saddles';
        print "ok 6\n";
    }
    elsif ( $el eq 'tango' ) {
        print "not " if $xp->namespace( $_[0] );
        print "ok 8\n";

        print "not " unless $_[0] eq $_[2];
        print "ok 9\n";

        print "not " if $xp->eq_name( $_[0], $_[2] );
        print "ok 10\n";

        my $cnt = 0;
        foreach ( $xp->new_ns_prefixes ) {
            $cnt++ if $_ eq '#default';
            $cnt++ if $_ eq 'zoo';
        }

        print "not " unless $cnt == 2;
        print "ok 11\n";
    }
}

sub end {
    my $xp = shift;
    my $el = shift;

    if ( $el eq 'zebra' ) {
        print "not "
          unless $xp->expand_ns_prefix('#default') eq 'urn:blazing-saddles';
        print "ok 7\n";
    }
    elsif ( $el eq 'everywhere' ) {
        print "not " unless $xp->namespace($el) eq 'urn:blazing-saddles';
        print "ok 16\n";
    }
}

sub proc {
    my $xp     = shift;
    my $target = shift;

    if ( $target eq 'nscheck' ) {
        print "not " if $xp->new_ns_prefixes > 0;
        print "ok 12\n";

        my $cnt = 0;
        foreach ( $xp->current_ns_prefixes ) {
            $cnt++ if $_ eq 'zoo';
            $cnt++ if $_ eq 'bar';
        }

        print "not " unless $cnt == 2;
        print "ok 13\n";

        print "not "
          unless $xp->expand_ns_prefix('bar') eq 'urn:young-frankenstein';
        print "ok 14\n";

        print "not "
          unless $xp->expand_ns_prefix('zoo') eq 'urn:high-anxiety';
        print "ok 15\n";
    }
}

my $parser = new XML::Parser(
    ErrorContext => 2,
    Namespaces   => 1,
    Handlers     => {
        Start => \&start,
        End   => \&end,
        Proc  => \&proc,
        Init  => \&init
    }
);

$parser->parse($docstring);
