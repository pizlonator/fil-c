use 5.004005;    #Devel::CheckLib
use ExtUtils::MakeMaker;
use lib qw(inc);
use Devel::CheckLib;

use Config;

$expat_libpath = $ENV{EXPATLIBPATH} || '';
$expat_incpath = $ENV{EXPATINCPATH} || '';

my @replacement_args;

foreach (@ARGV) {
    if (/^EXPAT(LIB|INC)PATH=(.+)/) {
        if ( $1 eq 'LIB' ) {
            $expat_libpath = $2;
        }
        else {
            $expat_incpath = $2;
        }

        #push(@replacement_args, "$1=$2");
    }
    else {
        push( @replacement_args, $_ );
    }
}

@ARGV = @replacement_args;

unless (
    check_lib(    # fill in what you prompted the user for here
        lib     => [qw(expat)],
        header  => ['expat.h'],
        incpath => $expat_incpath,
        ( $expat_libpath ? ( libpath => $expat_libpath ) : () ),
    )
) {
    warn <<'Expat_Not_Installed;';

Expat must be installed prior to building XML::Parser and I can't find
it in the standard library directories. Install 'expat-devel' (or
'libexpat1-dev') package with your OS package manager. See 'README'.

Or you can download expat from:

http://sourceforge.net/projects/expat/

If expat is installed, but in a non-standard directory, then use the
following options to Makefile.PL:

    EXPATLIBPATH=...  To set the directory in which to find libexpat

    EXPATINCPATH=...  To set the directory in which to find expat.h

For example:

    perl Makefile.PL EXPATLIBPATH=/home/me/lib EXPATINCPATH=/home/me/include

Note that if you build against a shareable library in a non-standard location
you may (on some platforms) also have to set your LD_LIBRARY_PATH environment
variable at run time for perl to find the library.

Expat_Not_Installed;

    # exiting before Makefile generation silences CPANTesters reports
    # when expat is not available.
    exit 0;
}

if ( not $expat_libpath and $] >= 5.006001 and $^O ne 'MSWin32' ) {
    require ExtUtils::Liblist;    # Buggy before this
    ($expat_libpath) = ExtUtils::Liblist->ext('-lexpat');
}

# Don't try to descend into Expat directory for testing

sub MY::test {
    my $self = shift;

    my $hold = delete $self->{DIR};
    my $ret  = $self->MM::test(@_);
    $self->{DIR} = $hold if defined($hold);
    $ret;
}

my @extras = ();

push(
    @extras,
    CAPI => 'TRUE'
  )
  if (  $PERL_VERSION >= 5.005
    and $OSNAME eq 'MSWin32'
    and $Config{archname} =~ /-object\b/i );

WriteMakefile1(
    ABSTRACT_FROM    => 'Parser.pm',
    AUTHOR           => 'Clark Cooper (coopercc@netheaven.com)',
    LICENSE          => 'perl',
    MIN_PERL_VERSION => '5.00405',
    META_MERGE       => {
        resources => {
            bugtracker => 'https://github.com/toddr/XML-Parser/issues',
            repository => 'http://github.com/toddr/XML-Parser',
        },
    },
    TEST_REQUIRES => {
        'Test::More' => 0,
        'warnings'   => 0,
    },

    NAME         => 'XML::Parser',
    DIR          => [qw(Expat)],
    dist         => { COMPRESS => 'gzip', SUFFIX => '.gz' },
    VERSION_FROM => 'Parser.pm',
    PREREQ_PM    => {
        'LWP::UserAgent' => 0,    #for tests
    },
    $^O =~ /win/i
    ? (
        dist => {
            TAR      => 'ptar',
            TARFLAGS => '-c -C -f',
        },
      )
    : (),
    @extras
);

sub WriteMakefile1 {    #Compatibility code for old versions of EU::MM. Written by Alexandr Ciornii, version 0.23. Added by eumm-upgrade.
    my %params       = @_;
    my $eumm_version = $ExtUtils::MakeMaker::VERSION;
    $eumm_version = eval $eumm_version;
    die "EXTRA_META is deprecated" if exists $params{EXTRA_META};
    die "License not specified" if not exists $params{LICENSE};
    if ( $params{AUTHOR} and ref( $params{AUTHOR} ) eq 'ARRAY' and $eumm_version < 6.5705 ) {
        $params{META_ADD}->{author} = $params{AUTHOR};
        $params{AUTHOR} = join( ', ', @{ $params{AUTHOR} } );
    }
    if ( $params{TEST_REQUIRES} and $eumm_version < 6.64 ) {
        $params{BUILD_REQUIRES} = { %{ $params{BUILD_REQUIRES} || {} }, %{ $params{TEST_REQUIRES} } };
        delete $params{TEST_REQUIRES};
    }
    if ( $params{BUILD_REQUIRES} and $eumm_version < 6.5503 ) {

        #EUMM 6.5502 has problems with BUILD_REQUIRES
        $params{PREREQ_PM} = { %{ $params{PREREQ_PM} || {} }, %{ $params{BUILD_REQUIRES} } };
        delete $params{BUILD_REQUIRES};
    }
    delete $params{CONFIGURE_REQUIRES} if $eumm_version < 6.52;
    delete $params{MIN_PERL_VERSION}   if $eumm_version < 6.48;
    delete $params{META_MERGE}         if $eumm_version < 6.46;
    delete $params{META_ADD}           if $eumm_version < 6.46;
    delete $params{LICENSE}            if $eumm_version < 6.31;

    WriteMakefile(%params);
}

