#! /bin/sh

. $srcdir/strip-reloc-subr.sh

testfiles hello_i386.ko hello_x86_64.ko hello_ppc64.ko hello_s390.ko \
	hello_aarch64.ko hello_m68k.ko hello_riscv64.ko hello_csky.ko \
	hello_arc_hs4.ko

# Most simple hello world kernel module for various architectures.
# Make sure that it contains debuginfo with CONFIG_DEBUG_INFO=y.
# ::::::::::::::
# Makefile
# ::::::::::::::
# obj-m	:= hello.o
# hello-y := init.o exit.o
# 
# all:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) \
#		CONFIG_DEBUG_INFO=y modules
# ::::::::::::::
# init.c
# ::::::::::::::
# #include <linux/kernel.h>
# #include <linux/module.h>
# 
# int init_module(void)
# {
#   printk(KERN_INFO "Hello, world!\n");
#   return 0;
# }
# ::::::::::::::
# exit.c
# ::::::::::::::
# #include <linux/kernel.h>
# #include <linux/module.h>
# 
# void cleanup_module()
# {
#   printk(KERN_INFO "Goodbye, World!\n");
# }
runtest hello_i386.ko 1
runtest hello_x86_64.ko 1
runtest hello_ppc64.ko 1
runtest hello_s390.ko 1
runtest hello_aarch64.ko 1
runtest hello_m68k.ko 1
runtest hello_riscv64.ko 1
runtest hello_csky.ko 1
runtest hello_arc_hs4.ko 1

exit $runtest_status
