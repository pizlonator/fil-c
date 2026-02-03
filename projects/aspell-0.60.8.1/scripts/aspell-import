#!/usr/bin/perl

#
# aspell-import -- Perl script to import old dictionaries
#
# This file is part of The New Aspell
# Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
# license version 2.0 or 2.1.  You should have received a copy of the
# LGPL license along with this library if you did not you can find it
# at http://www.gnu.org/.



%abrv = qw( american     en
	    british      en
	    canadian     en
	    catala       ca
	    czech        cs
	    dansk        da
	    deutsch      de
	    ellhnika     el
	    english      en
	    espanol      es
	    esperanto    eo
	    francais     fr
	    german       de 
	    italian      it
	    liet         lt
	    nederlands   nl
	    norsk        no
	    polish       pl
	    portugues    pt
	    russian      ru
	    svenska      sv);

chdir $ENV{HOME};

foreach $file (<.ispell_*>, <.aspell.*.*>)
{
  $_ = $file;
  if    (/^.ispell_(.+)$/)            {$lang = $1; $type = 'ispell'}
  elsif (/^.aspell.(.+?).(per|pws)$/) {$lang = $1; $type = 'personal'}
  elsif (/^.aspell.(.+?).(prepl)$/)   {$lang = $1; $type = 'repl'}
  $abrv = $abrv{$lang};
  if (not defined $abrv) {
    print "Warning language \"$lang\" is not known\n" unless length $lang == 2;
    next;
  }
  open IN, $file;
  print "Processing \"~/$file\", lang = $abrv\n";
  if ($type eq 'ispell' || $type eq 'personal') {
    <IN> if $type eq 'personal';
    while (<IN>) {
      chop; 
      push @{$words{$abrv}{per}}, $_;
    }
  } elsif ($type eq 'repl') {
    $_ = <IN>;
    if (!/^personal\_repl\-1\.1/) {
      print "$file not in a supported format\n";
      next;
    }
    while (<IN>) {
      /^([^ ]+) (.+)\n$/ or die;
      push @{$words{$abrv}{repl}}, [$1,$2];
    }
  }
  close IN;
}

$SIG{PIPE} = 'IGNORE';

foreach $abrv (keys %words) {
  print "Merging $abrv\n";
  open P, "| aspell -a --lang=$abrv --sug-mode=ultra" or next;
  foreach (@{$words{$abrv}{per}}) {
    print P "* $_\n";
  }
  foreach (@{$words{$abrv}{repl}}) {
    print P "\$\$ra $_->[0],$_->[1]\n";
  }
  print P "#\n";
  close P;
}
