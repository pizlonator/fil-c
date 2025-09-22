topdir=$(shell pwd)/../..
include ../../Make.Rules

GOTARGET=$(shell eval $$(go env) ; echo $${GOHOSTOS}_$${GOARCH})

all: go/fib

go/fib: go/main.go go/fibber/fib.go go/fibber/linkage.go go/fibber/fibs_$(GOTARGET).s go/fibber/fib_$(GOTARGET).syso
	cd go && CGO_ENABLED=0 go build

# Build the host native version.
go/fibber/fib_$(GOTARGET).syso go/fibber/linkage.go: c/fib.c ./c/gcc.sh ./package_fns.sh
	GCC=gcc ./c/gcc.sh -O3 c/fib.c -c -o go/fibber/fib_$(GOTARGET).syso
	./package_fns.sh fibber go/fibber/fib_$(GOTARGET).syso > go/fibber/linkage.go

Dockerfile: Makefile ./mkdocker.sh
	./mkdocker.sh > $@

# Use this build target (make arms) to extend support to include arm
# and arm64 GOARCH values.
arms: Dockerfile Makefile ./c/gcc.sh ./c/build.sh ./c/fib.c
	docker run --rm -v $$PWD/c:/shared:z -h debian -u $$(id -u) -it expt shared/build.sh
	mv c/*.syso go/fibber/
	touch arms

clean:
	rm -f *~ arms
	rm -f c/*.o c/*~
	rm -f go/fib go/*~
	rm -f go/fibber/*.syso go/fibber/*~ go/fibber/linkage.go
