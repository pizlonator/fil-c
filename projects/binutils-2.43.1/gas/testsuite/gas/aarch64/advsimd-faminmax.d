#objdump: -dr
#as: -march=armv9.5-a
#as: -march=armv8-a+faminmax

.*:     file format .*

Disassembly of section \.text:

0+ <.*>:
[^:]+:	0ec01c00 	famax	v0.4h, v0.4h, v0.4h
[^:]+:	0ec01c1f 	famax	v31.4h, v0.4h, v0.4h
[^:]+:	0ec01fe0 	famax	v0.4h, v31.4h, v0.4h
[^:]+:	0edf1c00 	famax	v0.4h, v0.4h, v31.4h
[^:]+:	0edb1eb1 	famax	v17.4h, v21.4h, v27.4h
[^:]+:	4ec01c00 	famax	v0.8h, v0.8h, v0.8h
[^:]+:	4ec01c1f 	famax	v31.8h, v0.8h, v0.8h
[^:]+:	4ec01fe0 	famax	v0.8h, v31.8h, v0.8h
[^:]+:	4edf1c00 	famax	v0.8h, v0.8h, v31.8h
[^:]+:	4ec41ce2 	famax	v2.8h, v7.8h, v4.8h
[^:]+:	0ea0dc00 	famax	v0.2s, v0.2s, v0.2s
[^:]+:	0ea0dc1f 	famax	v31.2s, v0.2s, v0.2s
[^:]+:	0ea0dfe0 	famax	v0.2s, v31.2s, v0.2s
[^:]+:	0ebfdc00 	famax	v0.2s, v0.2s, v31.2s
[^:]+:	0eb3dc23 	famax	v3.2s, v1.2s, v19.2s
[^:]+:	4ea0dc00 	famax	v0.4s, v0.4s, v0.4s
[^:]+:	4ea0dc1f 	famax	v31.4s, v0.4s, v0.4s
[^:]+:	4ea0dfe0 	famax	v0.4s, v31.4s, v0.4s
[^:]+:	4ebfdc00 	famax	v0.4s, v0.4s, v31.4s
[^:]+:	4ea7dd09 	famax	v9.4s, v8.4s, v7.4s
[^:]+:	4ee0dc00 	famax	v0.2d, v0.2d, v0.2d
[^:]+:	4ee0dc1f 	famax	v31.2d, v0.2d, v0.2d
[^:]+:	4ee0dfe0 	famax	v0.2d, v31.2d, v0.2d
[^:]+:	4effdc00 	famax	v0.2d, v0.2d, v31.2d
[^:]+:	4ef0ddd2 	famax	v18.2d, v14.2d, v16.2d
[^:]+:	0ef3dc23 	.inst	0x0ef3dc23 ; undefined
[^:]+:	2ec01c00 	famin	v0.4h, v0.4h, v0.4h
[^:]+:	2ec01c1f 	famin	v31.4h, v0.4h, v0.4h
[^:]+:	2ec01fe0 	famin	v0.4h, v31.4h, v0.4h
[^:]+:	2edf1c00 	famin	v0.4h, v0.4h, v31.4h
[^:]+:	2edb1eb1 	famin	v17.4h, v21.4h, v27.4h
[^:]+:	6ec01c00 	famin	v0.8h, v0.8h, v0.8h
[^:]+:	6ec01c1f 	famin	v31.8h, v0.8h, v0.8h
[^:]+:	6ec01fe0 	famin	v0.8h, v31.8h, v0.8h
[^:]+:	6edf1c00 	famin	v0.8h, v0.8h, v31.8h
[^:]+:	6ec41ce2 	famin	v2.8h, v7.8h, v4.8h
[^:]+:	2ea0dc00 	famin	v0.2s, v0.2s, v0.2s
[^:]+:	2ea0dc1f 	famin	v31.2s, v0.2s, v0.2s
[^:]+:	2ea0dfe0 	famin	v0.2s, v31.2s, v0.2s
[^:]+:	2ebfdc00 	famin	v0.2s, v0.2s, v31.2s
[^:]+:	2eb3dc23 	famin	v3.2s, v1.2s, v19.2s
[^:]+:	6ea0dc00 	famin	v0.4s, v0.4s, v0.4s
[^:]+:	6ea0dc1f 	famin	v31.4s, v0.4s, v0.4s
[^:]+:	6ea0dfe0 	famin	v0.4s, v31.4s, v0.4s
[^:]+:	6ebfdc00 	famin	v0.4s, v0.4s, v31.4s
[^:]+:	6ea7dd09 	famin	v9.4s, v8.4s, v7.4s
[^:]+:	6ee0dc00 	famin	v0.2d, v0.2d, v0.2d
[^:]+:	6ee0dc1f 	famin	v31.2d, v0.2d, v0.2d
[^:]+:	6ee0dfe0 	famin	v0.2d, v31.2d, v0.2d
[^:]+:	6effdc00 	famin	v0.2d, v0.2d, v31.2d
[^:]+:	6ef0ddd2 	famin	v18.2d, v14.2d, v16.2d
[^:]+:	2ef3dc23 	.inst	0x2ef3dc23 ; undefined
