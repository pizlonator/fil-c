DISABLE_CIL ?= n

export DISABLE_CIL

all: 
	$(MAKE) -C src 
	$(MAKE) -C utils

install: 
	$(MAKE) -C include install
	$(MAKE) -C src install
	$(MAKE) -C utils install
	$(MAKE) -C man install

relabel:
	$(MAKE) -C src relabel

clean:
	$(MAKE) -C src clean
	$(MAKE) -C utils clean
	$(MAKE) -C tests clean

indent:
	$(MAKE) -C src $@
	$(MAKE) -C include $@
	$(MAKE) -C utils $@

test:
	$(MAKE) -C tests test

