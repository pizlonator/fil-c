#!/bin/awk
BEGIN {
  printf("const char *mke2fs_default_profile = \n");
}

{
  gsub("\"","\\\"",$0);
  printf("  \"%s\\n\"\n", $0);
}

END {
  printf(";\n", str)
}
