# Copyright (c) 2024-2025 Epic Games, Inc. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

all: \
	../../pizfix/benchmarks/stepanov_container \
	../../pizfix/benchmarks/richards \
	../../pizfix/benchmarks/pcre_benchmark \
	../../pizfix/benchmarks/deltablue \
	../../pizfix/benchmarks/binarytrees \
	../../pizfix/benchmarks/fannkuch-redux \
	../../pizfix/benchmarks/fannkuch-redux-2 \
	../../pizfix/benchmarks/n-body \
	../../pizfix/benchmarks/n-body-2 \
	../../pizfix/benchmarks/spectralnorm \
	../../pizfix/benchmarks/reversecomplement

clean:
	rm -f ../../pizfix/benchmarks/stepanov_container
	rm -f ../../pizfix/benchmarks/richards
	rm -f ../../pizfix/benchmarks/pcre_benchmark
	rm -f ../../pizfix/benchmarks/deltablue
	rm -f ../../pizfix/benchmarks/binarytrees
	rm -f ../../pizfix/benchmarks/fannkuch-redux
	rm -f ../../pizfix/benchmarks/fannkuch-redux-2
	rm -f ../../pizfix/benchmarks/n-body
	rm -f ../../pizfix/benchmarks/n-body-2
	rm -f ../../pizfix/benchmarks/spectralnorm
	rm -f ../../pizfix/benchmarks/reversecomplement

../../pizfix/benchmarks/stepanov_container: stepanov_container.cpp
	../../build/bin/clang++ \
	    -o ../../pizfix/benchmarks/stepanov_container \
	    stepanov_container.cpp -O3 -g

../../pizfix/benchmarks/richards: richards.c
	../../build/bin/clang \
	    -o ../../pizfix/benchmarks/richards \
	    richards.c -O3 -g -Dbench100

../../pizfix/benchmarks/pcre_benchmark: pcre_benchmark.c
	../../build/bin/clang \
		-o ../../pizfix/benchmarks/pcre_benchmark \
		pcre_benchmark.c -O3 -g -lpcre2-8

../../pizfix/benchmarks/deltablue: deltablue.c
	../../build/bin/clang \
	    -o ../../pizfix/benchmarks/deltablue \
	    deltablue.c -O3 -g

../../pizfix/benchmarks/binarytrees: binarytrees.cpp
	../../build/bin/clang++ \
	    -o ../../pizfix/benchmarks/binarytrees \
	    binarytrees.cpp -O3 -g

../../pizfix/benchmarks/fannkuch-redux: fannkuch-redux.c
	../../build/bin/clang \
	    -o ../../pizfix/benchmarks/fannkuch-redux \
	    fannkuch-redux.c -O3 -g

../../pizfix/benchmarks/fannkuch-redux-2: fannkuch-redux-2.cpp
	../../build/bin/clang++ \
	    -o ../../pizfix/benchmarks/fannkuch-redux-2 \
	    fannkuch-redux-2.cpp -O3 -g

../../pizfix/benchmarks/n-body: n-body.c
	../../build/bin/clang \
	    -o ../../pizfix/benchmarks/n-body \
	    n-body.c -O3 -g -march=ivybridge

../../pizfix/benchmarks/n-body-2: n-body-2.c
	../../build/bin/clang \
	    -o ../../pizfix/benchmarks/n-body-2 \
	    n-body-2.c -O3 -g

../../pizfix/benchmarks/spectralnorm: spectralnorm.c
	../../build/bin/clang \
	    -o ../../pizfix/benchmarks/spectralnorm \
	    spectralnorm.c -O3 -g -mavx

../../pizfix/benchmarks/reversecomplement: reversecomplement.c
	../../build/bin/clang \
	    -o ../../pizfix/benchmarks/reversecomplement \
	    reversecomplement.c -O3 -g

