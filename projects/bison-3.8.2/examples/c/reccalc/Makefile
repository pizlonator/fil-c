# This Makefile is designed to be simple and readable.  It does not
# aim at portability.  It requires GNU Make.

BASE = reccalc
BISON = bison
FLEX = flex

all: $(BASE)

%.c %.h %.xml %.gv: %.y
	$(BISON) $(BISONFLAGS) --header --graph -o $*.c $<

%.c %.h: %.l
	$(FLEX) $(FLEXFLAGS) -o$*.c --header=$*.h $<

scan.o: parse.h
parse.o: scan.h
$(BASE): parse.o scan.o
	$(CC) $(CFLAGS) -o $@ $^

run: $(BASE)
	@echo "Type arithmetic expressions.  Quit with ctrl-d."
	./$<

CLEANFILES =						\
  $(BASE) *.o						\
  parse.[ch] parse.output parse.xml parse.html parse.gv	\
  scan.[ch]
clean:
	rm -f $(CLEANFILES)
