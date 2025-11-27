all: _build

_build: Makefile *.rst images/*.svg */*.rst conf.py ../../NEWS
	python3 -m sphinx  -b html . _build

linkcheck:
	python3 -m sphinx  -b linkcheck -n . _build

clean:
	rm -Rf _build
