#!/bin/bash
#
# Handy script to rebuild the markdown version of the man pages.
# This uses pandoc if it is installed.
#
# For rendering the md, we can use a different command:
#
#   cd md; for x in *.md ; do pandoc -s $x --metadata pagetitle="${x%.md}" -o ${x%.md}.html --lua-filter=../md2html.lua ; done

if [[ -z "$(which pandoc)" ]]; then
    echo "pandoc not found - skipping conversion"
    exit 0
fi

outdir="$1"
if [[ -z "${outdir}" ]]; then
    echo "usage $0 <outdir>"
    exit 1
fi

mkdir -p "${outdir}"
if [[ $? -ne 0 ]]; then
    echo "failed to make output directory: ${outdir}"
    exit 1
fi

index="${outdir}/index.md"

function do_page () {
    m="$1"
    base="${m%.*}"
    sect="${m##*.}"
    output="${base}-${sect}.md"

    echo "converting ${m}" 1>&2

    redir="$(grep '^.so man' "${m}")"
    if [[ $? -eq 0 ]]; then
	r="${redir#*/}"
	rbase="${r%.*}"
	rsect="${r#*.}"
	echo "* [${base}(${sect})](${rbase}-${rsect}.md)" >> "${index}"
	return
    fi

    pandoc -f man -t markdown < "${m}" | sed 's/\*\*\([^*]\+\)\*\*(\([1358]\+\))/[\1(\2)](\1-\2.md)/g' > "${outdir}/${base}-${sect}.md"
    echo "* [${base}(${sect})](${base}-${sect}.md)" >> "${index}"
}

cat > "${index}" <<EOF
# Manpages for libcap and libpsx

EOF

if [[ -f "local-md.preamble" ]]; then
    cat "local-md.preamble" >> "${index}"
fi

cat >> "${index}" <<EOF

## Individual reference pages
EOF

# Assumes the m's are listed alphabetically.
for n in 1 3 5 8 ; do
	cat >> "${index}" <<EOF

### Section ${n}

EOF
    for m in *.${n}; do
	do_page "${m}"
    done
done

cat >> "${index}" <<EOF

## More information

EOF

if [[ -f "local-md.postscript" ]]; then
    cat "local-md.postscript" >> "${index}"
fi

cat >> "${index}" <<EOF

For further information, see the
[FullyCapable](https://sites.google.com/site/fullycapable/) homepage
for libcap.

## MD page generation

These official man pages for libcap and libpsx were converted to
markdown using [pandoc](https://pandoc.org).

EOF
