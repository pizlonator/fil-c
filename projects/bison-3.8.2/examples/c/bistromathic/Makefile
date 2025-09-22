# This Makefile is designed to be simple and readable.  It does not
# aim at portability.  It requires GNU Make.

BASE = bistromathic
BISON = bison

# We need to find the headers and libs for readline (and possibly intl).
# You probably need to customize this for your own environment.
CPPFLAGS = -I/opt/local/include
LDFLAGS = -L/opt/local/lib

# Find the translation catalog for Bison's generated messagess.
BISON_LOCALEDIR = $(shell $(BISON) $(BISON_FLAGS) --print-localedir)
CPPFLAGS += -DENABLE_NLS -DBISON_LOCALEDIR='"$(BISON_LOCALEDIR)"'

LIBS = -lreadline -lm # In some environments, -lintl is needed.

all: $(BASE)

%.c %.h %.html %.xml %.gv: %.y
	$(BISON) $(BISONFLAGS) --header --html --graph -o $*.c $<

$(BASE): parse.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

run: $(BASE)
	@echo "Type bistromathic expressions.  Quit with ctrl-d."
	./$<

CLEANFILES =						\
  $(BASE) *.o						\
  parse.[ch] parse.output parse.xml parse.html parse.gv

clean:
	rm -f $(CLEANFILES)
