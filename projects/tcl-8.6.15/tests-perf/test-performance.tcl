# ------------------------------------------------------------------------
#
# test-performance.tcl --
#
#  This file provides common performance tests for comparison of tcl-speed
#  degradation or regression by switching between branches.
#
#  To execute test case evaluate direct corresponding file "tests-perf\*.perf.tcl".
#
# ------------------------------------------------------------------------
#
# Copyright (c) 2014 Serg G. Brester (aka sebres)
#
# See the file "license.terms" for information on usage and redistribution
# of this file.
#

namespace eval ::tclTestPerf {
# warm-up interpreter compiler env, calibrate timerate measurement functionality:

# if no timerate here - import from unsupported:
if {[namespace which -command timerate] eq {}} {
  namespace inscope ::tcl::unsupported {namespace export timerate}
  namespace import ::tcl::unsupported::timerate
}

# if not yet calibrated:
if {[lindex [timerate {} 10] 6] >= (10-1)} {
  puts -nonewline "Calibration ... "; flush stdout
  puts "done: [lrange \
    [timerate -calibrate {}] \
  0 1]"
}

proc {**STOP**} {args} {
  return -code error -level 4 "**STOP** in [info level [expr {[info level]-2}]] [join $args { }]"
}

proc _test_get_commands {lst} {
  regsub -all {(?:^|\n)[ \t]*(\#[^\n]*|\msetup\M[^\n]*|\mcleanup\M[^\n]*)(?=\n\s*(?:[\{\#]|setup|cleanup|$))} $lst "\n{\\1}"
}

proc _test_out_total {} {
  upvar _ _

  set tcnt [llength $_(itm)]
  if {!$tcnt} {
    puts ""
    return
  }

  set mintm 0x7fffffff
  set maxtm 0
  set nettm 0
  set wtm 0
  set wcnt 0
  set i 0
  foreach tm $_(itm) {
    if {[llength $tm] > 6} {
      set nettm [expr {$nettm + [lindex $tm 6]}]
    }
    set wtm [expr {$wtm + [lindex $tm 0]}]
    set wcnt [expr {$wcnt + [lindex $tm 2]}]
    set tm [lindex $tm 0]
    if {$tm > $maxtm} {set maxtm $tm; set maxi $i}
    if {$tm < $mintm} {set mintm $tm; set mini $i}
    incr i
  }

  puts [string repeat ** 40]
  set s [format "%d cases in %.2f sec." $tcnt [expr {([clock milliseconds] - $_(starttime)) / 1000.0}]]
  if {$nettm > 0} {
    append s [format " (%.2f net-sec.)" [expr {$nettm / 1000.0}]]
  }
  puts "Total $s:"
  lset _(m) 0 [format %.6f $wtm]
  lset _(m) 2 $wcnt
  lset _(m) 4 [format %.3f [expr {$wcnt / (($nettm ? $nettm : ($tcnt * [lindex $_(reptime) 0])) / 1000.0)}]]
  if {[llength $_(m)] > 6} {
    lset _(m) 6 [format %.3f $nettm]
  }
  puts $_(m)
  puts "Average:"
  lset _(m) 0 [format %.6f [expr {[lindex $_(m) 0] / $tcnt}]]
  lset _(m) 2 [expr {[lindex $_(m) 2] / $tcnt}]
  if {[llength $_(m)] > 6} {
    lset _(m) 6 [format %.3f [expr {[lindex $_(m) 6] / $tcnt}]]
    lset _(m) 4 [format %.0f [expr {[lindex $_(m) 2] / [lindex $_(m) 6] * 1000}]]
  }
  puts $_(m)
  puts "Min:"
  puts [lindex $_(itm) $mini]
  puts "Max:"
  puts [lindex $_(itm) $maxi]
  puts [string repeat ** 40]
  puts ""
  unset -nocomplain _(itm) _(starttime)
}

proc _test_start {reptime} {
  upvar _ _
  array set _ [list itm {} reptime $reptime starttime [clock milliseconds] -from-run 0]
}

proc _test_iter {args} {
  if {[llength $args] > 2} {
    return -code error "wrong # args: should be \"[lindex [info level [info level]] 0] ?level? measure-result\""
  }
  set lvl 1
  if {[llength $args] > 1} {
    set args [lassign $args lvl]
  }
  upvar $lvl _ _
  puts [set _(m) {*}$args]
  lappend _(itm) $_(m)
  puts ""
}

proc _adjust_maxcount {reptime maxcount} {
  if {[llength $reptime] > 1} {
    lreplace $reptime 1 1 [expr {min($maxcount,[lindex $reptime 1])}]
  } else {
    lappend reptime $maxcount
  }
}

proc _test_run {args} {
  upvar _ _
  # parse args:
  array set _ {-no-result 0 -uplevel 0 -convert-result {}}
  while {[llength $args] > 2} {
    if {![info exists _([set o [lindex $args 0]])]} {
      break
    }
    if {[string is boolean -strict $_($o)]} {
      set _($o) [expr {! $_($o)}]
      set args [lrange $args 1 end]
    } else {
      if {[llength $args] <= 2} {
        return -code error "value expected for option $o"
      }
      set _($o) [lindex $args 1]
      set args [lrange $args 2 end]
    }
  }
  unset -nocomplain o
  if {[llength $args] < 2 || [llength $args] > 3} {
    return -code error "wrong # args: should be \"[lindex [info level [info level]] 0] ?-no-result? reptime lst ?outcmd?\""
  }
  set _(outcmd) {puts}
  set args [lassign $args reptime lst]
  if {[llength $args]} {
    set _(outcmd) [lindex $args 0]
  }
  # avoid output if only once:
  if {[lindex $reptime 0] <= 1 || ([llength $reptime] > 1 && [lindex $reptime 1] == 1)} {
    set _(-no-result) 1
  }
  if {![info exists _(itm)]} {
    array set _ [list itm {} reptime $reptime starttime [clock milliseconds] -from-run 1]
  } else {
    array set _ [list reptime $reptime]
  }

  # process measurement:
  foreach _(c) [_test_get_commands $lst] {
    {*}$_(outcmd) "% [regsub -all {\n[ \t]*} $_(c) {; }]"
    if {[regexp {^\s*\#} $_(c)]} continue
    if {[regexp {^\s*(?:setup|cleanup)\s+} $_(c)]} {
      set _(c) [lindex $_(c) 1]
      if {$_(-uplevel)} {
        set _(c) [list uplevel 1 $_(c)]
      }
      {*}$_(outcmd) [if 1 $_(c)]
      continue
    }
    if {$_(-uplevel)} {
      set _(c) [list uplevel 1 $_(c)]
    }
    set _(ittime) $_(reptime)
    # if output result (and not once):
    if {!$_(-no-result)} {
      set _(r) [if 1 $_(c)]
      if {$_(-convert-result) ne ""} { set _(r) [if 1 $_(-convert-result)] }
      {*}$_(outcmd) $_(r)
      if {[llength $_(ittime)] > 1} { # decrement max-count
        lset _(ittime) 1 [expr {[lindex $_(ittime) 1] - 1}]
      }
    }
    {*}$_(outcmd) [set _(m) [timerate $_(c) {*}$_(ittime)]]
    lappend _(itm) $_(m)
    {*}$_(outcmd) ""
  }
  if {$_(-from-run)} {
    _test_out_total
  }
}

}; # end of namespace ::tclTestPerf
