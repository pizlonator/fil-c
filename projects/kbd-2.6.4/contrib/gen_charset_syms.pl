#!/usr/bin/env perl
use strict;
use warnings;

use Getopt::Std;

my %options;
getopts("hs:n:", \%options);

sub usage {
    print STDERR "Usage: cat CHARSET_to_uni.trans | $0 -s START -n NAME\n";
    exit(1);
}

usage() if $options{h} || !$options{s} || !$options{n};
usage() if @ARGV;

my $start = int($options{s});
my $charset = $options{n};

my $keysyms = {
    "undefined" => "",

    "cyrillic_small_letter_ukrainian_ie" => "ukrainian_cyrillic_small_letter_ie",
    "cyrillic_small_letter_byelorussian_ukrainian_i" => "ukrainian_cyrillic_small_letter_i",
    "cyrillic_small_letter_yi" => "ukrainian_cyrillic_small_letter_yi",
    "cyrillic_capital_letter_ukrainian_ie" => "ukrainian_cyrillic_capital_letter_ie",
    "cyrillic_capital_letter_byelorussian_ukrainian_i" => "ukrainian_cyrillic_capital_letter_i",
    "cyrillic_capital_letter_yi" => "ukrainian_cyrillic_capital_letter_yi",
    "cyrillic_capital_letter_dje" => "serbocroatian_cyrillic_capital_letter_dje",
    "cyrillic_capital_letter_gje" => "macedonian_cyrillic_capital_letter_gje",
    "cyrillic_capital_letter_dze" => "macedonian_cyrillic_capital_letter_dze",
    "cyrillic_capital_letter_tshe" => "serbocroatian_cyrillic_capital_letter_chje",
    "cyrillic_capital_letter_kje" => "macedonian_cyrillic_capital_letter_kje",
    "cyrillic_capital_letter_short_u" => "bielorussian_cyrillic_capital_letter_short_u",
    "cyrillic_small_letter_dje" => "serbocroatian_cyrillic_small_letter_dje",
    "cyrillic_small_letter_gje" => "macedonian_cyrillic_small_letter_gje",
    "cyrillic_small_letter_dze" => "macedonian_cyrillic_small_letter_dze",
    "cyrillic_small_letter_tshe" => "serbocroatian_cyrillic_small_letter_chje",
    "cyrillic_small_letter_kje" => "macedonian_cyrillic_small_letter_kje",
    "cyrillic_small_letter_short_u" => "bielorussian_cyrillic_small_letter_short_u",

    "middle_dot" => "periodcentered",
    "not" => "notsign",
    "sharp_s" => "ssharp",
    "Ostroke" => "Ooblique",
    "ostroke" => "oslash",
    "Tstroke" => "Tslash",
    "tstroke" => "tslash",
    "greek_tonos" => "accent",
    "greek_dialytika_tonos" => "diaeresisaccent",
    "Xi" => "Ksi",
    "Chi" => "Khi",
    "xi" => "ksi",
    "chi" => "khi",
    "finalsigma" => "terminalsigma",
    "overline" => "overscore",
    "double_low_line" => "doubleunderscore",
    "no_break_space" => "nobreakspace",
    "inverted_exclamation_mark" => "exclamdown",
    "pound" => "sterling",
    "broken_bar" => "brokenbar",
    "feminine_ordinal_indicator" => "ordfeminine",
    "left_pointing_double_angle_quotation_mark" => "guillemotleft",
    "soft_hyphen" => "hyphen",
    "plus_minus" => "plusminus",
    "superscript_one" => "onesuperior",
    "superscript_two" => "twosuperior",
    "superscript_digit_two" => "twosuperior",
    "superscript_three" => "threesuperior",
    "micro" => "mu",
    "pilcrow" => "paragraph",
    "masculine_ordinal_indicator" => "masculine",
    "right_pointing_double_angle_quotation_mark" => "guillemotright",
    "inverted_question_mark" => "questiondown",
    "multiplication" => "multiply",
    "dotless_i" => "idotless",
    "numero" => "number_acronym",
    "horizontal_ellipsis" => "ellipsis",
    "double_dagger" => "doubledagger",
    "per_mille" => "permille",
    "em_dash" => "emdash",
    "en_dash" => "endash",
    "trade_mark" => "trademark",

    # should be synonyms?
    "cyrillic_small_letter_soft" => "cyrillic_small_soft_sign",
    "cyrillic_small_letter_hard" => "cyrillic_small_hard_sign",
    "cyrillic_capital_letter_soft" => "cyrillic_capital_soft_sign",
    "cyrillic_capital_letter_hard" => "cyrillic_capital_hard_sign",
};

my $table = [];
while (<STDIN>) {
    if (my ($c, $uni, $name) = /^0x([0-9a-fA-F]{2})[ \t]+U\+([0-9a-fA-F]{4})[ \t]+#(.*)/) {
        my $code = hex($c);
        next if $code < $start;

        $uni =~ y/[A-F]/[a-f]/;

        $name =~ s/^[ \t]+//;
        $name =~ s/[ \t]$//;
        $name =~ y/[A-Z]/[a-z]/;
        $name =~ s/[ -]+/_/g;

        $name =~ s/^latin_capital_(?:letter|ligature)_([a-z]+)/uc($1)/e;
        $name =~ s/^latin_small_(?:letter|ligature)_//;
        $name =~ s/^greek_capital_letter_([a-z])/uc($1)/e;
        $name =~ s/^greek_small_letter_//;
        $name =~ s/^hebrew_letter_//;
        $name =~ s/_with_tonos$/_with_accent/;
        $name =~ s/_with_dialytika$/_with_diaeresis/;
        $name =~ s/_with_dialytika_and_tonos$/_with_diaeresisaccent/;
        $name =~ s/^([A-Za-z]+)_with_/$1/;
        $name =~ s/^vulgar_fraction_([a-z]+)_([a-z]+)$/$1$2/;

        $name =~ s/acute_accent$/acute/;
        $name =~ s/dot_above$/abovedot/;
        $name =~ s/double_acute$/doubleacute/;
        $name =~ s/ring_above$/ring/;
        $name =~ s/_single_quotation_mark$/quote/;

        $name =~ s/_sign$//;
        $name =~ s/^final_/final/;
        $name =~ s/_\(.*\)$//;

        $table->[$code-$start] = { uni => $uni, keysym => $keysyms->{$name} // $name };
    } else {
        die "invalid line: $_";
    }
}

print "static sym\n";
print "const ${charset}[] = {\n";
foreach my $r (@$table) {
    if (!defined($r)) {
        print "\t{ 0xfffd, \"\" },\n";
    } else {
        print "\t{ 0x$r->{uni}, \"$r->{keysym}\" },\n";
    }
}
for (my $i = $start + scalar(@$table); $i < 256; $i++) {
    print "\t{ 0xfffd, \"\" },\n";
}
print "};\n";
