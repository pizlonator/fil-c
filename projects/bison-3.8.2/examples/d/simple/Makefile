# This Makefile is designed to be simple and readable.  It does not
# aim at portability.  It requires GNU Make.

BISON = bison
DC = dmd

all: calc

%.d %.html %.gv: %.y
	$(BISON) $(BISONFLAGS) --html --graph -o $*.d $<

%: %.d
	$(DC) $(DCFLAGS) $<

run: calc
	@echo "Type arithmetic expressions.  Quit with ctrl-d."
	./$<

clean:
	rm -f calc calc.d calc.xml calc.gv calc.html *.o
