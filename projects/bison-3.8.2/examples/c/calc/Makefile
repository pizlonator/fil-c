# This Makefile is designed to be simple and readable.  It does not
# aim at portability.  It requires GNU Make.

BASE = calc
BISON = bison

all: $(BASE)

%.c %.h %.html %.xml %.gv: %.y
	$(BISON) $(BISONFLAGS) --header --html --graph -o $*.c $<

$(BASE): $(BASE).o
	$(CC) $(CFLAGS) -o $@ $^

run: $(BASE)
	@echo "Type arithmetic expressions.  Quit with ctrl-d."
	./$<

CLEANFILES =									\
  $(BASE) *.o $(BASE).[ch] $(BASE).output $(BASE).xml $(BASE).html $(BASE).gv

clean:
	rm -f $(CLEANFILES)
