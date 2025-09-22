# This Makefile is designed to be simple and readable.  It does not
# aim at portability.  It requires GNU Make.

BISON = bison
JAVAC = javac
JAVA = java

all: Calc.class

%.java %.html %.gv: %.y
	$(BISON) $(BISONFLAGS) --html --graph -o $*.java $<

%.class: %.java
	$(JAVAC) $(JAVACFLAGS) $<

run: Calc.class
	@echo "Type arithmetic expressions.  Quit with ctrl-d."
	$(JAVA) $(JAVAFLAGS) Calc

clean:
	rm -f *.class Calc.java Calc.html Calc.xml Calc.gv
