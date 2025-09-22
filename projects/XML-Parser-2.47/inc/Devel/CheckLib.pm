# $Id: CheckLib.pm,v 1.25 2008/10/27 12:16:23 drhyde Exp $

package Devel::CheckLib;

use 5.00405; #postfix foreach
use strict;
use vars qw($VERSION @ISA @EXPORT);
$VERSION = '1.16';
use Config qw(%Config);
use Text::ParseWords qw(quotewords shellwords);

use File::Spec;
use File::Temp;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(assert_lib check_lib_or_exit check_lib);

# localising prevents the warningness leaking out of this module
local $^W = 1;    # use warnings is a 5.6-ism

_findcc(); # bomb out early if there's no compiler

=head1 NAME

Devel::CheckLib - check that a library is available

=head1 DESCRIPTION

Devel::CheckLib is a perl module that checks whether a particular C
library and its headers are available.

=head1 SYNOPSIS

    use Devel::CheckLib;

    check_lib_or_exit( lib => 'jpeg', header => 'jpeglib.h' );
    check_lib_or_exit( lib => [ 'iconv', 'jpeg' ] );
  
    # or prompt for path to library and then do this:
    check_lib_or_exit( lib => 'jpeg', libpath => $additional_path );

=head1 USING IT IN Makefile.PL or Build.PL

If you want to use this from Makefile.PL or Build.PL, do
not simply copy the module into your distribution as this may cause
problems when PAUSE and search.cpan.org index the distro.  Instead, use
the use-devel-checklib script.

=head1 HOW IT WORKS

You pass named parameters to a function, describing to it how to build
and link to the libraries.

It works by trying to compile some code - which defaults to this:

    int main(int argc, char *argv[]) { return 0; }

and linking it to the specified libraries.  If something pops out the end
which looks executable, it gets executed, and if main() returns 0 we know
that it worked.  That tiny program is
built once for each library that you specify, and (without linking) once
for each header file.

If you want to check for the presence of particular functions in a
library, or even that those functions return particular results, then
you can pass your own function body for main() thus:

    check_lib_or_exit(
        function => 'foo();if(libversion() > 5) return 0; else return 1;'
        incpath  => ...
        libpath  => ...
        lib      => ...
        header   => ...
    );

In that case, it will fail to build if either foo() or libversion() don't
exist, and main() will return the wrong value if libversion()'s return
value isn't what you want.

=head1 FUNCTIONS

All of these take the same named parameters and are exported by default.
To avoid exporting them, C<use Devel::CheckLib ()>.

=head2 assert_lib

This takes several named parameters, all of which are optional, and dies
with an error message if any of the libraries listed can
not be found.  B<Note>: dying in a Makefile.PL or Build.PL may provoke
a 'FAIL' report from CPAN Testers' automated smoke testers.  Use 
C<check_lib_or_exit> instead.

The named parameters are:

=over

=item lib

Must be either a string with the name of a single 
library or a reference to an array of strings of library names.  Depending
on the compiler found, library names will be fed to the compiler either as
C<-l> arguments or as C<.lib> file names.  (E.g. C<-ljpeg> or C<jpeg.lib>)

=item libpath

a string or an array of strings
representing additional paths to search for libraries.

=item LIBS

a C<ExtUtils::MakeMaker>-style space-separated list of
libraries (each preceded by '-l') and directories (preceded by '-L').

This can also be supplied on the command-line.

=item debug

If true - emit information during processing that can be used for
debugging.

=back

And libraries are no use without header files, so ...

=over

=item header

Must be either a string with the name of a single 
header file or a reference to an array of strings of header file names.

=item incpath

a string or an array of strings
representing additional paths to search for headers.

=item INC

a C<ExtUtils::MakeMaker>-style space-separated list of
incpaths, each preceded by '-I'.

This can also be supplied on the command-line.

=item ccflags

Extra flags to pass to the compiler.

=item ldflags

Extra flags to pass to the linker.

=item analyze_binary

a callback function that will be invoked in order to perform custom
analysis of the generated binary. The callback arguments are the
library name and the path to the binary just compiled.

It is possible to use this callback, for instance, to inspect the
binary for further dependencies.

=item not_execute

Do not try to execute generated binary. Only check that compilation has not failed.

=back

=head2 check_lib_or_exit

This behaves exactly the same as C<assert_lib()> except that instead of
dieing, it warns (with exactly the same error message) and exits.
This is intended for use in Makefile.PL / Build.PL
when you might want to prompt the user for various paths and
things before checking that what they've told you is sane.

If any library or header is missing, it exits with an exit value of 0 to avoid
causing a CPAN Testers 'FAIL' report.  CPAN Testers should ignore this
result -- which is what you want if an external library dependency is not
available.

=head2 check_lib

This behaves exactly the same as C<assert_lib()> except that it is silent,
returning false instead of dieing, or true otherwise.

=cut

sub check_lib_or_exit {
    eval 'assert_lib(@_)';
    if($@) {
        warn $@;
        exit;
    }
}

sub check_lib {
    eval 'assert_lib(@_)';
    return $@ ? 0 : 1;
}

# borrowed from Text::ParseWords
sub _parse_line {
    my($delimiter, $keep, $line) = @_;
    my($word, @pieces);

    no warnings 'uninitialized';  # we will be testing undef strings

    while (length($line)) {
        # This pattern is optimised to be stack conservative on older perls.
        # Do not refactor without being careful and testing it on very long strings.
        # See Perl bug #42980 for an example of a stack busting input.
        $line =~ s/^
                    (?:
                        # double quoted string
                        (")                             # $quote
                        ((?>[^\\"]*(?:\\.[^\\"]*)*))"   # $quoted
        | # --OR--
                        # singe quoted string
                        (')                             # $quote
                        ((?>[^\\']*(?:\\.[^\\']*)*))'   # $quoted
                    |   # --OR--
                        # unquoted string
                        (                               # $unquoted
                            (?:\\.|[^\\"'])*?
                        )
                        # followed by
                        (                               # $delim
                            \Z(?!\n)                    # EOL
                        |   # --OR--
                            (?-x:$delimiter)            # delimiter
                        |   # --OR--
                            (?!^)(?=["'])               # a quote
                        )
        )//xs or return;    # extended layout
        my ($quote, $quoted, $unquoted, $delim) = (($1 ? ($1,$2) : ($3,$4)), $5, $6);

        return() unless( defined($quote) || length($unquoted) || length($delim));

        if ($keep) {
            $quoted = "$quote$quoted$quote";
        }
        else {
            $unquoted =~ s/\\(.)/$1/sg;
            if (defined $quote) {
                $quoted =~ s/\\(.)/$1/sg if ($quote eq '"');
            }
        }
        $word .= substr($line, 0, 0); # leave results tainted
        $word .= defined $quote ? $quoted : $unquoted;

        if (length($delim)) {
            push(@pieces, $word);
            push(@pieces, $delim) if ($keep eq 'delimiters');
            undef $word;
        }
        if (!length($line)) {
            push(@pieces, $word);
        }
    }
    return(@pieces);
}

sub _parsewords {
    return shellwords @_ if $^O ne 'MSWin32';
    # for Win32, take off "" but leave \
    map { my $s=$_; $s =~ s/^"(.*)"$/$1/; $s } grep defined && length, quotewords '\s+', 1, @_;
}

sub _compile_cmd {
    my ($Config_cc, $cc, $cfile, $exefile, $incpaths, $ld, $Config_libs, $lib, $libpaths) = @_;
    my @sys_cmd = @$cc;
    if ( $Config_cc eq 'cl' ) {                 # Microsoft compiler
	# this is horribly sensitive to the order of arguments
	push @sys_cmd,
	    $cfile,
	    (defined $lib ? "${lib}.lib" : ()),
	    "/Fe$exefile",
	    (map '/I'.$_, @$incpaths),
	    "/link",
	    @$ld,
	    _parsewords($Config_libs),
	    (defined $lib ? map '/libpath:'.$_, @$libpaths : ()),
	    ;
    } elsif($Config_cc =~ /bcc32(\.exe)?/) {    # Borland
	push @sys_cmd,
	    @$ld,
	    (map "-I$_", @$incpaths),
	    "-o$exefile",
	    (defined $lib ? ((map "-L$_", @$libpaths), "-l$lib") : ()),
	    $cfile,
	    ;
    } else { # Unix-ish: gcc, Sun, AIX (gcc, cc), ...
	push @sys_cmd,
	    (map "-I$_", @$incpaths),
	    $cfile,
	    (!defined $lib ? () : (
	      (map "-L$_", @$libpaths),
	      ($^O eq 'darwin' ? (map { "-Wl,-rpath,$_" } @$libpaths) : ()),
	      "-l$lib",
	    )),
	    @$ld,
	    "-o", $exefile,
	    ;
    }
    @sys_cmd;
}

sub _make_cfile {
    my ($use_headers, $function, $debug) = @_;
    my $code = '';
    $code .= qq{#include <$_>\n} for @$use_headers;
    $code .= "int main(int argc, char *argv[]) { ".($function || 'return 0;')." }\n";
    if ($debug) {
	(my $c = $code) =~ s:^:# :gm;
	warn "# Code:\n$c\n";
    }
    my ($ch, $cfile) = File::Temp::tempfile(
	'assertlibXXXXXXXX', SUFFIX => '.c'
    );
    print $ch $code;
    close $ch;
    (my $ofile = $cfile) =~ s/\.c$/$Config{_o}/;
    ($cfile, $ofile);
}

sub assert_lib {
    my %args = @_;
    $args{$_} = [$args{$_}]
        for grep $args{$_} && !ref($args{$_}), qw(lib libpath header incpath);
    my @libs = @{$args{lib} || []};
    my @libpaths = @{$args{libpath} || []};
    my @headers = @{$args{header} || []};
    my @incpaths = @{$args{incpath} || []};
    my $analyze_binary = $args{analyze_binary};
    my $execute = !$args{not_execute};

    my @argv = @ARGV;
    push @argv, _parse_line('\s+', 0, $ENV{PERL_MM_OPT}||'');

    # work-a-like for Makefile.PL's LIBS and INC arguments
    # if given as command-line argument, append to %args
    for my $arg (@argv) {
        for my $mm_attr_key (qw(LIBS INC)) {
            if (my ($mm_attr_value) = $arg =~ /\A $mm_attr_key = (.*)/x) {
            # it is tempting to put some \s* into the expression, but the
            # MM command-line parser only accepts LIBS etc. followed by =,
            # so we should not be any more lenient with whitespace than that
                $args{$mm_attr_key} .= " $mm_attr_value";
            }
        }
    }

    if(defined($args{LIBS})) {
        foreach my $arg (_parsewords($args{LIBS})) {
            die("LIBS argument badly-formed: $arg\n") unless($arg =~ /^-[lLR]/);
            push @{$arg =~ /^-l/ ? \@libs : \@libpaths}, substr($arg, 2);
        }
    }
    if(defined($args{INC})) {
        foreach my $arg (_parsewords($args{INC})) {
            die("INC argument badly-formed: $arg\n") unless($arg =~ /^-I/);
            push @incpaths, substr($arg, 2);
        }
    }

    my ($cc, $ld) = _findcc($args{debug}, $args{ccflags}, $args{ldflags});
    my @missing;
    my @wrongresult;
    my @wronganalysis;
    my @use_headers;

    # first figure out which headers we can't find ...
    for my $header (@headers) {
        push @use_headers, $header;
        my ($cfile, $ofile) = _make_cfile(\@use_headers, '', $args{debug});
        my $exefile = File::Temp::mktemp( 'assertlibXXXXXXXX' ) . $Config{_exe};
        my @sys_cmd = _compile_cmd($Config{cc}, $cc, $cfile, $exefile, \@incpaths, $ld, $Config{libs});
        warn "# @sys_cmd\n" if $args{debug};
        my $rv = $args{debug} ? system(@sys_cmd) : _quiet_system(@sys_cmd);
        push @missing, $header if $rv != 0 || ! -f $exefile;
        _cleanup_exe($exefile);
        unlink $cfile;
    }

    # now do each library in turn with headers
    my ($cfile, $ofile) = _make_cfile(\@use_headers, @args{qw(function debug)});
    for my $lib ( @libs ) {
        last if $Config{cc} eq 'CC/DECC';          # VMS
        my $exefile = File::Temp::mktemp( 'assertlibXXXXXXXX' ) . $Config{_exe};
        my @sys_cmd = _compile_cmd($Config{cc}, $cc, $cfile, $exefile, \@incpaths, $ld, $Config{libs}, $lib, \@libpaths);
        warn "# @sys_cmd\n" if $args{debug};
        local $ENV{LD_RUN_PATH} = join(":", grep $_, @libpaths, $ENV{LD_RUN_PATH}) unless $^O eq 'MSWin32' or $^O eq 'darwin';
        local $ENV{PATH} = join(";", @libpaths).";".$ENV{PATH} if $^O eq 'MSWin32';
        my $rv = $args{debug} ? system(@sys_cmd) : _quiet_system(@sys_cmd);
        if ($rv != 0 || ! -f $exefile) {
            push @missing, $lib;
        }
        else {
            chmod 0755, $exefile;
            my $absexefile = File::Spec->rel2abs($exefile);
            $absexefile = '"'.$absexefile.'"' if $absexefile =~ m/\s/;
            warn "# Execute($execute): $absexefile\n" if $args{debug};
            if ($execute) {
                my $retval = system($absexefile);
                warn "# return value: $retval\n" if $args{debug};
                push @wrongresult, $lib if $retval != 0;
            }
            push @wronganalysis, $lib
                if $analyze_binary and !$analyze_binary->($lib, $exefile);
        }
        _cleanup_exe($exefile);
    }
    unlink $cfile;

    my $miss_string = join( q{, }, map qq{'$_'}, @missing );
    die("Can't link/include C library $miss_string, aborting.\n") if @missing;
    my $wrong_string = join( q{, }, map qq{'$_'}, @wrongresult);
    die("wrong result: $wrong_string\n") if @wrongresult;
    my $analysis_string = join(q{, }, map qq{'$_'}, @wronganalysis );
    die("wrong analysis: $analysis_string") if @wronganalysis;
}

sub _cleanup_exe {
    my ($exefile) = @_;
    my $ofile = $exefile;
    $ofile =~ s/$Config{_exe}$/$Config{_o}/;
    # List of files to remove
    my @rmfiles;
    push @rmfiles, $exefile, $ofile, "$exefile\.manifest";
    if ( $Config{cc} eq 'cl' ) {
        # MSVC also creates foo.ilk and foo.pdb
        my $ilkfile = $exefile;
        $ilkfile =~ s/$Config{_exe}$/.ilk/;
        my $pdbfile = $exefile;
        $pdbfile =~ s/$Config{_exe}$/.pdb/;
	push @rmfiles, $ilkfile, $pdbfile;
    }
    foreach (grep -f, @rmfiles) {
	unlink $_ or warn "Could not remove $_: $!";
    }
    return
}

# return ($cc, $ld)
# where $cc is an array ref of compiler name, compiler flags
# where $ld is an array ref of linker flags
sub _findcc {
    my ($debug, $user_ccflags, $user_ldflags) = @_;
    # Need to use $keep=1 to work with MSWin32 backslashes and quotes
    my $Config_ccflags =  $Config{ccflags};  # use copy so ASPerl will compile
    $Config_ccflags =~ s:-O\S*::; # stop GCC optimising away test code
    my @Config_ldflags = ();
    for my $config_val ( @Config{qw(ldflags)} ){
        push @Config_ldflags, $config_val if ( $config_val =~ /\S/ );
    }
    my @ccflags = grep { length } _parsewords($Config_ccflags||'', $user_ccflags||'');
    my @ldflags = grep { length && $_ !~ m/^-Wl/ } _parsewords(@Config_ldflags, $user_ldflags||'');
    my @paths = split(/$Config{path_sep}/, $ENV{PATH});
    my @cc = _parsewords($Config{cc});
    if (check_compiler ($cc[0], $debug)) {
	return ( [ @cc, @ccflags ], \@ldflags );
    }
    # Find the extension for executables.
    my $exe = $Config{_exe};
    if ($^O eq 'cygwin') {
	$exe = '';
    }
    foreach my $path (@paths) {
	# Look for "$path/$cc[0].exe"
        my $compiler = File::Spec->catfile($path, $cc[0]) . $exe;
	if (check_compiler ($compiler, $debug)) {
	    return ([ $compiler, @cc[1 .. $#cc], @ccflags ], \@ldflags)
	}
        next if ! $exe;
	# Look for "$path/$cc[0]" without the .exe, if necessary.
        $compiler = File::Spec->catfile($path, $cc[0]);
	if (check_compiler ($compiler, $debug)) {
	    return ([ $compiler, @cc[1 .. $#cc], @ccflags ], \@ldflags)
	}
    }
    die("Couldn't find your C compiler.\n");
}

sub check_compiler
{
    my ($compiler, $debug) = @_;
    if (-f $compiler && -x $compiler) {
	warn "# Compiler seems to be $compiler\n" if $debug;
	return 1;
    }
    warn "# Compiler was not $compiler\n" if $debug;
    return '';
}


# code substantially borrowed from IPC::Run3
sub _quiet_system {
    my (@cmd) = @_;

    # save handles
    local *STDOUT_SAVE;
    local *STDERR_SAVE;
    open STDOUT_SAVE, ">&STDOUT" or die "CheckLib: $! saving STDOUT";
    open STDERR_SAVE, ">&STDERR" or die "CheckLib: $! saving STDERR";
    
    # redirect to nowhere
    local *DEV_NULL;
    open DEV_NULL, ">" . File::Spec->devnull 
        or die "CheckLib: $! opening handle to null device";
    open STDOUT, ">&" . fileno DEV_NULL
        or die "CheckLib: $! redirecting STDOUT to null handle";
    open STDERR, ">&" . fileno DEV_NULL
        or die "CheckLib: $! redirecting STDERR to null handle";

    # run system command
    my $rv = system(@cmd);

    # restore handles
    open STDOUT, ">&" . fileno STDOUT_SAVE
        or die "CheckLib: $! restoring STDOUT handle";
    open STDERR, ">&" . fileno STDERR_SAVE
        or die "CheckLib: $! restoring STDERR handle";

    return $rv;
}

=head1 PLATFORMS SUPPORTED

You must have a C compiler installed.  We check for C<$Config{cc}>,
both literally as it is in Config.pm and also in the $PATH.

It has been tested with varying degrees of rigorousness on:

=over

=item gcc (on Linux, *BSD, Mac OS X, Solaris, Cygwin)

=item Sun's compiler tools on Solaris

=item IBM's tools on AIX

=item SGI's tools on Irix 6.5

=item Microsoft's tools on Windows

=item MinGW on Windows (with Strawberry Perl)

=item Borland's tools on Windows

=item QNX

=back

=head1 WARNINGS, BUGS and FEEDBACK

This is a very early release intended primarily for feedback from
people who have discussed it.  The interface may change and it has
not been adequately tested.

Feedback is most welcome, including constructive criticism.
Bug reports should be made using L<http://rt.cpan.org/> or by email.

When submitting a bug report, please include the output from running:

    perl -V
    perl -MDevel::CheckLib -e0

=head1 SEE ALSO

L<Devel::CheckOS>

L<Probe::Perl>

=head1 AUTHORS

David Cantrell E<lt>david@cantrell.org.ukE<gt>

David Golden E<lt>dagolden@cpan.orgE<gt>

Yasuhiro Matsumoto E<lt>mattn@cpan.orgE<gt>

Thanks to the cpan-testers-discuss mailing list for prompting us to write it
in the first place;

to Chris Williams for help with Borland support;

to Tony Cook for help with Microsoft compiler command-line options

=head1 COPYRIGHT and LICENCE

Copyright 2007 David Cantrell. Portions copyright 2007 David Golden.

This module is free-as-in-speech software, and may be used, distributed,
and modified under the same conditions as perl itself.

=head1 CONSPIRACY

This module is also free-as-in-mason software.

=cut

1;
