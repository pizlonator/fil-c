#!/usr/bin/tclsh

# ------------------------------------------------------------------------
#
# timer-event.perf.tcl --
#
#  This file provides performance tests for comparison of tcl-speed
#  of timer events (event-driven tcl-handling).
#
# ------------------------------------------------------------------------
#
# Copyright (c) 2014 Serg G. Brester (aka sebres)
#
# See the file "license.terms" for information on usage and redistribution
# of this file.
#


if {![namespace exists ::tclTestPerf]} {
  source -encoding utf-8 [file join [file dirname [info script]] test-performance.tcl]
}


namespace eval ::tclTestPerf-Timer-Event {

namespace path {::tclTestPerf}

proc test-queue {{reptime {1000 10000}}} {

  set howmuch [lindex $reptime 1]

  # because of extremely short measurement times by tests below, wait a little bit (warming-up),
  # to minimize influence of the time-gradation (just for better dispersion resp. result-comparison)
  timerate {after 0} 156

  puts "*** up to $howmuch events ***"
  # single iteration by update, so using -no-result (measure only):
  _test_run -no-result $reptime [string map [list \{*\}\$reptime $reptime \$howmuch $howmuch \\# \#] {
    # generate up to $howmuch idle-events:
    {after idle {set foo bar}}
    # update / after idle:
    {update; if {![llength [after info]]} break}

    # generate up to $howmuch idle-events:
    {after idle {set foo bar}}
    # update idletasks / after idle:
    {update idletasks; if {![llength [after info]]} break}

    # generate up to $howmuch immediate events:
    {after 0 {set foo bar}}
    # update / after 0:
    {update; if {![llength [after info]]} break}

    # generate up to $howmuch 1-ms events:
    {after 1 {set foo bar}}
    setup {after 1}
    # update / after 1:
    {update; if {![llength [after info]]} break}

    # generate up to $howmuch immediate events (+ 1 event of the second generation):
    {after 0 {after 0 {}}}
    # update / after 0 (double generation):
    {update; if {![llength [after info]]} break}

    # cancel forwards "after idle" / $howmuch idle-events in queue:
    setup {set i 0; timerate {set ev([incr i]) [after idle {set foo bar}]} {*}$reptime}
    setup {set le $i; set i 0; list 1 .. $le; # cancel up to $howmuch events}
    {after cancel $ev([incr i]); if {$i >= $le} break}
    cleanup {update; unset -nocomplain ev}
    # cancel backwards "after idle" / $howmuch idle-events in queue:
    setup {set i 0; timerate {set ev([incr i]) [after idle {set foo bar}]} {*}$reptime}
    setup {set le $i; incr i; list $le .. 1; # cancel up to $howmuch events}
    {after cancel $ev([incr i -1]); if {$i <= 1} break}
    cleanup {update; unset -nocomplain ev}

    # cancel forwards "after 0" / $howmuch timer-events in queue:
    setup {set i 0; timerate {set ev([incr i]) [after 0 {set foo bar}]} {*}$reptime}
    setup {set le $i; set i 0; list 1 .. $le; # cancel up to $howmuch events}
    {after cancel $ev([incr i]); if {$i >= $le} break}
    cleanup {update; unset -nocomplain ev}
    # cancel backwards "after 0" / $howmuch timer-events in queue:
    setup {set i 0; timerate {set ev([incr i]) [after 0 {set foo bar}]} {*}$reptime}
    setup {set le $i; incr i; list $le .. 1; # cancel up to $howmuch events}
    {after cancel $ev([incr i -1]); if {$i <= 1} break}
    cleanup {update; unset -nocomplain ev}

    # end $howmuch events.
    cleanup {if [llength [after info]] {error "unexpected: [llength [after info]] events are still there."}}
  }]
}

proc test-access {{reptime {1000 5000}}} {
  set howmuch [lindex $reptime 1]

  _test_run $reptime [string map [list \{*\}\$reptime $reptime \$howmuch $howmuch] {
    # event random access: after idle + after info (by $howmuch events)
    setup {set i -1; timerate {set ev([incr i]) [after idle {}]} {*}$reptime}
    {after info $ev([expr {int(rand()*$i)}])}
    cleanup {update; unset -nocomplain ev}
    # event random access: after 0 + after info (by $howmuch events)
    setup {set i -1; timerate {set ev([incr i]) [after 0 {}]} {*}$reptime}
    {after info $ev([expr {int(rand()*$i)}])}
    cleanup {update; unset -nocomplain ev}

    # end $howmuch events.
    cleanup {if [llength [after info]] {error "unexpected: [llength [after info]] events are still there."}}
  }]
}

proc test-exec {{reptime 1000}} {
  _test_run $reptime {
    # after idle + after cancel
    {after cancel [after idle {set foo bar}]}
    # after 0 + after cancel
    {after cancel [after 0 {set foo bar}]}
    # after idle + update idletasks
    {after idle {set foo bar}; update idletasks}
    # after idle + update
    {after idle {set foo bar}; update}
    # immediate: after 0 + update
    {after 0 {set foo bar}; update}
    # delayed: after 1 + update
    {after 1 {set foo bar}; update}
    # empty update:
    {update}
    # empty update idle tasks:
    {update idletasks}

    # simple shortest sleep:
    {after 0}
  }
}

proc test-nrt-capability {{reptime 1000}} {
  _test_run $reptime {
    # comparison values:
    {after 0 {set a 5}; update}
    {after 0 {set a 5}; vwait a}

    # conditional vwait with very brief wait-time:
    {after 1 {set a timeout}; vwait a; expr {$::a ne "timeout" ? 1 : "0[unset ::a]"}}
    {after 0 {set a timeout}; vwait a; expr {$::a ne "timeout" ? 1 : "0[unset ::a]"}}
  }
}

proc test-long {{reptime 1000}} {
  _test_run $reptime {
    # in-between important event by amount of idle events:
    {time {after idle {after 30}} 10; after 1 {set important 1}; vwait important;}
    cleanup {foreach i [after info] {after cancel $i}}
    # in-between important event (of new generation) by amount of idle events:
    {time {after idle {after 30}} 10; after 1 {after 0 {set important 1}}; vwait important;}
    cleanup {foreach i [after info] {after cancel $i}}
  }
}

proc test {{reptime 1000}} {
  test-exec $reptime
  foreach howmuch {5000 50000} {
    test-access [list $reptime $howmuch]
  }
  test-nrt-capability $reptime
  test-long $reptime

  puts ""
  foreach howmuch { 10000 20000 40000 60000 } {
    test-queue [list $reptime $howmuch]
  }

  puts \n**OK**
}

}; # end of ::tclTestPerf-Timer-Event

# ------------------------------------------------------------------------

# if calling direct:
if {[info exists ::argv0] && [file tail $::argv0] eq [file tail [info script]]} {
  array set in {-time 500}
  array set in $argv
  ::tclTestPerf-Timer-Event::test $in(-time)
}
