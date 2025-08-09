The build infrastructure adds a "make test_NAME" target for each NAME.test
file in this directory, and "make tests" iterates through all of them.

Individual tests boil down to a call to "scripts/test.sh NAME", and
testing all is "scripts/test.sh" with no arguments.

The test infrastructure, including the shell functions each test calls
(mostly "testcmd" and "optional") is described in scripts/test.sh
