	// Valid instructions
	famax	v0.4h, v0.4h, v0.4h
	famax	v31.4h, v0.4h, v0.4h
	famax	v0.4h, v31.4h, v0.4h
	famax	v0.4h, v0.4h, v31.4h
	famax	v17.4h, v21.4h, v27.4h

	famax	v0.8h, v0.8h, v0.8h
	famax	v31.8h, v0.8h, v0.8h
	famax	v0.8h, v31.8h, v0.8h
	famax	v0.8h, v0.8h, v31.8h
	famax 	v2.8h, v7.8h, v4.8h

	famax	v0.2s, v0.2s, v0.2s
	famax	v31.2s, v0.2s, v0.2s
	famax	v0.2s, v31.2s, v0.2s
	famax	v0.2s, v0.2s, v31.2s
	famax 	v3.2s, v1.2s, v19.2s

	famax	v0.4s, v0.4s, v0.4s
	famax	v31.4s, v0.4s, v0.4s
	famax	v0.4s, v31.4s, v0.4s
	famax	v0.4s, v0.4s, v31.4s
	famax 	v9.4s, v8.4s, v7.4s

	famax	v0.2d, v0.2d, v0.2d
	famax	v31.2d, v0.2d, v0.2d
	famax	v0.2d, v31.2d, v0.2d
	famax	v0.2d, v0.2d, v31.2d
	famax 	v18.2d, v14.2d, v16.2d

	// Invalid instruction because Q = 0 and sz = 1 which is reserved
	.inst 0xef3dc23

	// Valid instructions
	famin	v0.4h, v0.4h, v0.4h
	famin	v31.4h, v0.4h, v0.4h
	famin	v0.4h, v31.4h, v0.4h
	famin	v0.4h, v0.4h, v31.4h
	famin	v17.4h, v21.4h, v27.4h

	famin	v0.8h, v0.8h, v0.8h
	famin	v31.8h, v0.8h, v0.8h
	famin	v0.8h, v31.8h, v0.8h
	famin	v0.8h, v0.8h, v31.8h
	famin 	v2.8h, v7.8h, v4.8h

	famin	v0.2s, v0.2s, v0.2s
	famin	v31.2s, v0.2s, v0.2s
	famin	v0.2s, v31.2s, v0.2s
	famin	v0.2s, v0.2s, v31.2s
	famin 	v3.2s, v1.2s, v19.2s

	famin	v0.4s, v0.4s, v0.4s
	famin	v31.4s, v0.4s, v0.4s
	famin	v0.4s, v31.4s, v0.4s
	famin	v0.4s, v0.4s, v31.4s
	famin 	v9.4s, v8.4s, v7.4s

	famin	v0.2d, v0.2d, v0.2d
	famin	v31.2d, v0.2d, v0.2d
	famin	v0.2d, v31.2d, v0.2d
	famin	v0.2d, v0.2d, v31.2d
	famin 	v18.2d, v14.2d, v16.2d

	// Invalid instruction because Q = 0 and sz = 1 which is reserved
	.inst	0x2ef3dc23
