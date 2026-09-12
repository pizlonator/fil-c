# defines target $(gen-gconv-modules) that ensures gconv-modules are available

gen-gconv-modules := $(common-objpfx)iconvdata/gconv-modules

$(gen-gconv-modules):
	$(MAKE) -C ../iconvdata subdir=iconvdata $@
