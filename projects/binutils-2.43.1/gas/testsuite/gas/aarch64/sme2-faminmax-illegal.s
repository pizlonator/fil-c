// Operand mismatch
famax	{ z18.h - z19.h }, { z18.h - z19.h }, { z30.s - z31.s }
famax	{ z30.s - z31.s }, { z30.s - z31.s }, { z18.h - z19.h }
famax	{ z24.s - z27.s }, { z24.s - z27.s }, { z30.s - z31.s }
famax	{ z30.h - z31.h }, { z30.h - z31.h }, { z24.h - z27.h }
famin	{ z18.h - z19.h }, { z18.h - z19.h }, { z30.s - z31.s }
famin	{ z30.s - z31.s }, { z30.s - z31.s }, { z18.h - z19.h }
famin	{ z24.s - z27.s }, { z24.s - z27.s }, { z30.s - z31.s }
famin	{ z30.h - z31.h }, { z30.h - z31.h }, { z24.h - z27.h }

// Disallowed types
famax	{ z18.b - z19.b }, { z18.b - z19.b }, { z30.b - z31.b }
famax	{ z24.b - z27.b }, { z24.b - z27.b }, { z19.b - z22.b }
famin	{ z18.b - z19.b }, { z18.b - z19.b }, { z30.b - z31.b }
famin	{ z24.b - z27.b }, { z24.b - z27.b }, { z19.b - z22.b }

// Incorrect operands
famax	{ z18.h - z20.h }, { z18.h - z20.h }, { z30.h - z31.h }
famax	{ z27.s - z31.s }, { z27.s - z31.s }, { z16.s - z20.s }
famax	{ z27.s - z28.s }, { z27.s - z30.s }, { z16.s - z17.s }
famin	{ z18.h - z20.h }, { z18.h - z20.h }, { z30.h - z31.h }
famin	{ z27.s - z31.s }, { z27.s - z31.s }, { z16.s - z20.s }
famin	{ z27.s - z28.s }, { z27.s - z30.s }, { z16.s - z17.s }

// Incorrect number of arguments
famax	{ z18.h - z19.h }
famax	{ z18.h - z19.h }, { z18.h - z19.h  }
famax	{ z18.h - z19.h }, { z18.h - z19.h  }, { z12.h - z13.h }, { z12.h - z13.h }
famin	{ z18.h - z19.h }
famin	{ z18.h - z19.h }, { z18.h - z19.h  }
famin	{ z18.h - z19.h }, { z18.h - z19.h  }, { z12.h - z13.h }, { z12.h - z13.h }

// Spelling mistakes
famax	{ z18.h - z19.h }, { z18.h - z19.h }, { z30.t - z31.t }
famax	{ z18.t - z19.t }, { z18.h - z19.h }, { z30.h - z31.h }
famin	{ z18.h - z19.h }, { z18.h - z19.h }, { z30.t - z31.t }
famin	{ z18.t - z19.t }, { z18.h - z19.h }, { z30.h - z31.h }

// Missing qualifers
famax	{ z18.h - z19.h }, { z18.h - z19.h }, { 30.h - z31.h }
famax	{ z18.h - z19.h }, { 18.h - z19.h }, { z30.h - z31.h }
famax	{ z18.h - 19.h }, { z18.h - z19.h }, { z30.h - z31.h }
famax	{ z18.h - z19.h }, { z18.h - z19.h }, { z30 - z31.h }
famax	{ z18.h - z19.h }, { z18 - z19.h }, { z30.h - z31.h }
famax	{ z18 - z19.h }, { z18.h - z19.h }, { z30.h - z31.h }
famin	{ z18.h - z19.h }, { z18.h - z19.h }, { 30.h - z31.h }
famin	{ z18.h - z19.h }, { 18.h - z19.h }, { z30.h - z31.h }
famin	{ z18.h - 19.h }, { z18.h - z19.h }, { z30.h - z31.h }
famin	{ z18.h - z19.h }, { z18.h - z19.h }, { z30 - z31.h }
famin	{ z18.h - z19.h }, { z18 - z19.h }, { z30.h - z31.h }
famin	{ z18 - z19.h }, { z18.h - z19.h }, { z30.h - z31.h }

// Out of range numbers
famax	{ z18.h - z19.h }, { z18.h - z19.h }, { z34.h - z35.h }
famax	{ z34.h - z35.h }, { z34.h - z35.h }, { z18.h - z19.h }
famin	{ z18.h - z19.h }, { z18.h - z19.h }, { z34.h - z35.h }
famin	{ z34.h - z35.h }, { z34.h - z35.h }, { z18.h - z19.h }

// Preceded by movprfx
movprfx    z17, z9
famax	{ z18.h - z19.h }, { z18.h - z19.h }, { z30.h - z31.h }
movprfx    z17, z9
famin	{ z18.h - z19.h }, { z18.h - z19.h }, { z30.h - z31.h }
