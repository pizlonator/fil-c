./scripts/prepare_doxygen.sh
doxygen
mkdir -p doc/api/html/doc
for file in `ls doc/*png`; do
  echo "copying "$file
  cp $file doc/api/html/doc
done