#!/bin/bash

set -ex

# Fixup Windows paths
python3 ./.gitlab-ci/fixup-cov-paths.py _coverage/*.lcov

for path in _coverage/*.lcov; do
    # Remove coverage from generated code in the build directory
    lcov --config-file .lcovrc -r "${path}" '*/_build/*' -o "$(pwd)/${path}"
    # Remove any coverage from system files
    lcov --config-file .lcovrc -e "${path}" "$(pwd)/*" -o "$(pwd)/${path}"
    # Remove coverage from the fuzz tests, since they are run on a separate CI system
    lcov --config-file .lcovrc -r "${path}" "*/fuzzing/*" -o "$(pwd)/${path}"
    # Remove coverage from copylibs and subprojects
    for lib in xdgmime libcharset gnulib; do
        lcov --config-file .lcovrc -r "${path}" "*/${lib}/*" -o "$(pwd)/${path}"
    done

    # Convert to cobertura format for gitlab integration
    cobertura_base="${path/.lcov}-cobertura"
    cobertura_xml="${cobertura_base}.xml"
    lcov_cobertura "${path}" --output "${cobertura_xml}"
    mkdir -p "${cobertura_base}"
    cobertura-split-by-package.py "${cobertura_xml}" "${cobertura_base}"
    rm -f "${cobertura_xml}"
done

genhtml \
    --ignore-errors=source \
    --config-file .lcovrc \
    _coverage/*.lcov \
    -o _coverage/coverage

cd _coverage
rm -f ./*.lcov

cat >index.html <<EOL
<html>
<body>
<ul>
<li><a href="coverage/index.html">Coverage</a></li>
</ul>
</body>
</html>
EOL

# Print a handy link to the coverage report
echo "Coverage report at: https://${CI_PROJECT_NAMESPACE}.pages.gitlab.gnome.org/-/${CI_PROJECT_NAME}/-/jobs/${CI_BUILD_ID}/artifacts/_coverage/coverage/index.html"
