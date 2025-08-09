#!/bin/bash

# Regenerate scripts/prereq (hopefully) portable build.

# Detect toybox prerequisites using record-commands

mkroot/record-commands make clean defconfig toybox
sed -i 's/default y/default n/' generated/Config.probed
CMDLIST="$(echo toybox; ./toybox cut -DF 1 log.txt | sort -u | grep -v nproc)"
{
  for i in $(tr '[:lower:]' '[:upper:]' <<<"$CMDLIST")
  do
    grep -qi CONFIG_$i'[= ]' .config && echo CONFIG_$i=y
  done
} > prereq.mini

# Create minimal dependency-free build

make clean allnoconfig KCONFIG_ALLCONFIG=prereq.mini
make toybox
cat > scripts/prereq/build.sh << 'EOF'
#!/bin/sh

BUILD='cc -funsigned-char -I scripts/prereq -I . -Os -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -fno-strict-aliasing -DTOYBOX_VERSION=""'
LINK=''
EOF
grep -A999 FILES= generated/build.sh >> scripts/prereq/build.sh
sed -i 's/ toybox$/&-prereq/' scripts/prereq/build.sh

# harvest stripped down headers

echo > scripts/prereq/generated/tags.h
sed 's/.*/#define HELP_& ""/' <<<"$CMDLIST" > scripts/prereq/generated/help.h
egrep "($(xargs <<<"$CMDLIST"|tr ' [:lower:]' '|[:upper:]'))" \
  generated/newtoys.h > scripts/prereq/generated/newtoys.h
FORS="$(sed -n 's/#define FOR_//p' $(grep -o 'toys/[^/]*/[^.]*\.c' scripts/prereq/build.sh) | xargs | tr ' ' '|')"
sed -En '1,/^$/p;/\/\/ ('"$FORS"') /,/^$/p;/#ifdef FOR_('"$FORS"')$/,/^$/p' generated/flags.h > scripts/prereq/generated/flags.h
egrep "OPTSTR_($(egrep -v "($FORS)" <<<"$CMDLIST" | xargs | tr ' ' '|'))" \
  generated/flags.h >> scripts/prereq/generated/flags.h
# TODO: slim down config.h
cp generated/{globals,config}.h scripts/prereq/generated/
