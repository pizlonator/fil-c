#!/bin/sh

LD_LIBRARY_PATH=../.. ; export LD_LIBRARY_PATH
PAMC_AGENT_PATH="../agents" ; export PAMC_AGENT_PATH

./test.libpamc
