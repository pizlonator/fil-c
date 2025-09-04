
# Note - in theory we should set the entsize field.  But since
# we want this file to be assembled for both 32-bit and 64-bit
# targets we leave it empty.  Readelf will complain, but will
# carry on and (helpfully) it will set the entsize field for us.
#
# We also use the magic .dc.a pseudo-op set the correctly sized
# entries in the RELR array.  This works for most, but not all
# ELF based targets.
	
	.section .relr.foo, "a", %19
	.dc.a 0x1000
	.dc.a 0x0003
