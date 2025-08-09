#!/bin/bash

# Create very basic index.html and commit links for a static git archive

mkdir -p commit
git log --pretty=%H | while read i
do
  [ -e commit/$i ] && break
  git format-patch -1 --stdout $i > commit/$i
  ln -sf $i commit/${i::12}
done

echo '<html><body><font face=monospace><table border=1 cellpadding=2>'
echo '<tr valign=top><td>commit</td><td>author</td><td>date</td><td>description</td></tr>'
git log --pretty='%H%n%an<%ae>%n%ad%n%s' --date=format:'%r<br />%d-%m-%Y' | while read HASH
do
  HASH="${HASH::12}"
  read AUTHOR
  AUTHOR1="${AUTHOR/<*/}"
  AUTHOR1="${AUTHOR1::17}"
  AUTHOR2="&lt;${AUTHOR/*</}"
  AUTHOR2="${AUTHOR2::20}"
  read DATE
  DATE="${DATE/ /&nbsp;}"
  read DESC
  echo "<tr valign=top><td><a href=commit/$HASH>$HASH</a></td><td>$AUTHOR1<br />$AUTHOR2</td><td>$DATE</td><td>$DESC</td></tr>"
done
echo "</table></body></html>"
