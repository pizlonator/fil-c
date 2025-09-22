#!/bin/sh

# This script builds the code coverage of the testsuite.
# The shadow utils must have been compiled with -fprofile-arcs -ftest-coverage

cd ../build/shadow-4.1.3.1/
rm -rf ../coverage
mkdir ../coverage
lcov --directory . --capture --output-file=lcov.data

genhtml --frames --output-directory ../coverage/ --show-details lcov.data
