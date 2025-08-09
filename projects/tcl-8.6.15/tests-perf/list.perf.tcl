#!/usr/bin/tclsh

# ------------------------------------------------------------------------
#
# list.perf.tcl --
#
#  This file provides performance tests for comparison of tcl-speed
#  of list facilities.
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


namespace eval ::tclTestPerf-List {

namespace path {::tclTestPerf}

proc test-lsearch-regress {{reptime 1000}} {
  _test_run -no-result $reptime {
    # found-first immediately, list with 5000 strings with ca. 50 chars elements:
    setup   { set str [join [lrepeat 13 "XXX"] /]; set l [lrepeat 5000 $str]; llength $l }

    { lsearch $l $str }
    { lsearch -glob $l $str }
    { lsearch -exact $l $str }
    { lsearch -dictionary $l $str }
    { lsearch -exact -dictionary $l $str }

    { lsearch -nocase $l $str }
    { lsearch -nocase -glob $l $str }
    { lsearch -nocase -exact $l $str }
    { lsearch -nocase -dictionary $l $str }
    { lsearch -nocase -exact -dictionary $l $str }
  }
}

proc test-lsearch-nf-regress {{reptime 1000}} {
  _test_run -no-result $reptime {
    # not-found, list with 5000 strings with ca. 50 chars elements:
    setup   { set str [join [lrepeat 13 "XXX"] /]; set sNF $str/NF; set l [lrepeat 5000 $str]; llength $l }

    { lsearch $l $sNF }
    { lsearch -glob $l $sNF }
    { lsearch -exact $l $sNF }
    { lsearch -dictionary $l $sNF }
    { lsearch -exact -dictionary $l $sNF }
    { lsearch -sorted $l $sNF }
    { lsearch -bisect $l $sNF }

    { lsearch -nocase $l $sNF }
    { lsearch -nocase -glob $l $sNF }
    { lsearch -nocase -exact $l $sNF }
    { lsearch -nocase -dictionary $l $sNF }
    { lsearch -nocase -exact -dictionary $l $sNF }
    { lsearch -nocase -sorted $l $sNF }
    { lsearch -nocase -bisect $l $sNF }
  }
}

proc test-lsearch-nf-non-opti-fast {{reptime 1000}} {
  _test_run -no-result $reptime {
    # not-found, list with 5000 strings with ca. 50 chars elements:
    setup   { set str [join [lrepeat 13 "XXX"] /]; set sNF "$str/*"; set l [lrepeat 5000 $str]; llength $l }

    { lsearch -sorted -dictionary $l $sNF }
    { lsearch -bisect -dictionary $l $sNF }

    { lsearch -sorted -nocase -dictionary $l $sNF }
    { lsearch -bisect -nocase -dictionary $l $sNF }

  }
}

proc test-lsearch-nf-non-opti-slow {{reptime 1000}} {
  _test_run -no-result $reptime {
    # not-found, list with 5000 strings with ca. 50 chars elements:
    setup   { set str [join [lrepeat 13 "XXX"] /]; set sNF "$str/*"; set l [lrepeat 5000 $str]; llength $l }

    { lsearch $l $sNF }
    { lsearch -glob $l $sNF }

    { lsearch -nocase $l $sNF }
    { lsearch -nocase -glob $l $sNF }

  }
}

proc test {{reptime 1000}} {
  test-lsearch-regress $reptime
  test-lsearch-nf-regress $reptime
  test-lsearch-nf-non-opti-fast $reptime
  test-lsearch-nf-non-opti-slow $reptime

  puts \n**OK**
}

}; # end of ::tclTestPerf-List

# ------------------------------------------------------------------------

# if calling direct:
if {[info exists ::argv0] && [file tail $::argv0] eq [file tail [info script]]} {
  array set in {-time 500}
  array set in $argv
  ::tclTestPerf-List::test $in(-time)
}
