BEGIN { print "1..4\n"; }
END { print "not ok 1\n" unless $loaded; }
use XML::Parser;
$loaded = 1;
print "ok 1\n";

$doc = <<'End_of_Doc;';
<!DOCTYPE foo [
<!ATTLIST bar zz CDATA 'there'>
]>
<foo>
  <bar xx="hello"/>
  <bar zz="other"/>
</foo>
End_of_Doc;

sub st {
    my $xp = shift;
    my $el = shift;

    if ( $el eq 'bar' ) {
        my %atts = @_;
        my %isdflt;
        my $specified = $xp->specified_attr;

        for ( my $i = $specified; $i < @_; $i += 2 ) {
            $isdflt{ $_[$i] } = 1;
        }

        if ( defined $atts{xx} ) {
            print 'not '
              if $isdflt{'xx'};
            print "ok 2\n";

            print 'not '
              unless $isdflt{'zz'};
            print "ok 3\n";
        }
        else {
            print 'not '
              if $isdflt{'zz'};
            print "ok 4\n";
        }

    }
}

$p = new XML::Parser( Handlers => { Start => \&st } );

$p->parse($doc);
