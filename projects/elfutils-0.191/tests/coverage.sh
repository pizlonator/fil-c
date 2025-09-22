#! /usr/bin/env bash

if [ "x$1" = "x-v" ]; then
  verbose=yes
else
  verbose=no
fi

cd ..

for d in lib libasm libdw libdwfl libebl libelf backends src debuginfod; do
  tmp=$d-data
  cd $d
  unused=0
  unused_files=
  for f in *.gcno; do
    base="$(basename $f .gcno)"
    if [ -f "$base.c" ]; then fc="$base.c"; else fc="$base.cxx"; fi
    gcda="$base.gcda"
    if [ -f "$gcda" ]; then
      gcov -n -a "$fc" |
      gawk "/$d.$fc/ { getline; co=gensub(/.*:(.*)% .*/, \"\\\\1\", \"g\"); co=co+0.0; li=\$4+0; printf \"%-35s  %6.2f %5d\n\", \"$d/$fc\", co, li } " >> $tmp
    else
      unused=$(($unused + 1))
      unused_files="$unused_files $fc"
    fi
  done
  if [ -f $tmp ]; then
    gawk "{ copct=\$2; co=(\$3*copct)/100; toco+=(co+0); toli += (\$3+0); } END { printf \"%-12s %6.2f%% covered       unused files: %3d\n\", \"$d\", (toco*100)/toli, \"$unused\" }" $tmp
    rm -f $tmp
  else
    printf "%-12s   0.00%% covered       unused files: %3d\n" "$d" $unused
  fi
  if [ $verbose = yes ]; then
    for f in $unused_files; do
      printf '%-42s%s\n' '' $f
    done
  fi
  cd ..
done
