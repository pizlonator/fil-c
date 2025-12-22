#!/bin/sh

tmpdir=`mktemp --tmpdir -d gst.XXXXXXXXXX`
workdir=$PWD
cd $tmpdir
res=0

for file in $TEMPLATE_FILES; do
  name=`basename $file element-templates`
  $SRC_DIR/gst-element-maker gst$name $name
  if test $? -ne 0; then
    res=1
    break
  fi
done

cd $workdir
rm -rf $tmpdir
exit $res;

