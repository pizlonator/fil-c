# This Makefile is designed to be simple and readable.  It does not
# aim at portability.  It requires GNU Make.

BASE = lexcalc
BISON = bison
FLEX = flex

all: $(BASE)

%.c %.h %.html %.xml %.gv: %.y
	$(BISON) $(BISONFLAGS) --header --html --graph -o $*.c $<

%.c: %.l
	$(FLEX) $(FLEXFLAGS) -o$@ $<

scan.o: parse.h
$(BASE): parse.o scan.o
	$(CC) $(CFLAGS) -o $@ $^

run: $(BASE)
	@echo "Type arithmetic expressions.  Quit with ctrl-d."
	./$<

CLEANFILES =						\
  $(BASE) *.o						\
  parse.[ch] parse.output parse.xml parse.html parse.gv	\
  scan.c
clean:
	rm -f $(CLEANFILES)
