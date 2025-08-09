#!/usr/bin/tclsh

# ------------------------------------------------------------------------
#
# chan.perf.tcl --
#
#  This file provides performance tests for comparison of tcl-speed
#  of channel subsystem.
#
# ------------------------------------------------------------------------
#
# Copyright (c) 2024 Serg G. Brester (aka sebres)
#
# See the file "license.terms" for information on usage and redistribution
# of this file.
#


if {![namespace exists ::tclTestPerf]} {
  source -encoding utf-8 [file join [file dirname [info script]] test-performance.tcl]
}


namespace eval ::tclTestPerf-Chan {

namespace path {::tclTestPerf}

proc _get_test_chan {{bufSize 4096}} {
  lassign [chan pipe] ch wch;
  fconfigure $ch -translation lf -encoding utf-8 -buffersize $bufSize -buffering full
  fconfigure $wch -translation lf -encoding utf-8 -buffersize $bufSize -buffering full

  exec [info nameofexecutable] -- $bufSize >@$wch << {
    set bufSize [lindex $::argv end]
    fconfigure stdout -translation lf -encoding utf-8 -buffersize $bufSize -buffering full
    set buf [string repeat test 1000]; # 4K
    # write ~ 10*1M + 10*2M + 10*10M + 1*20M:
    set i 0; while {$i < int((10*1e6 + 10*2e6 + 10*10e6 + 1*20e6)/4e3)} {
      #puts -nonewline stdout $i\t
      puts stdout $buf
      #flush stdout; # don't flush to use full buffer
      incr i
    }
  } &
  close $wch
  return $ch
}

# regression tests for [bug-da16d15574] (fix for [db4f2843cd]):
proc test-read-regress {{reptime {50000 10}}} {
  _test_run -no-result $reptime {
    # with 4KB buffersize:
    setup   { set ch [::tclTestPerf-Chan::_get_test_chan 4096]; fconfigure $ch -buffersize }
    # 10 * 1M:
    {read $ch [expr {int(1e6)}]}
    # 10 * 2M:
    {read $ch [expr {int(2e6)}]}
    # 10 * 10M:
    {read $ch [expr {int(10e6)}]}
    #  1 * 20M:
    {read $ch; break}
    cleanup { close $ch }

    # with 1MB buffersize:
    setup   { set ch [::tclTestPerf-Chan::_get_test_chan 1048576]; fconfigure $ch -buffersize }
    # 10 * 1M:
    {read $ch [expr {int(1e6)}]}
    # 10 * 2M:
    {read $ch [expr {int(2e6)}]}
    # 10 * 10M:
    {read $ch [expr {int(10e6)}]}
    #  1 * 20M:
    {read $ch; break}
    cleanup { close $ch }
  }
}

proc test {{reptime 1000}} {
  test-read-regress

  puts \n**OK**
}

}; # end of ::tclTestPerf-Chan

# ------------------------------------------------------------------------

# if calling direct:
if {[info exists ::argv0] && [file tail $::argv0] eq [file tail [info script]]} {
  array set in {-time 500}
  array set in $argv
  ::tclTestPerf-Chan::test $in(-time)
}
