	// Operand mismatch
	famax	v0.4s, v1.4h, v2.4h
	famax	v31.8h, v30.8h, v29.2s
	famax   v15.2s, v16.2d, v17.2s
	famin	v0.4s, v1.4h, v2.4h
	famin	v31.8h, v30.8h, v29.2s
	famin   v15.2s, v16.2d, v17.2s

	// Disallowed types
	famax	v0.8b, v1.4b, v2.4b
	famax	v10.16b, v9.16b, v8.16b
	famin	v0.8b, v1.4b, v2.4b
	famin	v10.16b, v9.16b, v8.16b

	// Incorrect number of arguments
	famax   v0.4h
	famax   v0.4h, v1.4h
	famax	v0.4h, v1.4h, v2.4h, v3.4h
	famin   v0.4h
	famin   v0.4h, v1.4h
	famin	v0.4h, v1.4h, v2.4h, v3.4h

	// Spelling mistakes
	famax	v0.4h, v1.4h, v2.4x
	famax	v0.2h, v2.2a, v1.2h
	famax	v2.2t, v0.2d, v1.2d
	famin	v0.4h, v1.4h, v2.4x
	famin	v0.2h, v2.2a, v1.2h
	famin	v2.2t, v0.2d, v1.2d

	// Missing qualifiers
	famax	v19.2d, 20.2d, v21.2d
	famax	19.4d, v20.4d, v21.4d
	famax	v19.2s, v20.2s, 21.2s
	famax	v19, v20.2d, v21.2d
	famax	v19.2d, v20, v21.2d
	famax	v19.2d, v20.2d, v21
	famin	v19.2d, 20.2d, v21.2d
	famin	19.4d, v20.4d, v21.4d
	famin	v19.2s, v20.2s, 21.2s
	famin	v19, v20.2d, v21.2d
	famin	v19.2d, v20, v21.2d
	famin	v19.2d, v20.2d, v21

	// Out of range numbers
	famax	v35.4d, v30.4d, v29.4d
	famax	v30.2s, v35.2s, v29.2s
	famax	v30.4s, v29.4s, v35.4s
	famin	v35.4d, v30.4d, v29.4d
	famin	v30.2s, v35.2s, v29.2s
	famin	v30.4s, v29.4s, v35.4s
