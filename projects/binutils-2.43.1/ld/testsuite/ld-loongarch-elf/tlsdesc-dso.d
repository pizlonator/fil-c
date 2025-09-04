#as:
#ld: -shared -z norelro --hash-style=both
#objdump: -dr
#skip: loongarch32-*-*

.*:     file format .*


Disassembly of section .text:

[0-9a-f]+ <fun_gl1>:
 +[0-9a-f]+:	[0-9a-f]+ 	pcaddi      	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcalau12i   	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$a0, \$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcaddi      	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcaddi      	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$ra, \$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	jirl        	\$ra, \$ra, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcalau12i   	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$a0, \$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcalau12i   	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$a0, \$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcaddi      	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcaddi      	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$ra, \$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	jirl        	\$ra, \$ra, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcalau12i   	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$a0, \$a0, .*

[0-9a-f]+ <fun_lo>:
 +[0-9a-f]+:	[0-9a-f]+ 	pcalau12i   	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$a0, \$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcaddi      	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcaddi      	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$ra, \$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	jirl        	\$ra, \$ra, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcaddi      	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcalau12i   	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$a0, \$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcalau12i   	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$a0, \$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcaddi      	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$ra, \$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	jirl        	\$ra, \$ra, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcaddi      	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcalau12i   	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$a0, \$a0, .*

[0-9a-f]+ <fun_external>:
 +[0-9a-f]+:	[0-9a-f]+ 	pcaddi      	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$ra, \$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	jirl        	\$ra, \$ra, .*

[0-9a-f]+ <fun_hidden>:
 +[0-9a-f]+:	[0-9a-f]+ 	pcaddi      	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$ra, \$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	jirl        	\$ra, \$ra, .*
 +[0-9a-f]+:	[0-9a-f]+ 	pcaddi      	\$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	ld.d        	\$ra, \$a0, .*
 +[0-9a-f]+:	[0-9a-f]+ 	jirl        	\$ra, \$ra, .*
