ASFLAGS-.os += -fPIC

ifeq ($(subdir),elf)
# Extra shared linker files to link only into dl-allobjs.so.
sysdep-rtld-routines += dl-start
endif
