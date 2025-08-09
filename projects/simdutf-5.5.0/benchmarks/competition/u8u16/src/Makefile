CTANGLE= ctangle $(CTFLAGS)
CWEAVE = cweave $(CWFLAGS)
PDFLATEX = pdflatex $(TEXFLAGS)
CWFLAGS=
CTFLAGS=
TEXFLAGS=

.SUFFIXES: .tex .pdf .w

.w.tex:
	$(CWEAVE) $*

.tex.pdf:
	$(PDFLATEX) $*
	$(PDFLATEX) $*

.w.c:
	$(CTANGLE) $*
