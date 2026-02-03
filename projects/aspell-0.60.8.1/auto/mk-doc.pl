#!/usr/bin/perl

my @files = qw(mk-src.in MkSrc/Info.pm 
	       MkSrc/Util.pm 
	       MkSrc/Read.pm MkSrc/Create.pm
	       MkSrc/CcHelper.pm);

my $final;

foreach (@files)
{
  open IN, $_;
  $final .= "\n\n### FILE: $_\n\n";
  while (<IN>) 
  {
    s/^\#pod\s*/\n\=pod\n\n/;
    s/^\#cut\s*/\n\=cut\n\n/;
    $final .= $_;
  }
  close IN;
}

open OUT, ">mk-src.pod" or die;
print OUT $final;
close OUT;

use Pod::Checker;
$parser = Pod::Checker->new();
$parser->parse_from_file('mk-src.pod', \*STDERR);

use Pod::Text;
$parser = Pod::Text->new(loose=>1);
$parser->parse_from_file('mk-src.pod', 'mk-src.txt');

#use Pod::LaTeX;
#$parser = Pod::LaTeX->new(AddPreamble => 0, AddPostamble => 0);
#$parser->parse_from_file('mk-src.pod', '../manual/mk-src.tex.new');

#$/ = undef;
#open IN, '../manual/mk-src.tex';
#$orig = <IN>;
#open IN, '../manual/mk-src.tex.new';
#$new = <IN>;
#close IN;
#if ($orig eq $new) {
#  print "mk-src.tex unchanged\n";
#} else {
#  rename '../manual/mk-src.tex.new', '../manual/mk-src.tex';
#}

#unlink "mk-src.pod";
