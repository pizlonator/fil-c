#!/bin/bash -e
# Copyright The GNU Toolchain Authors.
# This file is part of the GNU C Library.
#
# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

if ! [ -d advisories ]; then
  echo "error: Run me from the toplevel directory of the glibc repository."
  exit 1
fi

command=$1

usage () {
    cat >&2 <<EOF
usage: $0 {update|news}
EOF
    exit 1
}

command="$1"

case "$command" in
    update|news)
    ;;
    *)
        usage
        ;;
esac

get_rel() {
  rel=$(git describe $1 | sed 's/glibc-\([^g]\+\)-g.*/\1/')
  # If the latest tag for the commit is the development tag, then increment
  # the release version.
  if echo $rel | grep -q "\.9000"; then
    rel=$(echo $rel | sed 's/2\.\([0-9]\+\)\.9000.*/\1/')
    rel="2.$((rel+1))"
  fi
  echo $rel
}

advisories_update() {
  advisory=$1

  if [ -z $1 ]; then
    echo "Usage: $0 update GLIBC-SA-YYYY-NNNN"
    exit 1
  fi

  advisory_file=advisories/$advisory

  grep --color=none Commit $advisory_file | awk '{printf "%s %s\n", $1, $2}' |
    while read t r; do
      rel=$(get_rel $r)
      echo "*** Updating: $t $r ($rel)"
      sed -i "s/^$t $r.*/$t $r ($rel)/" $advisory_file
    done
}

advisories_news() {
  rel=$(get_rel "HEAD")
  for f in $(grep -l "^Fix-Commit: .* ($rel)$" advisories/*); do
    echo -e "  $(basename $f):"
    cve_id=$(sed -n 's/CVE-Id: \(.*\)/\1/p' $f)
    echo "$(head -1 $f) ($cve_id)" | fold -w 68 -s |
      while read line; do
	echo "    $line"
      done
    echo
  done
}

advisories_$command $2
