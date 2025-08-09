#! /usr/bin/python

#                   PCRE2 UNICODE PROPERTY SUPPORT
#                   ------------------------------

# This script generates the pcre2_ucptables.c file, which contains tables for
# recognizing Unicode property names. It is #included by pcre2_tables.c. In
# order to reduce the number of relocations when loading the PCRE2 library, the
# names are held as a single large string, with offsets in the table. This is
# tedious to maintain by hand. Therefore, a script is used to generate the
# table.

# This script was created in December 2021 based on the previous GenerateUtt
# script, whose output had to be manually edited into pcre2_tables.c. Here is
# the history of the original script:

# -----------------------------------------------------------------------------
# Modified by PH 17-March-2009 to generate the more verbose form that works
# for UTF-support in EBCDIC as well as ASCII environments.
# Modified by PH 01-March-2010 to add new scripts for Unicode 5.2.0.
# Modified by PH 04-May-2010 to add new "X.." special categories.
# Modified by PH 30-April-2011 to add new scripts for Unicode 6.0.0
# Modified by ChPe 30-September-2012 to add this note; no other changes were
# necessary for Unicode 6.2.0 support.
# Modfied by PH 26-February-2013 to add the Xuc special category.
# Comment modified by PH 13-May-2014 to update to PCRE2 file names.
# Script updated to Python 3 by running it through the 2to3 converter.
# Added script names for Unicode 7.0.0, 20-June-2014.
# Added script names for Unicode 8.0.0, 19-June-2015.
# Added script names for Unicode 10.0.0, 02-July-2017.
# Added script names for Unicode 11.0.0, 03-July-2018.
# Added 'Unknown' script, 01-October-2018.
# Added script names for Unicode 12.1.0, 27-July-2019.
# Added script names for Unicode 13.0.0, 10-March-2020.
# Added Script names for Unicode 14.0.0, PCRE2-10.39
# Added support for bidi class and bidi control, 06-December-2021
#   This also involved lower casing strings and removing underscores, in
#   accordance with Unicode's "loose matching" rules, which Perl observes.
# Changed default script type from PT_SC to PT_SCX, 18-December-2021
# -----------------------------------------------------------------------------
#
# Note subsequent changes here:
#
# 27-December-2021: Added support for 4-letter script abbreviations.
# 10-January-2022:  Further updates for Boolean property support
# -----------------------------------------------------------------------------


# Import common data lists and functions

from GenerateCommon import \
  abbreviations, \
  bool_properties, \
  bidi_classes, \
  category_names, \
  general_category_names, \
  script_names, \
  open_output

# Open the output file (no return on failure). This call also writes standard
# header boilerplate.

f = open_output("pcre2_ucptables.c")

# The list in bidi_classes contains just the Unicode classes such as AN, LRE,
# etc., along with comments. We need to add "bidi" in front of each value, in
# order to create names that don't clash with other types of property.

bidi_class_names = []
for i in range(0, len(bidi_classes), 2):
  bidi_class_names.append("bidi" + bidi_classes[i])

# Remove the comments from other lists that contain them.

category_names = category_names[::2]

# Create standardized versions of the names by lowercasing and removing
# underscores.

def stdname(x):
  return x.lower().replace('_', '')

def stdnames(x):
  y = [''] * len(x)
  for i in range(len(x)):
    y[i] = stdname(x[i])
  return y

std_category_names = stdnames(category_names)
std_general_category_names = stdnames(general_category_names)
std_bidi_class_names = stdnames(bidi_class_names)
std_bool_properties = stdnames(bool_properties)

# Create the table, starting with the Unicode script, category and bidi class
# names. We keep both the standardized name and the original, because the
# latter is used for the ucp_xx names. NOTE: for the script abbreviations, we
# still use the full original names.

utt_table = []

scx_end = script_names.index('Unknown')

for idx, name in enumerate(script_names):
  pt_type = 'PT_SCX' if idx < scx_end else 'PT_SC'
  utt_table.append((stdname(name), name, pt_type))
  for abbrev in abbreviations[name]:
    utt_table.append((stdname(abbrev), name, pt_type))

# Add the remaining property lists

utt_table += list(zip(std_category_names, category_names, ['PT_PC'] * len(category_names)))
utt_table += list(zip(std_general_category_names, general_category_names, ['PT_GC'] * len(general_category_names)))
utt_table += list(zip(std_bidi_class_names, bidi_class_names, ['PT_BIDICL'] * len(bidi_class_names)))

for name in bool_properties:
  utt_table.append((stdname(name), name, 'PT_BOOL'))
  if name in abbreviations:
    for abbrev in abbreviations[name]:
      utt_table.append((stdname(abbrev), name, 'PT_BOOL'))

# Now add specials and synonyms. Note both the standardized and capitalized
# forms are needed.

utt_table.append(('any', 'Any', 'PT_ANY'))
utt_table.append(('l&',  'L&',  'PT_LAMP'))
utt_table.append(('lc',  'LC',  'PT_LAMP'))
utt_table.append(('xan', 'Xan', 'PT_ALNUM'))
utt_table.append(('xps', 'Xps', 'PT_PXSPACE'))
utt_table.append(('xsp', 'Xsp', 'PT_SPACE'))
utt_table.append(('xuc', 'Xuc', 'PT_UCNC'))
utt_table.append(('xwd', 'Xwd', 'PT_WORD'))

# Remove duplicates from the table and then sort it.

utt_table = list(set(utt_table)) 
utt_table.sort()

# Output file-specific heading

f.write("""\
#ifdef SUPPORT_UNICODE

/* The PRIV(utt)[] table below translates Unicode property names into type and
code values. It is searched by binary chop, so must be in collating sequence of
name. Originally, the table contained pointers to the name strings in the first
field of each entry. However, that leads to a large number of relocations when
a shared library is dynamically loaded. A significant reduction is made by
putting all the names into a single, large string and using offsets instead.
All letters are lower cased, and underscores are removed, in accordance with
the "loose matching" rules that Unicode advises and Perl uses. */
\n""")

# We have to use STR_ macros to define the strings so that it all works in
# UTF-8 mode on EBCDIC platforms.

for utt in utt_table:
  f.write('#define STRING_%s0' % (utt[0].replace('&', '_AMPERSAND')))
  for c in utt[0]:
    if c == '&':
      f.write(' STR_AMPERSAND')
    else:
      f.write(' STR_%s' % c);
  f.write(' "\\0"\n')

# Output the long string of concatenated names

f.write('\nconst char PRIV(utt_names)[] =\n');
last = ''
for utt in utt_table:
  if utt == utt_table[-1]:
    last = ';'
  f.write('  STRING_%s0%s\n' % (utt[0].replace('&', '_AMPERSAND'), last))

# Output the property type table

f.write('\nconst ucp_type_table PRIV(utt)[] = {\n')
offset = 0
last = ','
for utt in utt_table:
  if utt[2] in ('PT_ANY', 'PT_LAMP', 'PT_ALNUM', 'PT_PXSPACE',
      'PT_SPACE', 'PT_UCNC', 'PT_WORD'):
    value = '0'
  else:
    value = 'ucp_' + utt[1]
  if utt == utt_table[-1]:
    last = ''
  f.write('  { %3d, %s, %s }%s\n' % (offset, utt[2], value, last))
  offset += len(utt[0]) + 1
f.write('};\n\n')

# Ending text

f.write("""\
const size_t PRIV(utt_size) = sizeof(PRIV(utt)) / sizeof(ucp_type_table);

#endif /* SUPPORT_UNICODE */

/* End of pcre2_ucptables.c */
""")

f.close

# End
