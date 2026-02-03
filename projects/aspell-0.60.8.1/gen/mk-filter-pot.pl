sub prep_str($)
{
  local $_ = $_[0];
  s/\\(.)/$1/g;
  s/([\"\\])/\\$1/g;
  return $_;
}

open OUT, ">gen/filter.pot";

print OUT <<'---';
#. TRANSLATORS: Like the strings in config.cpp, all strings in *-filter.opt
#. should be under 50 characters, begin with a lower case character and 
#. not include any trailing punctuation marks.
---

@files = (<modules/filter/*-filter.info>, <modules/filter/modes/*.amf>);

foreach $f (@files)
{
  open IN, $f;
  while (<IN>) 
  {
    next unless /^\s*(description|desc)\s+(.+?)\s*$/i;
    $_ = prep_str($2);
    print OUT "\#: $f:$.\n";
    print OUT "msgid \"$_\"\n";
    print OUT "msgstr \"\"\n";
    print OUT "\n";
  }
  close IN;
}

close OUT;
