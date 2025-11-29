#include "unicode-names.h"

const char *
get_unicode_type_name (GUnicodeType type)
{
  const char *names[] = {
    "Other, Control",
    "Other, Format",
    "Other, Not Assigned",
    "Other, Private Use",
    "Other, Surrogate",
    "Letter, Lowercase",
    "Letter, Modifier",
    "Letter, Other",
    "Letter, Titlecase",
    "Letter, Uppercase",
    "Mark, Spacing",
    "Mark, Enclosing",
    "Mark, Nonspacing",
    "Number, Decimal Digit",
    "Number, Letter",
    "Number, Other",
    "Punctuation, Connector",
    "Punctuation, Dash",
    "Punctuation, Close",
    "Punctuation, Final quote",
    "Punctuation, Initial quote",
    "Punctuation, Other",
    "Punctuation, Open",
    "Symbol, Currency",
    "Symbol, Modifier",
    "Symbol, Math",
    "Symbol, Other",
    "Separator, Line",
    "Separator, Paragraph",
    "Separator, Space",
  };

  if (type < G_N_ELEMENTS (names))
    return names[type];

  return "Unknown";
}

const char *
get_break_type_name (GUnicodeBreakType type)
{
  const char *names[] = {
    "Mandatory Break",
    "Carriage Return",
    "Line Feed",
    "Attached Characters and Combining Marks",
    "Surrogates",
    "Zero Width Space",
    "Inseparable",
    "Non-breaking (\"Glue\")",
    "Contingent Break Opportunity",
    "Space",
    "Break Opportunity After",
    "Break Opportunity Before",
    "Break Opportunity Before and After",
    "Hyphen",
    "Nonstarter",
    "Opening Punctuation",
    "Closing Punctuation",
    "Ambiguous Quotation",
    "Exclamation/Interrogation",
    "Ideographic",
    "Numeric",
    "Infix Separator (Numeric)",
    "Symbols Allowing Break After",
    "Ordinary Alphabetic and Symbol Characters",
    "Prefix (Numeric)",
    "Postfix (Numeric)",
    "Complex Content Dependent (South East Asian)",
    "Ambiguous (Alphabetic or Ideographic)",
    "Unknown",
    "Next Line",
    "Word Joiner",
    "Hangul L Jamo",
    "Hangul V Jamo",
    "Hangul T Jamo",
    "Hangul LV Syllable",
    "Hangul LVT Syllable",
    "Closing Parenthesis",
    "Conditional Japanese Starter",
    "Hebrew Letter",
    "Regional Indicator",
    "Emoji Base",
    "Emoji Modifier",
    "Zero Width Joiner",
  };

  if (type < G_N_ELEMENTS (names))
    return names[type];

  return "Unknown";
}

const char *
get_combining_class_name (int cclass)
{
  const char *classes[256] = { 0, };

  classes[0] = "Not Reordered";
  classes[1] = "Overlay";
  classes[7] = "Nukta";
  classes[8] = "Kana Voicing";
  classes[9] = "Virama";
  classes[10] = "CCC10 (Hebrew)";
  classes[11] = "CCC11 (Hebrew)";
  classes[12] = "CCC12 (Hebrew)";
  classes[13] = "CCC13 (Hebrew)";
  classes[14] = "CCC14 (Hebrew)";
  classes[15] = "CCC15 (Hebrew)";
  classes[16] = "CCC16 (Hebrew)";
  classes[17] = "CCC17 (Hebrew)";
  classes[18] = "CCC18 (Hebrew)";
  classes[19] = "CCC19 (Hebrew)";
  classes[20] = "CCC20 (Hebrew)";
  classes[21] = "CCC21 (Hebrew)";
  classes[22] = "CCC22 (Hebrew)";
  classes[23] = "CCC23 (Hebrew)";
  classes[24] = "CCC24 (Hebrew)";
  classes[25] = "CCC25 (Hebrew)";
  classes[26] = "CCC26 (Hebrew)";

  classes[27] = "CCC27 (Arabic)";
  classes[28] = "CCC28 (Arabic)";
  classes[29] = "CCC29 (Arabic)";
  classes[30] = "CCC30 (Arabic)";
  classes[31] = "CCC31 (Arabic)";
  classes[32] = "CCC32 (Arabic)";
  classes[33] = "CCC33 (Arabic)";
  classes[34] = "CCC34 (Arabic)";
  classes[35] = "CCC35 (Arabic)";

  classes[36] = "CCC36 (Syriac)";

  classes[84] = "CCC84 (Telugu)";
  classes[85] = "CCC85 (Telugu)";

  classes[103] = "CCC103 (Thai)";
  classes[107] = "CCC107 (Thai)";

  classes[118] = "CCC118 (Lao)";
  classes[122] = "CCC122 (Lao)";

  classes[129] = "CCC129 (Tibetan)";
  classes[130] = "CCC130 (Tibetan)";
  classes[133] = "CCC133 (Tibetan)";

  classes[200] = "Attached Below Left";
  classes[202] = "Attached Below";
  classes[214] = "Attached Above";
  classes[216] = "Attached Above Right";
  classes[218] = "Below Left";
  classes[220] = "Below";
  classes[222] = "Below Right";
  classes[224] = "Left";
  classes[226] = "Right";
  classes[228] = "Above Left";
  classes[230] = "Above";
  classes[232] = "Above Right";
  classes[233] = "Double Below";
  classes[234] = "Double Above";
  classes[240] = "Iota Subscript";
  classes[255] = "Invalid";

  if (cclass < 256 && classes[cclass] != NULL)
    return classes[cclass];

  return "Unknown";
}
