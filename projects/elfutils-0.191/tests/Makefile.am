## Process this file with automake to create Makefile.in
##
## Copyright (C) 1996-2019 Red Hat, Inc.
## This file is part of elfutils.
##
## This file is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
## (at your option) any later version.
##
## elfutils is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##
include $(top_srcdir)/config/eu.am
BUILD_RPATH = \$$ORIGIN/../libasm:\$$ORIGIN/../libdw:\$$ORIGIN/../backends:\$$ORIGIN/../libelf

AM_CPPFLAGS += -I$(top_srcdir)/libasm -I$(top_srcdir)/libdw \
	    -I$(top_srcdir)/libdwfl -I$(top_srcdir)/libdwelf \
	    -I$(top_srcdir)/libebl -I$(top_srcdir)/libelf \
	    -I$(top_srcdir)/lib -I..
AM_LDFLAGS = -Wl,-rpath-link,../libasm:../libdw:../libelf

if TESTS_RPATH
AM_LDFLAGS += -Wl,-rpath,$(BUILD_RPATH)
tests_rpath = yes
else
tests_rpath = no
endif

check_PROGRAMS = arextract arsymtest newfile saridx scnnames sectiondump \
		  showptable update1 update2 update3 update4 test-nlist \
		  show-die-info get-files next-files get-lines next-lines \
		  get-pubnames \
		  get-aranges allfcts line2addr addrscopes funcscopes \
		  show-abbrev hash newscn ecp dwflmodtest \
		  find-prologues funcretval allregs rdwrmmap \
		  dwfl-bug-addr-overflow arls dwfl-bug-fd-leak \
		  dwfl-addr-sect dwfl-bug-report early-offscn \
		  dwfl-bug-getmodules dwarf-getmacros dwarf-ranges addrcfi \
		  dwfl-core-noncontig dwarfcfi \
		  test-flag-nobits dwarf-getstring rerequest_tag \
		  alldts typeiter typeiter2 low_high_pc \
		  test-elf_cntl_gelf_getshdr dwflsyms dwfllines \
		  dwfl-report-elf-align dwfl-report-segment-contiguous \
		  dwfl-report-offline-memory \
		  varlocs backtrace backtrace-child \
		  backtrace-data backtrace-dwarf debuglink debugaltlink \
		  buildid deleted deleted-lib.so aggregate_size peel_type \
		  vdsosyms \
		  getsrc_die strptr newdata elfstrtab dwfl-proc-attach \
		  elfshphehdr elfstrmerge dwelfgnucompressed elfgetchdr \
		  elfgetzdata elfputzdata zstrptr emptyfile vendorelf \
		  fillfile dwarf_default_lower_bound dwarf-die-addr-die \
		  get-units-invalid get-units-split attr-integrate-skel \
		  all-dwarf-ranges unit-info next_cfi \
		  elfcopy addsections xlate_notes elfrdwrnop \
		  dwelf_elf_e_machine_string \
		  getphdrnum leb128 read_unaligned \
		  msg_tst system-elf-libelf-test system-elf-gelf-test \
		  nvidia_extended_linemap_libdw elf-print-reloc-syms \
		  cu-dwp-section-info declfiles \
		  $(asm_TESTS)

asm_TESTS = asm-tst1 asm-tst2 asm-tst3 asm-tst4 asm-tst5 \
	    asm-tst6 asm-tst7 asm-tst8 asm-tst9

if BIARCH
check_PROGRAMS += backtrace-child-biarch
endif

# Substitute $(COMPILE).
backtrace-child-biarch$(EXEEXT): backtrace-child.c
	$(AM_V_CC)$(CC_BIARCH) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
		     $(AM_CPPFLAGS) $(CPPFLAGS) \
		     $(AM_CFLAGS) $(CFLAGS) $(backtrace_child_CFLAGS) \
		     $(AM_LDFLAGS) $(LDFLAGS) $(backtrace_child_LDFLAGS) \
		     -o $@ $<

if GCOV
GCOV_FLAGS=-fprofile-arcs -ftest-coverage
else
GCOV_FLAGS=
endif

# test_nlist checks its own symbol table, and expects various symbols
# to be in the order as specified in the source file. Explicitly set
# minimal CFLAGS. But add sanitizers if in use.
if USE_ADDRESS_SANITIZER
EXTRA_NLIST_CFLAGS=-fsanitize=address
else
if USE_MEMORY_SANITIZER
EXTRA_NLIST_CFLAGS=-fsanitize=memory -fsanitize-memory-track-origins
else
EXTRA_NLIST_CFLAGS=
endif
endif

test-nlist$(EXEEXT): test-nlist.c
	$(AM_V_CC)$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	  $(AM_CPPFLAGS) $(CPPFLAGS) \
	  $(test_nlist_CFLAGS) $(GCOV_FLAGS) -o $@ $< $(test_nlist_LDADD)

TESTS = run-arextract.sh run-arsymtest.sh run-ar.sh newfile test-nlist \
	run-ar-N.sh \
	update1 update2 update3 update4 \
	run-show-die-info.sh run-get-files.sh run-get-lines.sh \
	run-next-files.sh run-next-lines.sh \
	run-get-pubnames.sh run-get-aranges.sh run-allfcts.sh \
	run-show-abbrev.sh run-line2addr.sh hash \
	run-large-elf-file.sh \
	newscn run-strip-test.sh run-strip-test2.sh \
	run-strip-test3.sh run-strip-test4.sh run-strip-test5.sh \
	run-strip-test6.sh run-strip-test7.sh run-strip-test8.sh \
	run-strip-test9.sh run-strip-test10.sh run-strip-test11.sh \
	run-strip-test12.sh \
	run-strip-nothing.sh run-strip-g.sh run-annobingroup.sh \
	run-strip-groups.sh run-strip-strmerge.sh \
	run-strip-reloc-ko.sh run-strip-reloc-self.sh \
	run-strip-reloc-ppc64.sh \
	run-strip-nobitsalign.sh run-strip-remove-keep.sh \
	run-unstrip-test.sh run-unstrip-test2.sh run-unstrip-test3.sh \
	run-unstrip-test4.sh run-unstrip-M.sh run-elfstrmerge-test.sh \
	run-ecp-test.sh run-ecp-test2.sh run-alldts.sh \
	run-elflint-test.sh run-elflint-self.sh run-ranlib-test.sh \
	run-ranlib-test2.sh run-ranlib-test3.sh run-ranlib-test4.sh \
	run-addrscopes.sh run-strings-test.sh run-funcscopes.sh \
	run-find-prologues.sh run-allregs.sh run-addrcfi.sh \
	run-dwarfcfi.sh run-nm-syms.sh \
	run-nm-self.sh run-readelf-self.sh run-readelf-info-plus.sh \
	run-srcfiles-self.sh \
	run-readelf-compressed.sh \
	run-readelf-const-values.sh \
	run-varlocs-self.sh run-exprlocs-self.sh \
	run-readelf-test1.sh run-readelf-test2.sh run-readelf-test3.sh \
	run-readelf-test4.sh run-readelf-twofiles.sh \
	run-readelf-macro.sh run-readelf-loc.sh run-readelf-ranges.sh \
	run-readelf-aranges.sh run-readelf-line.sh run-readelf-z.sh \
	run-readelf-frames.sh \
	run-readelf-n.sh \
	run-retain.sh \
	run-native-test.sh run-bug1-test.sh \
	run-debuglink.sh run-debugaltlink.sh run-buildid.sh \
	dwfl-bug-addr-overflow run-addrname-test.sh \
	dwfl-bug-fd-leak dwfl-bug-report dwfl-report-segment-contiguous \
	run-dwfl-bug-offline-rel.sh run-dwfl-addr-sect.sh \
	run-disasm-x86.sh run-disasm-x86-64.sh \
	run-early-offscn.sh run-dwarf-getmacros.sh run-dwarf-ranges.sh \
	run-test-flag-nobits.sh run-prelink-addr-test.sh \
	run-dwarf-getstring.sh run-rerequest_tag.sh run-typeiter.sh \
	run-readelf-d.sh run-readelf-gdb_index.sh run-unstrip-n.sh \
	run-low_high_pc.sh run-macro-test.sh run-elf_cntl_gelf_getshdr.sh \
	run-test-archive64.sh run-readelf-vmcoreinfo.sh \
	run-readelf-mixed-corenote.sh run-dwfllines.sh \
	run-readelf-variant.sh run-readelf-fat-lto.sh \
	run-dwfl-report-elf-align.sh run-addr2line-test.sh \
	run-dwfl-report-offline-memory.sh \
	run-addr2line-C-test.sh \
	run-addr2line-i-test.sh run-addr2line-i-lex-test.sh \
	run-addr2line-i-demangle-test.sh run-addr2line-alt-debugpath.sh \
	run-varlocs.sh run-exprlocs.sh run-varlocs-vars.sh run-funcretval.sh \
	run-backtrace-native.sh run-backtrace-data.sh run-backtrace-dwarf.sh \
	run-backtrace-native-biarch.sh run-backtrace-native-core.sh \
	run-backtrace-native-core-biarch.sh run-backtrace-core-x86_64.sh \
	run-backtrace-fp-core-x86_64.sh \
	run-backtrace-fp-core-aarch64.sh \
	run-backtrace-fp-core-ppc64le.sh \
	run-backtrace-core-x32.sh \
	run-backtrace-core-i386.sh run-backtrace-fp-core-i386.sh \
	run-backtrace-core-ppc.sh \
	run-backtrace-core-s390x.sh run-backtrace-core-s390.sh \
	run-backtrace-core-aarch64.sh run-backtrace-core-sparc.sh \
	run-backtrace-demangle.sh run-stack-d-test.sh run-stack-i-test.sh \
	run-stack-demangled-test.sh run-readelf-zx.sh run-readelf-zp.sh \
	run-readelf-arm-flags.sh \
	run-readelf-addr.sh run-readelf-str.sh \
	run-readelf-multi-noline.sh \
	run-readelf-types.sh \
	run-readelf-dwz-multi.sh run-allfcts-multi.sh run-deleted.sh \
	run-linkmap-cut.sh run-aggregate-size.sh run-peel-type.sh \
	vdsosyms run-readelf-A.sh \
	run-getsrc-die.sh run-strptr.sh newdata elfstrtab dwfl-proc-attach \
	elfshphehdr run-lfs-symbols.sh run-dwelfgnucompressed.sh \
	run-elfgetchdr.sh \
	run-elfgetzdata.sh run-elfputzdata.sh run-zstrptr.sh \
	run-compress-test.sh \
	run-readelf-zdebug.sh run-readelf-zdebug-rel.sh \
	emptyfile vendorelf fillfile dwarf_default_lower_bound \
	run-dwarf-die-addr-die.sh \
	run-get-units-invalid.sh run-get-units-split.sh \
	run-attr-integrate-skel.sh \
	run-all-dwarf-ranges.sh run-unit-info.sh \
	run-reloc-bpf.sh \
	run-next-cfi.sh run-next-cfi-self.sh \
	run-reverse-sections.sh run-reverse-sections-self.sh \
	run-copyadd-sections.sh \
	run-copymany-be32.sh run-copymany-le32.sh \
	run-copymany-be64.sh run-copymany-le64.sh \
	run-typeiter-many.sh run-strip-test-many.sh \
	run-strip-version.sh run-xlate-note.sh \
	run-readelf-discr.sh \
	run-dwelf_elf_e_machine_string.sh \
	run-elfclassify.sh run-elfclassify-self.sh \
	run-disasm-riscv64.sh \
	run-pt_gnu_prop-tests.sh \
	run-getphdrnum.sh run-test-includes.sh \
	leb128 read_unaligned \
	msg_tst system-elf-libelf-test system-elf-gelf-test \
	$(asm_TESTS) run-disasm-bpf.sh run-low_high_pc-dw-form-indirect.sh \
	run-nvidia-extended-linemap-libdw.sh run-nvidia-extended-linemap-readelf.sh \
	run-readelf-dw-form-indirect.sh run-strip-largealign.sh \
	run-readelf-Dd.sh run-dwfl-core-noncontig.sh run-cu-dwp-section-info.sh \
	run-declfiles.sh

if !BIARCH
export ELFUTILS_DISABLE_BIARCH = 1
endif

if !DEMANGLE
export ELFUTILS_DISABLE_DEMANGLE = 1
endif

if LZMA
TESTS += run-readelf-s.sh run-dwflsyms.sh
endif

if HAVE_ZSTD
TESTS += run-readelf-compressed-zstd.sh
endif

if USE_ZSTD_COMPRESS
export ELFUTILS_ZSTD = 1
endif

if USE_MEMORY_SANITIZER
export ELFUTILS_MEMORY_SANITIZER = 1
endif

if DEBUGINFOD
check_PROGRAMS += debuginfod_build_id_find
# With the dummy delegation doesn't work
if !DUMMY_LIBDEBUGINFOD
TESTS += run-debuginfod-dlopen.sh \
	 run-debuginfod-artifact-running.sh \
	 run-debuginfod-fd-prefetch-caches.sh \
	 run-debuginfod-regex.sh \
	 run-debuginfod-duplicate-urls.sh \
	 run-debuginfod-file.sh \
	 run-debuginfod-sizetime.sh \
	 run-debuginfod-malformed.sh \
	 run-debuginfod-negative-cache.sh \
	 run-debuginfod-tmp-home.sh \
	 run-debuginfod-writable.sh \
	 run-debuginfod-no-urls.sh \
	 run-debuginfod-query-retry.sh \
	 run-debuginfod-extraction.sh \
	 run-debuginfod-archive-groom.sh \
	 run-debuginfod-archive-rename.sh \
	 run-debuginfod-archive-test.sh \
	 run-debuginfod-federation-sqlite.sh \
	 run-debuginfod-federation-link.sh \
         run-debuginfod-percent-escape.sh \
         run-debuginfod-x-forwarded-for.sh \
         run-debuginfod-response-headers.sh \
         run-debuginfod-extraction-passive.sh \
	 run-debuginfod-webapi-concurrency.sh \
	 run-debuginfod-section.sh \
	 run-debuginfod-IXr.sh
endif
if !OLD_LIBMICROHTTPD
# Will crash on too old libmicrohttpd
# Too many open file descriptors confuses libmicrohttpd < 0.9.51
TESTS += run-debuginfod-federation-metrics.sh
endif
endif

if HAVE_CXX11
check_PROGRAMS += funcretval_test++11
funcretval_test__11_SOURCES = funcretval_test++11.cxx
TESTS += run-funcretval++11.sh
endif

EXTRA_DIST = run-arextract.sh run-arsymtest.sh run-ar.sh \
	     run-ar-N.sh \
	     run-show-die-info.sh run-get-files.sh run-get-lines.sh \
	     run-next-files.sh run-next-lines.sh testfile-only-debug-line.bz2 \
	     run-get-pubnames.sh run-get-aranges.sh \
	     run-show-abbrev.sh run-strip-test.sh \
	     run-strip-test2.sh run-ecp-test.sh run-ecp-test2.sh \
	     testfile.bz2 testfile2.bz2 testfile3.bz2 testfile4.bz2 \
	     testfile5.bz2 testfile6.bz2 testfile7.bz2 testfile8.bz2 \
	     testfile9.bz2 testfile10.bz2 testfile11.bz2 testfile12.bz2 \
	     testfile13.bz2 run-strip-test3.sh run-allfcts.sh \
	     testfile_class_func.bz2 testfile_nested_funcs.bz2 \
	     testfile-lto-gcc10.bz2 \
	     testfile-lto-gcc9.bz2 testfile-lto-gcc8.bz2 \
	     run-line2addr.sh run-elflint-test.sh testfile14.bz2 \
	     run-strip-test4.sh run-strip-test5.sh run-strip-test6.sh \
	     run-strip-test7.sh run-strip-test8.sh run-strip-groups.sh \
	     run-strip-test9.sh run-strip-test10.sh run-strip-test11.sh \
	     run-strip-test12.sh \
	     run-strip-nothing.sh run-strip-remove-keep.sh run-strip-g.sh \
	     run-annobingroup.sh testfile-annobingroup.o.bz2 \
	     testfile-annobingroup-i386.o.bz2 \
	     testfile-annobingroup-x86_64.o.bz2 \
	     run-strip-strmerge.sh run-strip-nobitsalign.sh \
	     testfile-nobitsalign.bz2 testfile-nobitsalign.strip.bz2 \
	     run-strip-reloc-ko.sh run-strip-reloc-self.sh \
	     run-strip-reloc-ppc64.sh strip-reloc-subr.sh \
	     hello_i386.ko.bz2 hello_x86_64.ko.bz2 \
	     hello_ppc64.ko.bz2 hello_s390.ko.bz2 hello_aarch64.ko.bz2 \
	     hello_m68k.ko.bz2 hello_riscv64.ko.bz2 hello_csky.ko.bz2 \
	     hello_arc_hs4.ko.bz2 \
	     run-unstrip-test.sh run-unstrip-test2.sh \
	     testfile-info-link.bz2 testfile-info-link.debuginfo.bz2 \
	     testfile-info-link.stripped.bz2 run-unstrip-test3.sh \
	     run-unstrip-test4.sh testfile-strtab.bz2 \
	     testfile-strtab.stripped.bz2 testfile-strtab.debuginfo.bz2 \
	     run-unstrip-M.sh run-elfstrmerge-test.sh \
	     run-elflint-self.sh run-ranlib-test.sh run-ranlib-test2.sh \
	     run-ranlib-test3.sh run-ranlib-test4.sh \
	     run-addrscopes.sh run-strings-test.sh run-funcscopes.sh \
	     run-nm-syms.sh testfilesyms32.bz2 testfilesyms64.bz2 \
	     run-nm-self.sh run-readelf-self.sh run-readelf-info-plus.sh \
	     run-srcfiles-self.sh \
		 run-readelf-compressed.sh \
	     run-readelf-compressed-zstd.sh \
	     run-readelf-const-values.sh testfile-const-values.debug.bz2 \
	     run-addrcfi.sh run-dwarfcfi.sh \
	     testfile11-debugframe.bz2 testfile12-debugframe.bz2 \
	     testfileaarch64-debugframe.bz2 testfilearm-debugframe.bz2 \
	     testfileppc32-debugframe.bz2 testfileppc64-debugframe.bz2 \
	     testfilecsky.bz2 \
	     run-varlocs-self.sh run-exprlocs-self.sh \
	     run-find-prologues.sh run-allregs.sh run-native-test.sh \
	     run-addrname-test.sh run-dwfl-bug-offline-rel.sh \
	     run-dwfl-addr-sect.sh run-early-offscn.sh \
	     run-dwarf-getmacros.sh \
	     run-dwarf-ranges.sh debug-ranges-no-lowpc.o.bz2 \
	     testfileranges4.debug.bz2 testfileranges5.debug.bz2 \
	     testfilesplitranges5.debug.bz2 \
	     testfile-ranges-hello5.dwo.bz2 testfile-ranges-world5.dwo.bz2 \
	     run-test-flag-nobits.sh \
	     run-dwarf-getstring.sh run-rerequest_tag.sh run-alldts.sh \
	     testfile15.bz2 testfile15.debug.bz2 \
	     testfile16.bz2 testfile16.debug.bz2 \
	     testfile17.bz2 testfile17.debug.bz2 \
	     testfile18.bz2 testfile19.bz2 testfile19.index.bz2 \
	     testfile20.bz2 testfile20.index.bz2 \
	     testfile21.bz2 testfile21.index.bz2 \
	     testfile22.bz2 testfile23.bz2 testfile24.bz2 testfile25.bz2 \
	     testfile26.bz2 testfile27.bz2 \
	     coverage.sh test-subr.sh test-wrapper.sh debuginfod-subr.sh \
	     run-readelf-test1.sh run-readelf-test2.sh run-readelf-test3.sh \
	     run-readelf-test4.sh run-readelf-twofiles.sh \
	     run-bug1-test.sh testfile28.bz2 testfile28.rdwr.bz2 \
	     run-debuglink.sh run-debugaltlink.sh run-buildid.sh \
	     testfile29.bz2 testfile29.rdwr.bz2 \
	     testfile30.bz2 testfile31.bz2 testfile32.bz2 testfile33.bz2 \
	     testfile34.bz2 testfile35.bz2 testfile35.debug.bz2 \
	     testfile36.bz2 testfile36.debug.bz2 \
	     testfile37.bz2 testfile37.debug.bz2 \
	     testfile38.bz2 testfile39.bz2 testfile40.bz2 testfile40.debug.bz2 \
	     testfile41.bz2 testfile42.bz2 testfile42_noshdrs.bz2 \
	     testfile43.bz2 \
	     testfile44.S.bz2 testfile44.expect.bz2 run-disasm-x86.sh \
	     testfile45.S.bz2 testfile45.expect.bz2 run-disasm-x86-64.sh \
	     testfile46.bz2 testfile47.bz2 testfile48.bz2 testfile48.debug.bz2 \
	     testfile49.bz2 testfile50.bz2 testfile51.bz2 \
	     testfile-macros-0xff.bz2 \
	     run-readelf-macro.sh testfilemacro.bz2 \
	     run-readelf-loc.sh testfileloc.bz2 \
	     splitdwarf4-not-split4.dwo.bz2 \
	     testfile-splitdwarf4-not-split4.debug.bz2 \
	     run-readelf-ranges.sh \
	     run-readelf-aranges.sh run-readelf-line.sh testfilefoobarbaz.bz2 \
	     testfile-ppc64-min-instr.bz2 \
	     testfile-dwarf-45.source \
	     testfile-dwarf-4.bz2 testfile-dwarf-5.bz2 \
	     run-readelf-z.sh \
	     run-readelf-dwz-multi.sh libtestfile_multi_shared.so.bz2 \
	     testfile_multi.dwz.bz2 testfile_multi_main.bz2 \
	     testfile-dwzstr.bz2 testfile-dwzstr.multi.bz2 \
	     run-readelf-arm-flags.sh testfile-arm-flags.bz2 \
	     run-readelf-addr.sh run-readelf-str.sh \
	     run-readelf-multi-noline.sh testfile_multi_noline.bz2 \
	     run-readelf-types.sh \
	     run-readelf-frames.sh \
	     run-readelf-n.sh \
	     testfile-gnu-property-note.bz2 testfile-gnu-property-note.o.bz2 \
	     testfile_gnu_props.32le.o.bz2 \
	     testfile_gnu_props.64le.o.bz2 \
	     testfile_gnu_props.32be.o.bz2 \
	     testfile_gnu_props.64be.o.bz2 \
	     testfile-gnu-property-note-aarch64.bz2 \
	     run-retain.sh testfile-retain.o.bz2 \
	     run-allfcts-multi.sh \
	     test-offset-loop.bz2 test-offset-loop.alt.bz2 \
	     run-prelink-addr-test.sh \
	     testfile52-32.so.bz2 testfile52-32.so.debug.bz2 \
	     testfile52-32.prelink.so.bz2 testfile52-32.noshdrs.so.bz2 \
	     testfile52-64.so.bz2 testfile52-64.so.debug.bz2 \
	     testfile52-64.prelink.so.bz2 testfile52-64.noshdrs.so.bz2 \
	     testfile53-32.bz2 testfile53-32.debug.bz2 \
	     testfile53-32.prelink.bz2 testfile53-64.bz2 \
	     testfile53-64.debug.bz2 testfile53-64.prelink.bz2 \
	     testfile54-32.so.bz2 testfile54-32.so.debug.bz2 \
	     testfile54-32.prelink.so.bz2 testfile54-32.noshdrs.so.bz2 \
	     testfile54-64.so.bz2 testfile54-64.so.debug.bz2 \
	     testfile54-64.prelink.so.bz2 testfile54-64.noshdrs.so.bz2 \
	     testfile55-32.bz2 testfile55-32.debug.bz2 \
	     testfile55-32.prelink.bz2 testfile55-64.bz2 \
	     testfile55-64.debug.bz2 testfile55-64.prelink.bz2 \
	     testfile56.bz2 testfile57.bz2 testfile58.bz2 \
	     run-typeiter.sh testfile59.bz2 \
	     run-readelf-d.sh testlib_dynseg.so.bz2 \
	     run-readelf-Dd.sh \
	     testfile-s390x-hash-both.bz2 \
	     run-readelf-gdb_index.sh testfilegdbindex5.bz2 \
	     testfilegdbindex7.bz2 testfilegdbindex9.bz2 \
	     testfilegdbindex9-no-maininfo.bz2 \
	     run-readelf-s.sh testfilebazdbg.bz2 testfilebazdyn.bz2 \
	     testfilebazmin.bz2 testfilebazdbg.debug.bz2 testfilebazmdb.bz2 \
	     testfilebaztab.bz2 testfilebasmin.bz2 testfilebaxmin.bz2 \
	     testfilebazdbg_pl.bz2 testfilebazmin_pl.bz2 \
	     testfilebazdbg_plr.bz2 testfilebazmin_plr.bz2 \
	     testfilebazdbgppc64.bz2 testfilebazdbgppc64.debug.bz2 \
	     testfilebazdbgppc64_pl.bz2 testfilebazdbgppc64_plr.bz2 \
	     testfilebazdynppc64.bz2 testfilebazmdbppc64.bz2 \
	     testfilebazminppc64.bz2 testfilebazminppc64_pl.bz2 \
	     testfilebazminppc64_plr.bz2 testfilebaztabppc64.bz2 \
	     run-readelf-variant.sh testfile-ada-variant.bz2 \
	     run-readelf-fat-lto.sh testfile-dwarf5-fat-lto.o.bz2 \
	     run-dwflsyms.sh \
	     run-unstrip-n.sh testcore-rtlib.bz2 testcore-rtlib-ppc.bz2 \
	     run-low_high_pc.sh testfile_low_high_pc.bz2 \
	     run-macro-test.sh testfile-macinfo.bz2 testfile-macros.bz2 \
	     run-elf_cntl_gelf_getshdr.sh \
	     run-test-archive64.sh testarchive64.a.bz2 \
	     testfile61.bz2 \
	     run-readelf-vmcoreinfo.sh testfile62.bz2 \
	     run-readelf-mixed-corenote.sh testfile63.bz2 testfile64.bz2 \
	     testfile65.bz2 testfile67.bz2 testfile68.bz2 \
	     testfile69.core.bz2 testfile69.so.bz2 \
	     testfile70.core.bz2 testfile70.exec.bz2 testfile71.bz2 \
	     run-dwfllines.sh run-dwfl-report-elf-align.sh \
	     run-dwfl-report-offline-memory.sh \
	     testfile-dwfl-report-elf-align-shlib.so.bz2 \
	     testfilenolines.bz2 test-core-lib.so.bz2 test-core.core.bz2 \
	     test-core.exec.bz2 run-addr2line-test.sh \
	     run-addr2line-C-test.sh \
	     run-addr2line-i-test.sh testfile-inlines.bz2 \
	     testfile-inlines-lto.bz2 \
	     run-addr2line-i-lex-test.sh testfile-lex-inlines.bz2 \
	     run-addr2line-i-demangle-test.sh run-addr2line-alt-debugpath.sh \
	     testfileppc32.bz2 testfileppc64.bz2 \
	     testfiles390.bz2 testfiles390x.bz2 \
	     testfilearm.bz2 testfileaarch64.bz2 \
	     run-varlocs.sh run-exprlocs.sh run-varlocs-vars.sh \
	     testfile-vars-clang-dwarf4-32.o.bz2 \
	     testfile-vars-clang-dwarf4-64.o.bz2 \
	     testfile-vars-clang-dwarf5-32.o.bz2 \
	     testfile-vars-clang-dwarf5-64.o.bz2 \
	     testfile-vars-gcc-dwarf4-32.o.bz2 \
	     testfile-vars-gcc-dwarf4-64.o.bz2 \
	     testfile-vars-gcc-dwarf5-32.o.bz2 \
	     testfile-vars-gcc-dwarf5-64.o.bz2 \
	     testfile-stridex.bz2 \
	     testfile_const_type.c testfile_const_type.bz2 \
	     testfile_implicit_pointer.c testfile_implicit_pointer.bz2 \
	     testfile_parameter_ref.c testfile_parameter_ref.bz2 \
	     testfile_entry_value.c testfile_entry_value.bz2 \
	     testfile_implicit_value.c testfile_implicit_value.bz2 \
	     testfile_aarch64_core.bz2 testfile_i686_core.bz2 \
	     addrx_constx-4.dwo.bz2 addrx_constx-5.dwo.bz2 \
	     testfile-addrx_constx-4.bz2 testfile-addrx_constx-5.bz2 \
	     run-funcretval.sh funcretval_test.c funcretval_test_aarch64.bz2 \
	     run-backtrace-data.sh run-backtrace-dwarf.sh cleanup-13.c \
	     run-backtrace-native.sh run-backtrace-native-biarch.sh \
	     run-backtrace-native-core.sh run-backtrace-native-core-biarch.sh \
	     run-backtrace-core-x86_64.sh run-backtrace-core-i386.sh \
	     run-backtrace-fp-core-x86_64.sh \
	     run-backtrace-core-x32.sh \
	     run-backtrace-fp-core-aarch64.sh \
	     backtrace.aarch64.fp.core.bz2 backtrace.aarch64.fp.exec.bz2 \
	     backtrace-subr.sh backtrace.i386.core.bz2 backtrace.i386.exec.bz2 \
	     run-backtrace-fp-core-i386.sh \
	     backtrace.i386.fp.core.bz2 backtrace.i386.fp.exec.bz2 \
	     run-backtrace-fp-core-ppc64le.sh \
	     backtrace.ppc64le.fp.core.bz2 backtrace.ppc64le.fp.exec.bz2 \
	     backtrace.x86_64.core.bz2 backtrace.x86_64.exec.bz2 \
	     backtrace.x86_64.fp.core.bz2 backtrace.x86_64.fp.exec.bz2 \
	     backtrace.ppc.core.bz2 backtrace.ppc.exec.bz2 \
	     run-backtrace-core-ppc.sh testfile66.bz2 testfile66.core.bz2 \
	     backtrace.s390x.core.bz2 backtrace.s390x.exec.bz2 \
	     backtrace.s390.core.bz2 backtrace.s390.exec.bz2 \
	     run-backtrace-core-s390x.sh run-backtrace-core-s390.sh \
	     run-backtrace-core-aarch64.sh \
	     backtrace.aarch64.core.bz2 backtrace.aarch64.exec.bz2 \
	     run-backtrace-core-sparc.sh \
	     backtrace.sparc.core.bz2 backtrace.sparc.exec.bz2 \
	     run-backtrace-demangle.sh testfile-backtrace-demangle.bz2 \
	     testfile-backtrace-demangle.cc \
	     testfile-backtrace-demangle.core.bz2 \
	     run-stack-d-test.sh run-stack-i-test.sh \
	     run-stack-demangled-test.sh \
	     testfiledwarfinlines.bz2 testfiledwarfinlines.core.bz2 \
	     run-readelf-zdebug.sh testfile-debug.bz2 testfile-zdebug.bz2 \
	     run-readelf-zdebug-rel.sh testfile-debug-rel.o.bz2 \
	     testfile-debug-rel-g.o.bz2 testfile-debug-rel-z.o.bz2 \
	     run-readelf-zx.sh run-readelf-zp.sh \
	     run-deleted.sh run-linkmap-cut.sh linkmap-cut-lib.so.bz2 \
	     linkmap-cut.bz2 linkmap-cut.core.bz2 \
	     run-aggregate-size.sh testfile-sizes1.o.bz2 testfile-sizes2.o.bz2 \
	     testfile-sizes3.o.bz2 testfile-sizes4.o.bz2 testfile-sizes4.s \
	     run-peel-type.sh \
	     run-readelf-A.sh testfileppc32attrs.o.bz2 \
	     testfilesparc64attrs.o.bz2 testfileppc64attrs.o.bz2 \
	     testfile-debug-types.bz2 \
	     run-getsrc-die.sh run-strptr.sh \
	     testfile-x32-core.bz2 testfile-x32.bz2 \
	     backtrace.x32.core.bz2 backtrace.x32.exec.bz2 \
	     testfile-x32-s.bz2 testfile-x32-d.bz2 testfile-x32-debug.bz2 \
	     run-lfs-symbols.sh lfs-symbols testfile-nolfs.bz2 \
	     testfile-zgnu32.bz2 testfile-zgnu64.bz2 \
	     testfile-zgnu32be.bz2 testfile-zgnu64be.bz2 \
	     run-dwelfgnucompressed.sh \
	     testfile-zgabi32.bz2 testfile-zgabi64.bz2 \
	     testfile-zgabi32be.bz2 testfile-zgabi64be.bz2 \
	     run-elfgetchdr.sh run-elfgetzdata.sh run-elfputzdata.sh \
	     run-zstrptr.sh run-compress-test.sh \
	     run-disasm-bpf.sh \
	     testfile-bpf-dis1.expect.bz2 testfile-bpf-dis1.o.bz2 \
	     run-reloc-bpf.sh \
	     testfile-bpf-reloc.expect.bz2 testfile-bpf-reloc.o.bz2 \
	     testfile-m68k-core.bz2 testfile-m68k.bz2 testfile-m68k-s.bz2 \
	     run-dwarf-die-addr-die.sh \
	     run-get-units-invalid.sh run-get-units-split.sh \
	     testfile-hello4.dwo.bz2 testfile-hello5.dwo.bz2 \
	     testfile-splitdwarf-4.bz2 testfile-splitdwarf-5.bz2 \
	     testfile-world5.dwo.bz2 testfile-world4.dwo.bz2 \
	     run-attr-integrate-skel.sh \
	     run-all-dwarf-ranges.sh testfilesplitranges4.debug.bz2 \
	     testfile-ranges-hello.dwo.bz2 testfile-ranges-world.dwo.bz2 \
	     run-unit-info.sh run-next-cfi.sh run-next-cfi-self.sh \
	     testfile-riscv64.bz2 testfile-riscv64-s.bz2 \
	     testfile-riscv64-core.bz2 \
	     run-reverse-sections.sh run-reverse-sections-self.sh \
	     run-copyadd-sections.sh test-copymany-subr.sh \
	     run-copymany-be32.sh run-copymany-le32.sh \
	     run-copymany-be64.sh run-copymany-le64.sh \
	     run-large-elf-file.sh \
	     run-typeiter-many.sh run-strip-test-many.sh \
	     testfile-debug-rel-ppc64-g.o.bz2 \
	     testfile-debug-rel-ppc64-z.o.bz2 \
	     testfile-debug-rel-ppc64.o.bz2 \
	     run-strip-version.sh testfile-version.bz2 \
	     run-xlate-note.sh \
	     run-readelf-discr.sh \
	     testfile-rng.debug.bz2 testfile-urng.debug.bz2 \
	     run-dwelf_elf_e_machine_string.sh \
	     run-elfclassify.sh run-elfclassify-self.sh \
	     run-disasm-riscv64.sh \
	     testfile-riscv64-dis1.o.bz2 testfile-riscv64-dis1.expect.bz2 \
             run-debuginfod-extraction.sh \
             run-debuginfod-federation-link.sh \
             run-debuginfod-federation-metrics.sh \
             run-debuginfod-artifact-running.sh \
             run-debuginfod-federation-sqlite.sh \
             run-debuginfod-x-forwarded-for.sh \
	     run-debuginfod-fd-prefetch-caches.sh \
	     run-debuginfod-regex.sh \
	     run-debuginfod-duplicate-urls.sh \
	     run-debuginfod-file.sh \
	     run-debuginfod-sizetime.sh \
	     run-debuginfod-dlopen.sh \
	     run-debuginfod-malformed.sh \
	     run-debuginfod-negative-cache.sh \
	     run-debuginfod-tmp-home.sh \
	     run-debuginfod-writable.sh \
	     run-debuginfod-no-urls.sh \
	     run-debuginfod-query-retry.sh \
	     run-debuginfod-archive-groom.sh \
	     run-debuginfod-archive-rename.sh \
             run-debuginfod-archive-test.sh \
             run-debuginfod-percent-escape.sh \
	     run-debuginfod-response-headers.sh \
             run-debuginfod-extraction-passive.sh \
             run-debuginfod-webapi-concurrency.sh \
	     run-debuginfod-section.sh \
	     run-debuginfod-IXr.sh \
	     debuginfod-rpms/fedora30/hello2-1.0-2.src.rpm \
	     debuginfod-rpms/fedora30/hello2-1.0-2.x86_64.rpm \
	     debuginfod-rpms/fedora30/hello2-debuginfo-1.0-2.x86_64.rpm \
	     debuginfod-rpms/fedora30/hello2-debugsource-1.0-2.x86_64.rpm \
	     debuginfod-rpms/fedora30/hello2-two-1.0-2.x86_64.rpm \
	     debuginfod-rpms/fedora30/hello2-two-debuginfo-1.0-2.x86_64.rpm \
	     debuginfod-rpms/fedora31/hello3-1.0-2.src.rpm \
	     debuginfod-rpms/fedora31/hello3-1.0-2.x86_64.rpm \
	     debuginfod-rpms/fedora31/hello3-debuginfo-1.0-2.x86_64.rpm \
	     debuginfod-rpms/fedora31/hello3-debugsource-1.0-2.x86_64.rpm \
	     debuginfod-rpms/fedora31/hello3-two-1.0-2.x86_64.rpm \
	     debuginfod-rpms/fedora31/hello3-two-debuginfo-1.0-2.x86_64.rpm \
	     debuginfod-rpms/hello2.specfile \
	     debuginfod-rpms/hello3.specfile \
	     debuginfod-rpms/rhel6/hello2-1.0-2.i686.rpm \
	     debuginfod-rpms/rhel6/hello2-1.0-2.src.rpm \
	     debuginfod-rpms/rhel6/hello2-debuginfo-1.0-2.i686.rpm \
	     debuginfod-rpms/rhel6/hello2-two-1.0-2.i686.rpm \
	     debuginfod-rpms/rhel7/hello2-1.0-2.src.rpm \
	     debuginfod-rpms/rhel7/hello2-1.0-2.x86_64.rpm \
	     debuginfod-rpms/rhel7/hello2-debuginfo-1.0-2.x86_64.rpm \
	     debuginfod-rpms/rhel7/hello2-two-1.0-2.x86_64.rpm \
	     debuginfod-rpms/rhel7/hello2-two-1.0-2.x86_64.rpm \
	     debuginfod-debs/hithere-dbgsym_1.0-1_amd64.ddeb \
	     debuginfod-debs/hithere_1.0-1.debian.tar.xz \
	     debuginfod-debs/hithere_1.0-1.dsc \
	     debuginfod-debs/hithere_1.0-1_amd64.deb \
	     debuginfod-debs/hithere_1.0.orig.tar.gz \
	     debuginfod-tars/hello-1-1-x86_64.pkg.tar.xz \
	     debuginfod-tars/hello-debug-1-1-x86_64.pkg.tar.bz2 \
	     debuginfod-tars/pacman-sources/PKGBUILD \
	     debuginfod-tars/pacman-sources/README.md \
	     debuginfod-tars/pacman-sources/hello.c \
	     run-pt_gnu_prop-tests.sh \
	     testfile_pt_gnu_prop.bz2 testfile_pt_gnu_prop32.bz2 \
	     run-getphdrnum.sh testfile-phdrs.elf.bz2 \
	     run-test-includes.sh run-low_high_pc-dw-form-indirect.sh \
	     run-readelf-dw-form-indirect.sh testfile-dw-form-indirect.bz2 \
	     run-nvidia-extended-linemap-libdw.sh run-nvidia-extended-linemap-readelf.sh \
	     testfile_nvidia_linemap.bz2 \
	     testfile-largealign.o.bz2 run-strip-largealign.sh \
	     run-funcretval++11.sh \
	     test-ar-duplicates.a.bz2 \
	     run-dwfl-core-noncontig.sh testcore-noncontig.bz2 \
	     testfile-dwarf5-line-clang.bz2 \
	     testfile-dwp-4.bz2 testfile-dwp-4.dwp.bz2 \
	     testfile-dwp-4-strict.bz2 testfile-dwp-4-strict.dwp.bz2 \
	     testfile-dwp-5.bz2 testfile-dwp-5.dwp.bz2 testfile-dwp.source \
	     run-cu-dwp-section-info.sh run-declfiles.sh \
	     testfile-dwp-5-cu-index-overflow.bz2 \
	     testfile-dwp-5-cu-index-overflow.dwp.bz2 \
	     testfile-dwp-4-cu-index-overflow.bz2 \
	     testfile-dwp-4-cu-index-overflow.dwp.bz2 \
	     testfile-dwp-cu-index-overflow.source


if USE_VALGRIND
valgrind_cmd=valgrind -q --leak-check=full --error-exitcode=1
endif


installed_TESTS_ENVIRONMENT = libdir='$(DESTDIR)$(libdir)'; \
			      bindir='$(DESTDIR)$(bindir)'; \
			      LC_ALL=C; LANG=C; \
			      VALGRIND_CMD='$(valgrind_cmd)'; \
			      abs_srcdir='$(abs_srcdir)'; \
			      abs_builddir='$(abs_builddir)'; \
			      abs_top_builddir='$(abs_top_builddir)'; \
			      export abs_srcdir; export abs_builddir; \
			      export abs_top_builddir; \
			      export libdir; export bindir; \
			      export LC_ALL; export LANG; export VALGRIND_CMD; \
			      unset DEBUGINFOD_URLS; \
			      NM='$(NM)'; export NM; \
			      CC='$(CC)'; export CC;
installed_LOG_COMPILER = $(abs_srcdir)/test-wrapper.sh \
			 installed $(tests_rpath) \
			 '$(program_transform_name)'
TESTS_ENVIRONMENT = LC_ALL=C; LANG=C; VALGRIND_CMD='$(valgrind_cmd)'; \
		    abs_srcdir='$(abs_srcdir)'; \
		    abs_builddir='$(abs_builddir)'; \
		    abs_top_builddir='$(abs_top_builddir)'; \
		    export abs_srcdir; export abs_builddir; \
		    export abs_top_builddir; \
		    export LC_ALL; export LANG; export VALGRIND_CMD; \
		    unset DEBUGINFOD_URLS; \
		    NM='$(NM)'; export NM; \
		    CC='$(CC)'; export CC;
LOG_COMPILER = $(abs_srcdir)/test-wrapper.sh \
	       $(abs_top_builddir)/libdw:$(abs_top_builddir)/backends:$(abs_top_builddir)/libelf:$(abs_top_builddir)/libasm:$(abs_top_builddir)/debuginfod

installcheck-local:
	$(MAKE) $(AM_MAKEFLAGS) \
		TESTS_ENVIRONMENT="$(installed_TESTS_ENVIRONMENT)" \
		LOG_COMPILER="$(installed_LOG_COMPILER)" check-TESTS

if BUILD_STATIC
libdw = ../libdw/libdw.a -lz $(zip_LIBS) $(libelf) $(libebl) -ldl -lpthread
libelf = ../libelf/libelf.a -lz $(zstd_LIBS)
libasm = ../libasm/libasm.a
else
libdw = ../libdw/libdw.so
libelf = ../libelf/libelf.so
libasm = ../libasm/libasm.so
endif
libebl = ../libebl/libebl.a ../backends/libebl_backends.a ../libcpu/libcpu.a
libeu = ../lib/libeu.a

arextract_LDADD = $(libelf)
arsymtest_LDADD = $(libelf)
newfile_LDADD = $(libelf)
saridx_LDADD = $(libeu) $(libelf)
scnnames_LDADD = $(libelf)
sectiondump_LDADD = $(libeu) $(libelf)
showptable_LDADD = $(libelf)
hash_LDADD = $(libelf)
test_nlist_CFLAGS =-g -O0 $(EXTRA_NLIST_CFLAGS)
test_nlist_LDADD = $(libelf)
msg_tst_LDADD = $(libelf)
newscn_LDADD = $(libelf)
early_offscn_LDADD = $(libeu) $(libelf)
ecp_LDADD = $(libeu) $(libelf)
update1_LDADD = $(libelf)
update2_LDADD = $(libelf)
update3_LDADD = $(libdw) $(libelf)
update4_LDADD = $(libdw) $(libelf)
show_die_info_LDADD = $(libdw) $(libelf)
get_pubnames_LDADD = $(libdw) $(libelf)
show_abbrev_LDADD = $(libdw) $(libelf)
get_lines_LDADD = $(libdw) $(libelf)
next_lines_LDADD = $(libdw) $(libelf)
get_files_LDADD = $(libdw) $(libelf)
next_files_LDADD = $(libdw) $(libelf)
get_aranges_LDADD = $(libdw) $(libelf)
allfcts_LDADD = $(libdw) $(libelf)
line2addr_LDADD = $(libeu) $(libdw) $(argp_LDADD)
addrscopes_LDADD = $(libeu) $(libdw) $(argp_LDADD)
funcscopes_LDADD = $(libeu) $(libdw) $(argp_LDADD)
funcretval_LDADD = $(libeu) $(libdw) $(argp_LDADD)
allregs_LDADD = $(libeu) $(libdw) $(argp_LDADD)
find_prologues_LDADD = $(libeu) $(libdw) $(argp_LDADD)
#show_ciefde_LDADD = ../libdwarf/libdwarf.so $(libelf)
asm_tst1_LDADD = $(libasm) $(libebl) $(libelf) $(libdw)
asm_tst2_LDADD = $(libasm) $(libebl) $(libelf) $(libdw)
asm_tst3_LDADD = $(libasm) $(libebl) $(libelf) $(libdw)
asm_tst4_LDADD = $(libasm) $(libebl) $(libelf) $(libdw)
asm_tst5_LDADD = $(libasm) $(libebl) $(libelf) $(libdw)
asm_tst6_LDADD = $(libasm) $(libebl) $(libelf) $(libdw)
asm_tst7_LDADD = $(libasm) $(libebl) $(libelf) $(libdw)
asm_tst8_LDADD = $(libasm) $(libebl) $(libelf) $(libdw)
asm_tst9_LDADD = $(libasm) $(libebl) $(libelf) $(libdw)
dwflmodtest_LDADD = $(libeu) $(libdw) $(libebl) $(libelf) $(argp_LDADD)
rdwrmmap_LDADD = $(libeu) $(libelf)
dwfl_bug_addr_overflow_LDADD = $(libdw) $(libebl) $(libelf)
arls_LDADD = $(libelf)
dwfl_bug_fd_leak_LDADD = $(libeu) $(libdw) $(libebl) $(libelf)
dwfl_bug_report_LDADD = $(libdw) $(libebl) $(libelf)
dwfl_bug_getmodules_LDADD = $(libeu) $(libdw) $(libebl) $(libelf)
dwfl_addr_sect_LDADD = $(libeu) $(libdw) $(libebl) $(libelf) $(argp_LDADD)
dwfl_core_noncontig_LDADD = $(libdw) $(libelf)
dwarf_getmacros_LDADD = $(libdw)
dwarf_ranges_LDADD = $(libdw)
dwarf_getstring_LDADD = $(libdw)
addrcfi_LDADD = $(libdw) $(libebl) $(libelf) $(argp_LDADD)
dwarfcfi_LDADD = $(libeu) $(libdw) $(libelf)
test_flag_nobits_LDADD = $(libelf)
rerequest_tag_LDADD = $(libdw)
alldts_LDADD = $(libdw) $(libelf)
typeiter_LDADD = $(libdw) $(libelf)
typeiter2_LDADD = $(libdw) $(libelf)
low_high_pc_LDADD = $(libdw) $(libelf) $(argp_LDADD)
test_elf_cntl_gelf_getshdr_LDADD = $(libelf)
dwflsyms_LDADD = $(libdw) $(libelf) $(argp_LDADD)
dwfllines_LDADD = $(libeu) $(libdw) $(libelf) $(argp_LDADD)
dwfl_report_elf_align_LDADD = $(libeu) $(libdw)
dwfl_report_offline_memory_LDADD = $(libeu) $(libdw)
dwfl_report_segment_contiguous_LDADD = $(libdw) $(libebl) $(libelf)
varlocs_LDADD = $(libeu) $(libdw) $(libelf) $(argp_LDADD)
backtrace_LDADD = $(libeu) $(libdw) $(libelf) $(argp_LDADD)
# backtrace-child-biarch also uses those *_CFLAGS and *_LDLAGS variables:
backtrace_child_CFLAGS = $(fpie_CFLAGS)
backtrace_child_LDFLAGS = -pie -pthread
backtrace_child_biarch_SOURCES = backtrace-child.c
backtrace_data_LDADD = $(libeu) $(libdw) $(libelf)
backtrace_dwarf_CFLAGS = -Wno-unused-parameter
backtrace_dwarf_LDADD = $(libeu) $(libdw) $(libelf)
debuglink_LDADD = $(libeu) $(libdw) $(libelf)
debugaltlink_LDADD = $(libeu) $(libdw) $(libelf)
buildid_LDADD = $(libeu) $(libdw) $(libelf)
deleted_LDADD = ./deleted-lib.so
deleted_lib_so_LDFLAGS = -shared
deleted_lib_so_CFLAGS = $(fpic_CFLAGS) -fasynchronous-unwind-tables
aggregate_size_LDADD = $(libdw) $(libelf) $(argp_LDADD)
peel_type_LDADD = $(libdw) $(libelf) $(argp_LDADD)
vdsosyms_LDADD = $(libeu) $(libdw) $(libelf)
getsrc_die_LDADD = $(libeu) $(libdw) $(libelf)
strptr_LDADD = $(libelf)
newdata_LDADD = $(libelf)
elfstrtab_LDADD = $(libelf)
dwfl_proc_attach_LDADD = $(libeu) $(libdw)
dwfl_proc_attach_LDFLAGS = -pthread -rdynamic $(AM_LDFLAGS)
elfshphehdr_LDADD =$(libelf)
elfstrmerge_LDADD = $(libeu) $(libdw) $(libelf)
dwelfgnucompressed_LDADD = $(libelf) $(libdw)
elfgetchdr_LDADD = $(libelf) $(libdw)
elfgetzdata_LDADD = $(libelf)
elfputzdata_LDADD = $(libelf)
zstrptr_LDADD = $(libelf)
emptyfile_LDADD = $(libelf)
vendorelf_LDADD = $(libelf)
fillfile_LDADD = $(libelf)
dwarf_default_lower_bound_LDADD = $(libdw)
dwarf_die_addr_die_LDADD = $(libdw)
get_units_invalid_LDADD = $(libdw)
get_units_split_LDADD = $(libdw)
attr_integrate_skel_LDADD = $(libdw)
all_dwarf_ranges_LDADD = $(libdw)
unit_info_LDADD = $(libdw)
next_cfi_LDADD = $(libeu) $(libelf) $(libdw)
elfcopy_LDADD = $(libelf)
addsections_LDADD = $(libelf)
debuginfod_build_id_find_LDADD = $(libelf) $(libdw)
xlate_notes_LDADD = $(libelf)
elfrdwrnop_LDADD = $(libelf)
dwelf_elf_e_machine_string_LDADD = $(libelf) $(libdw)
getphdrnum_LDADD = $(libelf) $(libdw)
leb128_LDADD = $(libelf) $(libdw)
read_unaligned_LDADD = $(libelf) $(libdw)
nvidia_extended_linemap_libdw_LDADD = $(libelf) $(libdw)
elf_print_reloc_syms_LDADD = $(libelf)
cu_dwp_section_info_LDADD = $(libdw)
declfiles_LDADD = $(libdw)

# We want to test the libelf headers against the system elf.h header.
# Don't include any -I CPPFLAGS. Except when we install our own elf.h.
# For the gelf test we do want our own libelf.h, but nothing else.
BUILT_SOURCES = libelf.h
CLEANFILES += libelf.h
libelf.h: $(top_srcdir)/libelf/libelf.h
	cp $< $@
if !INSTALL_ELFH
system_elf_libelf_test_CPPFLAGS =
system_elf_gelf_test_CPPFLAGS = -I.
else
system_elf_libelf_test_CPPFLAGS = -I$(top_srcdir)/libelf
system_elf_gelf_test_CPPFLAGS = -I$(top_srcdir)/libelf
endif
system_elf_libelf_test_LDADD = $(libelf)
system_elf_gelf_test_LDADD = $(libelf)

# A lock file used to make sure only one test dumps core at a time
CLEANFILES += core-dump-backtrace.lock

if GCOV
check: check-am coverage
.PHONY: coverage
coverage:
	-$(srcdir)/coverage.sh
endif
