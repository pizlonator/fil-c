GLIBC_PROVIDES dnl See aclocal.m4 in the top level source directory.

AC_CHECK_TOOL(MIG, mig, MISSING)
if test "x$MIG" = xMISSING; then
  AC_MSG_ERROR([cannot find required build tool mig])
fi
LIBC_CONFIG_VAR([MIG], [$MIG])

OLD_CPPFLAGS=$CPPFLAGS
if test -n "$sysheaders"; then
  CPPFLAGS="$CPPFLAGS $SYSINCLUDES"
fi

### Sanity checks for Mach header installation
CPPFLAGS="$CPPFLAGS -ffreestanding"
AC_CHECK_HEADER(mach/mach_types.h,,
                [AC_MSG_ERROR([cannot find Mach headers])], -)
AC_CHECK_HEADER(mach/mach_types.defs,, [dnl
AC_MSG_ERROR([cannot find Mach .defs files])], -)

dnl
dnl mach_TYPE_CHECK(foo_t, bar_t)
dnl
dnl Check if foo_t is defined by <mach/mach_types.h>.
dnl If not, compile with -Dfoo_t=bar_t.
dnl
AC_DEFUN([mach_TYPE_CHECK], [dnl
AC_CACHE_CHECK(for $1 in mach/mach_types.h, libc_cv_mach_$1,
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <mach/mach_types.h>]], [[extern $1 foo;]])],
[libc_cv_mach_$1=$1], [libc_cv_mach_$1=$2]))
if test [$]libc_cv_mach_$1 != $1; then
  DEFINES="$DEFINES -D$1=$2"
fi])

dnl
dnl OSF Mach has renamed these typedefs for some reason.
dnl
mach_TYPE_CHECK(task_t, task_port_t)
mach_TYPE_CHECK(thread_t, thread_port_t)

dnl
dnl The creation_time field is a GNU Mach addition the other variants lack.
dnl
AC_CACHE_CHECK(for creation_time in task_basic_info,
	       libc_cv_mach_task_creation_time, [dnl
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <mach/task_info.h>]], [[
extern struct task_basic_info *i;
long s = i->creation_time.seconds;
]])], [libc_cv_mach_task_creation_time=yes], [libc_cv_mach_task_creation_time=no])])
if test $libc_cv_mach_task_creation_time = no; then
  AC_MSG_ERROR([you need Mach headers supporting task_info.creation_time])
fi

dnl
dnl The Darwin variant no longer has <mach/mach.defs>
dnl but instead has several constituent .defs files.
dnl In this scenario we will presume there is a <mach/mach_interface.h>
dnl that contains an #include for each constituent header file,
dnl but we don't do a check for that here because in a bare
dnl environment the compile against those headers will fail.
dnl
mach_interface_list=
for ifc in mach mach4 gnumach \
	   clock clock_priv host_priv host_security ledger lock_set \
	   processor processor_set task task_notify thread_act vm_map \
	   memory_object memory_object_default i386/mach_i386 x86_64/mach_i386 \
	   ; do
  AC_CHECK_HEADER(mach/${ifc}.defs, [dnl
  mach_interface_list="$mach_interface_list $ifc"],, -)
done
if test "x$mach_interface_list" = x; then
  AC_MSG_ERROR([what manner of Mach is this?])
fi

dnl
dnl mach_RPC_CHECK(interface.defs, rpc_method, define)
dnl
dnl Check if rpc_method RPC is defined by interface.defs
dnl and define `define`.
dnl
AC_DEFUN([mach_RPC_CHECK], [dnl
AC_CACHE_CHECK(for $2 in $1, libc_cv_mach_rpc_$2, [dnl
AC_EGREP_HEADER($2, mach/$1,
		libc_cv_mach_rpc_$2=yes,
		libc_cv_mach_rpc_$2=no)])
if test $libc_cv_mach_rpc_$2 = yes; then
  AC_DEFINE([$3])
fi
])


mach_RPC_CHECK(mach_host.defs, host_page_size,
	       HAVE_HOST_PAGE_SIZE)
mach_RPC_CHECK(gnumach.defs, thread_set_name,
	       HAVE_MACH_THREAD_SET_NAME)
mach_RPC_CHECK(gnumach.defs, thread_get_name,
	       HAVE_MACH_THREAD_GET_NAME)

AC_CHECK_HEADER(mach/machine/ndr_def.h, [dnl
  DEFINES="$DEFINES -DNDR_DEF_HEADER='<mach/machine/ndr_def.h>'"], [dnl
AC_CHECK_HEADER(machine/ndr_def.h, [dnl
  DEFINES="$DEFINES -DNDR_DEF_HEADER='<machine/ndr_def.h>'"],, -)], -)

AC_CACHE_CHECK(for i386_io_perm_modify in mach_i386.defs,
	       libc_cv_mach_i386_ioports, [dnl
AC_EGREP_HEADER(i386_io_perm_modify, mach/machine/mach_i386.defs,
		libc_cv_mach_i386_ioports=yes,
		libc_cv_mach_i386_ioports=no)])
if test $libc_cv_mach_i386_ioports = yes; then
  AC_DEFINE([HAVE_I386_IO_PERM_MODIFY])
fi

AC_CACHE_CHECK(for i386_set_gdt in mach_i386.defs,
	       libc_cv_mach_i386_gdt, [dnl
AC_EGREP_HEADER(i386_set_gdt, mach/machine/mach_i386.defs,
		libc_cv_mach_i386_gdt=yes,
		libc_cv_mach_i386_gdt=no)])
if test $libc_cv_mach_i386_gdt = yes; then
  AC_DEFINE([HAVE_I386_SET_GDT])
fi

CPPFLAGS=$OLD_CPPFLAGS
