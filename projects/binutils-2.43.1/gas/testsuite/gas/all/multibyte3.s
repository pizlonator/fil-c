	.macro m
UmlautÜ\@:
	.endm

UmlautÄ:
	.irpc c,szß
UmlautÖ\@\c\():
	m
	.endr

	.warning "end of input"
