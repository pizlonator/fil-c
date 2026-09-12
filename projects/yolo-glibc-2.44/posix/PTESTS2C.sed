# Future self: the vertical bar is being used here as a delimiter in
# the input file, not in the usual alternate-choice regex meaning.
/^##/d
s/^# \(.*\)/  { 0, 0, "\1", NULL, },/
s/^#W \(.*\)/  { 0, 0, NULL, "\1" },/
s/\([^|]*\)|\([^|]*\)|\([^|]*\)|\([^|]*\)|\(.*\)/  { \1, \2, "\3", "\4", \5 },/
s/\\/\\\\/g
s//\\r/g
