# update man pages

all: ../../g-ir-compiler.1 ../../g-ir-doc-tool.1 ../../g-ir-generate.1 ../../g-ir-scanner.1

../../%.1:%.rst
	rst2man $< > $@

.PHONY: clean

clean:
	rm -f ../../g-ir-compiler.1 ../../g-ir-doc-tool.1 ../../g-ir-generate.1 ../../g-ir-scanner.1
