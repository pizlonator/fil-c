#!/bin/sh

for dir in ./ subprojects/*/; do
    echo "git commit in $dir:"
    ( cd "$dir" && git describe --always --broken --dirty --long --tags ) || true
done
