#
#unmask dots and mask quotation marks and backslashes
sub prep_str($)
{
  local $_ = $_[0];
  s/\\(.)/$1/g;
  s/([\"\\])/\\$1/g;
  return $_;
}

#holds the description of all recognized filters
%filters=();

while ($filename=shift) {

#check if filter description file (*-filter.info) has been read before  
  $filtername=$filename;
  $filtername=~s/-filter\.info$//;
  $filtername=~s/[^\/]*\///g;
  ( exists $filters{$filtername}) &&
   (printf STDERR "filter allready defined $filtername($filename); ignored;\n") && next;
  ( open OPTIONFILE,"<$filename") || 
   (printf STDERR "can't open `$filename'; ignored;\n") && next;

#create basic structure holding description of filter types and options
  $filter=$filters{$filtername}={};
  ${$filter}{"NAME"}=$filtername;
  ${$filter}{"DECODER"}="0";
  ${$filter}{"FILTER"}="0";
  ${$filter}{"ENCODER"}="0";
  ${$filter}{"DESCRIPTION"}="";
  $inoption=0;

#parse file and fill filter structure
  while (<OPTIONFILE>) {
    chomp;

#ignore comments and empty lines; strip leading and trailing whitespaces
    (($_=~/^\#/) || ($_=~/^[ \t]*$/)) && next;
    $_=~s/[ \t]+$//;
    $_=~s/^[ \t]+//;
#check if forced end of file
    ($_=~s/^ENDFILE(?:[ \t]+|$)//i) && last;

#if line does not belong to block, describing a filter option, check if it
#provides basic information upon filter or if it starts an option block
    unless ($inoption) {
      ( $_=~s/^DES(?:CRIPTION)[ \t]+//i) && (($_=~s/\\(?=[ \t])//g) || 1) &&
       (${$filter}{"DESCRIPTION"}=$_) && next;
      ( $_=~s/^STATIC[ \t]+//i) && (($feature=uc $_ ) || 1) &&
       (${$filter}{$feature}="new_aspell_".$filtername."_".(lc $_)) && next;
      ( $_=~s/^ASPELL[ \t]+//i) && next;
      ( $_=~/^LIB-FILE/i) && next;
      ( $_=~/^OPTION[ \t]+/i) || 
       (die "Invalid general key in $filename on line $.");
    }

#if line starts new option block create corresponding fields
    if ($_=~s/^OPTION[ \t]+//i) {
      $inoption=1;
      $option=${$filter}{$_}={};
      ${$option}{"NAME"}=$_;
      ${$option}{"TYPE"}="KeyInfoBool";
      ${$option}{"DEFAULT"}="";
      ${$option}{"DESCRIPTION"}="";
      next;
    }
#fill option structure with information provided by line
#or terminate option block and use line to adjust other parts of the
#filters structure
    ( $_=~s/^TYPE[ \t]+//i) &&
     (${$option}{"TYPE"}=$_) && next;
    ( $_=~s/^DEF(?:AULT)[ \t]+//i) && (($_=~s/\\(?=[ \t])//g) || 1) &&
     (((${$option}{"DEFAULT"} ne "") && (${$option}{"DEFAULT"}.=":")) ||1) &&
     (${$option}{"DEFAULT"}.=prep_str($_)) && next;
    ( $_=~s/^DES(?:CRIPTION)[ \t]+//i) && (($_=~s/\\(?=[ \t])//g) || 1) &&
     (${$option}{"DESCRIPTION"}=prep_str($_)) && next;
    ( $_=~s/^FLAGS[ \t]+//i) && 
     (${$option}{"FLAGS"}=prep_str($_)) && next;
    ( $_=~s/^ENDOPTION(?:[ \t]+|$)//i) && 
     (($inoption=0)||1) && next;
    ( $_=~s/^STATIC[ \t]+//i) && (($feature=uc $_ ) || 1) &&
     (${$filter}{$feature}="new_".$filtername."_".(lc $_)) && 
     (($inoption=0)||1) && next;
    die "Invalid option key in $filename on line $.";
  }
  close OPTIONFILE;
}

#check what we have got. If no filter was found simply terminate. 
#In any other case create the static_filter.src.cpp file. It will be compiled
#into Aspell binary, all filters described within this file are assumed to be
#statically linked into Aspell

(scalar (@allfilters = keys %filters)) || exit 0;
open STATICFILTERS, ">gen/static_filters.src.cpp" || die "cant generate static filter description\n";
printf STATICFILTERS "/*File generated during static filter build\n".
                     "  Automatically generated file\n*/\n";

#declare all filter creation functions 
@rallfilters=();
while ($filter = shift @allfilters) {
  ( $filters{$filter}{"DECODER"} ne  "0") &&
   (printf STATICFILTERS "\n  extern \"C\" IndividualFilter * ".
    $filters{$filter}{"DECODER"}."();\n");
  ( $filters{$filter}{"FILTER"} ne "0") &&
   (printf STATICFILTERS "\n  extern \"C\" IndividualFilter * ".
                         $filters{$filter}{"FILTER"}."();\n");
  ( $filters{$filter}{"ENCODER"} ne "0") &&
   (printf STATICFILTERS "\n  extern \"C\" IndividualFilter * ".
                         $filters{$filter}{"ENCODER"}."();\n");
  push @rallfilters,$filter;
}

#create standard filter list denoting all statically linked filters
#and corresponding variables
@allfilters=(@rallfilters);
printf STATICFILTERS "\n  static FilterEntry standard_filters[] = {\n";
@filterhashes=();
@rallfilters=();
while ($filter = shift @allfilters) {
  push @filterhashes,$filters{$filter};
  (scalar @rallfilters) && (printf STATICFILTERS ",\n");
  printf STATICFILTERS "    {\"$filter\",".$filters{$filter}{"DECODER"}.
                                     ",".$filters{$filter}{"FILTER"}.
                                     ",".$filters{$filter}{"ENCODER"}."}";
  push @rallfilters,$filter;
}
printf STATICFILTERS "\n  };\n";
printf STATICFILTERS "\n  const unsigned int standard_filters_size = ".
                         "sizeof(standard_filters)/sizeof(FilterEntry);\n";


#create KeyInfo structures for each static filter
while ($filter = shift @filterhashes) {
  printf STATICFILTERS "\n  static KeyInfo ".${$filter}{"NAME"}."_options[] = {\n";

#create KeyInfo structs and begin end handles
  $firstopt = 1;
  while (($name,$option)=each %{$filter}) {
    ($name=~/(?:NAME|(?:DE|EN)CODER|FILTER|DESCRIPTION)/) && next;
    ( $firstopt != 1 ) && ( printf STATICFILTERS ",\n" );
    $firstopt = 0;
    printf STATICFILTERS "    {\n".
                            "      \"f-${$filter}{NAME}-$name\",\n";
    (    (lc ${$option}{"TYPE"}) eq "bool") &&
      printf STATICFILTERS  "      KeyInfoBool,\n";
    ( (lc ${$option}{"TYPE"}) eq "int") &&
      printf STATICFILTERS  "      KeyInfoInt,\n";
    ( (lc ${$option}{"TYPE"}) eq "string") &&
      printf STATICFILTERS  "      KeyInfoString,\n";
    ( (lc ${$option}{"TYPE"}) eq "list") &&
      printf STATICFILTERS  "      KeyInfoList,\n";
    print STATICFILTERS     "      \"".${$option}{"DEFAULT"}."\",\n".
                            "      \"".${$option}{"DESCRIPTION"}."\"\n".
                            "    }";
  }
  printf STATICFILTERS "\n  };\n";
  printf STATICFILTERS "\n  const KeyInfo * ".${$filter}{"NAME"}."_options_begin = ".
                                              ${$filter}{"NAME"}."_options;\n";
  # If structure is empty, set options_end to same as options_begin.
  if ($firstopt) {
    printf STATICFILTERS "\n  const KeyInfo * ".${$filter}{"NAME"}."_options_end = ".
                                                ${$filter}{"NAME"}."_options;\n";
  } else {
    printf STATICFILTERS "\n  const KeyInfo * ".${$filter}{"NAME"}."_options_end = ".
                                                ${$filter}{"NAME"}."_options+sizeof(".
                                                ${$filter}{"NAME"}."_options)/".
                                                "sizeof(KeyInfo);\n";
  }
}

#finally create filter modules list.
printf STATICFILTERS  "\n\n  static ConfigModule filter_modules[] = {\n";
#FIXME obsolete  due to modes moving to textfiles
#printf STATICFILTERS      "    {\"fm\",0,modes_module_begin,modes_module_end}";
$firstopt = 1;
while ($filter = shift @rallfilters) {
  ( $firstopt != 1 ) && ( printf STATICFILTERS ",\n" );
  $firstopt = 0;
  printf STATICFILTERS "    {\n".
                       "      \"$filter\",0,\n".
                       "      \"".${${filters}{$filter}}{DESCRIPTION}."\",\n" .
                       "      ${filter}_options_begin,${filter}_options_end\n" .
                       "    }";
} 
printf STATICFILTERS    "\n  };\n";
printf STATICFILTERS "\n  const ConfigModule * filter_modules_begin = ".
                         "filter_modules;\n";
printf STATICFILTERS "\n  const ConfigModule * filter_modules_end = ".
                         "filter_modules+sizeof(filter_modules)/".
                         "sizeof(ConfigModule);\n";
printf STATICFILTERS "\n  const size_t filter_modules_size = ".
                          "sizeof(filter_modules);\n";

close STATICFILTERS;

