# This Makefile is designed to be simple and readable.  It does not
# aim at portability.  It requires GNU Make.

BASE = mfcalc
BISON = bison

all: $(BASE)

%.c %.html %.gv: %.y
	$(BISON) $(BISONFLAGS) --html --graph -o $*.c $<

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

run: $(BASE)
	@echo "Type arithmetic expressions.  Quit with ctrl-d."
	./$<

clean:
	rm -f $(BASE) $(BASE).c $(BASE).html $(BASE).xml $(BASE).gv $(BASE).output
