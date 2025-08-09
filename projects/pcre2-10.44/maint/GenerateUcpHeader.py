#! /usr/bin/python

#                   PCRE2 UNICODE PROPERTY SUPPORT
#                   ------------------------------

# This script generates the pcre2_ucp.h file from Unicode data files. This
# header uses enumerations to give names to Unicode property types and script
# names.

# This script was created in December 2021 as part of the Unicode data
# generation refactoring.


# Import common data lists and functions

from GenerateCommon import \
  bidi_classes, \
  bool_properties, \
  bool_props_list_item_size, \
  break_properties, \
  category_names, \
  general_category_names, \
  script_list_item_size, \
  script_names, \
  open_output

# Open the output file (no return on failure). This call also writes standard
# header boilerplate.

f = open_output("pcre2_ucp.h")

# Output this file's heading text

f.write("""\
#ifndef PCRE2_UCP_H_IDEMPOTENT_GUARD
#define PCRE2_UCP_H_IDEMPOTENT_GUARD

/* This file contains definitions of the Unicode property values that are
returned by the UCD access macros and used throughout PCRE2.

IMPORTANT: The specific values of the first two enums (general and particular
character categories) are assumed by the table called catposstab in the file
pcre2_auto_possess.c. They are unlikely to change, but should be checked after
an update. */
\n""")

f.write("/* These are the general character categories. */\n\nenum {\n")
for i in general_category_names:
  f.write("  ucp_%s,\n" % i)
f.write("};\n\n")

f.write("/* These are the particular character categories. */\n\nenum {\n")
for i in range(0, len(category_names), 2):
  f.write("  ucp_%s,    /* %s */\n" % (category_names[i], category_names[i+1]))
f.write("};\n\n")

f.write("/* These are Boolean properties. */\n\nenum {\n")
for i in bool_properties:
  f.write("  ucp_%s,\n" % i)

f.write("  /* This must be last */\n")
f.write("  ucp_Bprop_Count\n};\n\n")

f.write("/* Size of entries in ucd_boolprop_sets[] */\n\n")
f.write("#define ucd_boolprop_sets_item_size %d\n\n" % bool_props_list_item_size)

f.write("/* These are the bidi class values. */\n\nenum {\n")
for i in range(0, len(bidi_classes), 2):
  sp = ' ' * (4 - len(bidi_classes[i]))
  f.write("  ucp_bidi%s,%s /* %s */\n" % (bidi_classes[i], sp, bidi_classes[i+1]))
f.write("};\n\n")

f.write("/* These are grapheme break properties. The Extended Pictographic "
  "property\ncomes from the emoji-data.txt file. */\n\nenum {\n")
for i in range(0, len(break_properties), 2):
  sp = ' ' * (21 - len(break_properties[i]))
  f.write("  ucp_gb%s,%s /* %s */\n" % (break_properties[i], sp, break_properties[i+1]))
f.write("};\n\n")

f.write("/* These are the script identifications. */\n\nenum {\n  /* Scripts which has characters in other scripts. */\n")
for i in script_names:
  if i == "Unknown":
    f.write("\n  /* Scripts which has no characters in other scripts. */\n")
  f.write("  ucp_%s,\n" % i)
f.write("\n")

f.write("  /* This must be last */\n")
f.write("  ucp_Script_Count\n};\n\n")

f.write("/* Size of entries in ucd_script_sets[] */\n\n")
f.write("#define ucd_script_sets_item_size %d\n\n" % script_list_item_size)

f.write("#endif  /* PCRE2_UCP_H_IDEMPOTENT_GUARD */\n\n")
f.write("/* End of pcre2_ucp.h */\n")

f.close()

# End
