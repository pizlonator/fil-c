# This Makefile is designed to be simple and readable.  It does not
# aim at portability.  It requires GNU Make.

BASE = c++-types
BISON = bison

all: $(BASE)

%.c %.h %.xml %.gv: %.y
	$(BISON) $(BISONFLAGS) --header --graph -o $*.c $<

$(BASE): $(BASE).o
	$(CC) $(CFLAGS) -o $@ $^

run: $(BASE)
	@echo "Type C++ declarations or expressions.  Quit with ctrl-d."
	./$<

CLEANFILES =									\
  $(BASE) *.o $(BASE).[ch] $(BASE).output $(BASE).xml $(BASE).html $(BASE).gv

clean:
	rm -f $(CLEANFILES)
