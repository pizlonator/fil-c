#!/usr/bin/tclsh
# ------------------------------------------------------------------------
#
# test-performance.tcl --
#
#  This file provides common performance tests for comparison of tcl-speed
#  degradation by switching between branches.
#  (currently for clock ensemble only)
#
# ------------------------------------------------------------------------
#
# Copyright (c) 2014 Serg G. Brester (aka sebres)
#
# See the file "license.terms" for information on usage and redistribution
# of this file.
#

array set in {-time 500}
if {[info exists ::argv0] && [file tail $::argv0] eq [file tail [info script]]} {
  array set in $argv
}

## common test performance framework:
if {![namespace exists ::tclTestPerf]} {
  source -encoding utf-8 [file join [file dirname [info script]] test-performance.tcl]
}

namespace eval ::tclTestPerf-TclClock {

namespace path {::tclTestPerf}

## set testing defaults:
set ::env(TCL_TZ) :CET

# warm-up interpreter compiler env, clock platform-related features:

## warm-up test-related features (load clock.tcl, system zones, locales, etc.):
clock scan "" -gmt 1
clock scan ""
clock scan "" -timezone :CET
clock scan "" -format "" -locale en
clock scan "" -format "" -locale de

## ------------------------------------------

proc test-format {{reptime 1000}} {
  _test_run $reptime {
    # Format : short, week only (in gmt)
    {clock format 1482525936 -format "%u" -gmt 1}
    # Format : short, week only (system zone)
    {clock format 1482525936 -format "%u"}
    # Format : short, week only (CEST)
    {clock format 1482525936 -format "%u" -timezone :CET}
    # Format : date only (in gmt)
    {clock format 1482525936 -format "%Y-%m-%d" -gmt 1}
    # Format : date only (system zone)
    {clock format 1482525936 -format "%Y-%m-%d"}
    # Format : date only (CEST)
    {clock format 1482525936 -format "%Y-%m-%d" -timezone :CET}
    # Format : time only (in gmt)
    {clock format 1482525936 -format "%H:%M" -gmt 1}
    # Format : time only (system zone)
    {clock format 1482525936 -format "%H:%M"}
    # Format : time only (CEST)
    {clock format 1482525936 -format "%H:%M" -timezone :CET}
    # Format : time only (in gmt)
    {clock format 1482525936 -format "%H:%M:%S" -gmt 1}
    # Format : time only (system zone)
    {clock format 1482525936 -format "%H:%M:%S"}
    # Format : time only (CEST)
    {clock format 1482525936 -format "%H:%M:%S" -timezone :CET}
    # Format : default (in gmt)
    {clock format 1482525936 -gmt 1 -locale en}
    # Format : default (system zone)
    {clock format 1482525936 -locale en}
    # Format : default (CEST)
    {clock format 1482525936 -timezone :CET -locale en}
    # Format : ISO date-time (in gmt, numeric zone)
    {clock format 1246379400 -format "%Y-%m-%dT%H:%M:%S %z" -gmt 1}
    # Format : ISO date-time (system zone, CEST, numeric zone)
    {clock format 1246379400 -format "%Y-%m-%dT%H:%M:%S %z"}
    # Format : ISO date-time (CEST, numeric zone)
    {clock format 1246379400 -format "%Y-%m-%dT%H:%M:%S %z" -timezone :CET}
    # Format : ISO date-time (system zone, CEST)
    {clock format 1246379400 -format "%Y-%m-%dT%H:%M:%S %Z"}
    # Format : julian day with time (in gmt):
    {clock format 1246379415 -format "%J %H:%M:%S" -gmt 1}
    # Format : julian day with time (system zone):
    {clock format 1246379415 -format "%J %H:%M:%S"}

    # Format : locale date-time (en):
    {clock format 1246379415 -format "%x %X" -locale en}
    # Format : locale date-time (de):
    {clock format 1246379415 -format "%x %X" -locale de}

    # Format : locale lookup table month:
    {clock format 1246379400 -format "%b" -locale en -gmt 1}
    # Format : locale lookup 2 tables - month and day:
    {clock format 1246379400 -format "%b %Od" -locale en -gmt 1}
    # Format : locale lookup 3 tables - week, month and day:
    {clock format 1246379400 -format "%a %b %Od" -locale en -gmt 1}
    # Format : locale lookup 4 tables - week, month, day and year:
    {clock format 1246379400 -format "%a %b %Od %Oy" -locale en -gmt 1}

    # Format : dynamic clock value (without converter caches):
    setup {set i 0}
    {clock format [incr i] -format "%Y-%m-%dT%H:%M:%S" -locale en -timezone :CET}
    cleanup {puts [clock format $i -format "%Y-%m-%dT%H:%M:%S" -locale en -timezone :CET]}
    # Format : dynamic clock value (without any converter caches, zone range overflow):
    setup {set i 0}
    {clock format [incr i 86400] -format "%Y-%m-%dT%H:%M:%S" -locale en -timezone :CET}
    cleanup {puts [clock format $i -format "%Y-%m-%dT%H:%M:%S" -locale en -timezone :CET]}

    # Format : dynamic format (cacheable)
    {clock format 1246379415 -format [string trim "%d.%m.%Y %H:%M:%S "] -gmt 1}

    # Format : all (in gmt, locale en)
    {clock format 1482525936 -format "%%a = %a | %%A = %A | %%b = %b | %%h = %h | %%B = %B | %%C = %C | %%d = %d | %%e = %e | %%g = %g | %%G = %G | %%H = %H | %%I = %I | %%j = %j | %%J = %J | %%k = %k | %%l = %l | %%m = %m | %%M = %M | %%N = %N | %%p = %p | %%P = %P | %%Q = %Q | %%s = %s | %%S = %S | %%t = %t | %%u = %u | %%U = %U | %%V = %V | %%w = %w | %%W = %W | %%y = %y | %%Y = %Y | %%z = %z | %%Z = %Z | %%n = %n | %%EE = %EE | %%EC = %EC | %%Ey = %Ey | %%n = %n | %%Od = %Od | %%Oe = %Oe | %%OH = %OH | %%Ok = %Ok | %%OI = %OI | %%Ol = %Ol | %%Om = %Om | %%OM = %OM | %%OS = %OS | %%Ou = %Ou | %%Ow = %Ow | %%Oy = %Oy" -gmt 1 -locale en}
    # Format : all (in CET, locale de)
    {clock format 1482525936 -format "%%a = %a | %%A = %A | %%b = %b | %%h = %h | %%B = %B | %%C = %C | %%d = %d | %%e = %e | %%g = %g | %%G = %G | %%H = %H | %%I = %I | %%j = %j | %%J = %J | %%k = %k | %%l = %l | %%m = %m | %%M = %M | %%N = %N | %%p = %p | %%P = %P | %%Q = %Q | %%s = %s | %%S = %S | %%t = %t | %%u = %u | %%U = %U | %%V = %V | %%w = %w | %%W = %W | %%y = %y | %%Y = %Y | %%z = %z | %%Z = %Z | %%n = %n | %%EE = %EE | %%EC = %EC | %%Ey = %Ey | %%n = %n | %%Od = %Od | %%Oe = %Oe | %%OH = %OH | %%Ok = %Ok | %%OI = %OI | %%Ol = %Ol | %%Om = %Om | %%OM = %OM | %%OS = %OS | %%Ou = %Ou | %%Ow = %Ow | %%Oy = %Oy" -timezone :CET -locale de}
  }
}

proc test-scan {{reptime 1000}} {
  _test_run -convert-result {clock format $_(r) -locale en} $reptime {
    # Scan : date (in gmt)
    {clock scan "25.11.2015" -format "%d.%m.%Y" -base 0 -gmt 1}
    # Scan : date (system time zone, with base)
    {clock scan "25.11.2015" -format "%d.%m.%Y" -base 0}
    # Scan : date (system time zone, without base)
    {clock scan "25.11.2015" -format "%d.%m.%Y"}
    # Scan : greedy match
    {clock scan "111" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "1111" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "11111" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "111111" -format "%d%m%y" -base 0 -gmt 1}
    # Scan : greedy match (space separated)
    {clock scan "1 1 1" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "111 1" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "1 111" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "1 11 1" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "1 11 11" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "11 11 11" -format "%d%m%y" -base 0 -gmt 1}

    # Scan : time (in gmt)
    {clock scan "10:35:55" -format "%H:%M:%S" -base 1000000000 -gmt 1}
    # Scan : time (system time zone, with base)
    {clock scan "10:35:55" -format "%H:%M:%S" -base 1000000000}
    # Scan : time (gmt, without base)
    {clock scan "10:35:55" -format "%H:%M:%S" -gmt 1}
    # Scan : time (system time zone, without base)
    {clock scan "10:35:55" -format "%H:%M:%S"}

    # Scan : date-time (in gmt)
    {clock scan "25.11.2015 10:35:55" -format "%d.%m.%Y %H:%M:%S" -base 0 -gmt 1}
    # Scan : date-time (system time zone with base)
    {clock scan "25.11.2015 10:35:55" -format "%d.%m.%Y %H:%M:%S" -base 0}
    # Scan : date-time (system time zone without base)
    {clock scan "25.11.2015 10:35:55" -format "%d.%m.%Y %H:%M:%S"}

    # Scan : julian day in gmt
    {clock scan 2451545 -format %J -gmt 1}
    # Scan : julian day in system TZ
    {clock scan 2451545 -format %J}
    # Scan : julian day in other TZ
    {clock scan 2451545 -format %J -timezone +0200}
    # Scan : julian day with time:
    {clock scan "2451545 10:20:30" -format "%J %H:%M:%S"}
    # Scan : julian day with time (greedy match):
    {clock scan "2451545 102030" -format "%J%H%M%S"}

    # Scan : century, lookup table month
    {clock scan {1970 Jan 2} -format {%C%y %b %d} -locale en -gmt 1}
    # Scan : century, lookup table month and day (both entries are first)
    {clock scan {1970 Jan 01} -format {%C%y %b %Od} -locale en -gmt 1}
    # Scan : century, lookup table month and day (list scan: entries with position 12 / 31)
    {clock scan {2016 Dec 31} -format {%C%y %b %Od} -locale en -gmt 1}

    # Scan : ISO date-time (CEST)
    {clock scan "2009-06-30T18:30:00+02:00" -format "%Y-%m-%dT%H:%M:%S%z"}
    {clock scan "2009-06-30T18:30:00 CEST" -format "%Y-%m-%dT%H:%M:%S %z"}
    # Scan : ISO date-time (UTC)
    {clock scan "2009-06-30T18:30:00Z" -format "%Y-%m-%dT%H:%M:%S%z"}
    {clock scan "2009-06-30T18:30:00 UTC" -format "%Y-%m-%dT%H:%M:%S %z"}

    # Scan : locale date-time (en):
    {clock scan "06/30/2009 18:30:15" -format "%x %X" -gmt 1 -locale en}
    # Scan : locale date-time (de):
    {clock scan "30.06.2009 18:30:15" -format "%x %X" -gmt 1 -locale de}

    # Scan : dynamic format (cacheable)
    {clock scan "25.11.2015 10:35:55" -format [string trim "%d.%m.%Y %H:%M:%S "] -base 0 -gmt 1}

    break
    # # Scan : long format test (allock chain)
    # {clock scan "25.11.2015" -format "%d.%m.%Y %d.%m.%Y %d.%m.%Y %d.%m.%Y %d.%m.%Y %d.%m.%Y %d.%m.%Y %d.%m.%Y" -base 0 -gmt 1}
    # # Scan : dynamic, very long format test (create obj representation, allock chain, GC, etc):
    # {clock scan "25.11.2015" -format [string repeat "[incr i] %d.%m.%Y %d.%m.%Y" 10] -base 0 -gmt 1}
    # # Scan : again:
    # {clock scan "25.11.2015" -format [string repeat "[incr i -1] %d.%m.%Y %d.%m.%Y" 10] -base 0 -gmt 1}
  }
}

proc test-freescan {{reptime 1000}} {
  _test_run -convert-result {clock format $_(r) -locale en}  $reptime {
    # FreeScan : relative date
    {clock scan "5 years 18 months 385 days" -base 0 -gmt 1}
    # FreeScan : relative date with relative weekday
    {clock scan "5 years 18 months 385 days Fri" -base 0 -gmt 1}
    # FreeScan : relative date with ordinal month
    {clock scan "5 years 18 months 385 days next 1 January" -base 0 -gmt 1}
    # FreeScan : relative date with ordinal month and relative weekday
    {clock scan "5 years 18 months 385 days next January Fri" -base 0 -gmt 1}
    # FreeScan : ordinal month
    {clock scan "next January" -base 0 -gmt 1}
    # FreeScan : relative week
    {clock scan "next Fri" -base 0 -gmt 1}
    # FreeScan : relative weekday and week offset
    {clock scan "next January + 2 week" -base 0 -gmt 1}
    # FreeScan : time only with base
    {clock scan "19:18:30" -base 148863600 -gmt 1}
    # FreeScan : time only without base, gmt
    {clock scan "19:18:30" -gmt 1}
    # FreeScan : time only without base, system
    {clock scan "19:18:30"}
    # FreeScan : date, system time zone
    {clock scan "05/08/2016 20:18:30"}
    # FreeScan : date, supplied time zone
    {clock scan "05/08/2016 20:18:30" -timezone :CET}
    # FreeScan : date, supplied gmt (equivalent -timezone :GMT)
    {clock scan "05/08/2016 20:18:30" -gmt 1}
    # FreeScan : date, supplied time zone gmt
    {clock scan "05/08/2016 20:18:30" -timezone :GMT}
    # FreeScan : time only, numeric zone in string, base time gmt (exchange zones between gmt / -0500)
    {clock scan "20:18:30 -0500" -base 148863600 -gmt 1}
    # FreeScan : time only, zone in string (exchange zones between system / gmt)
    {clock scan "19:18:30 GMT" -base 148863600}
    # FreeScan : fast switch of zones in cycle - GMT, MST, CET (system) and EST
    {clock scan "19:18:30 MST" -base 148863600 -gmt 1
     clock scan "19:18:30 EST" -base 148863600
    }
  }
}

proc test-add {{reptime 1000}} {
  set tests {
    # Add : years
    {clock add 1246379415 5 years -gmt 1}
    # Add : months
    {clock add 1246379415 18 months -gmt 1}
    # Add : weeks
    {clock add 1246379415 20 weeks -gmt 1}
    # Add : days
    {clock add 1246379415 385 days -gmt 1}
    # Add : weekdays
    {clock add 1246379415 3 weekdays -gmt 1}

    # Add : hours
    {clock add 1246379415 5 hours -gmt 1}
    # Add : minutes
    {clock add 1246379415 55 minutes -gmt 1}
    # Add : seconds
    {clock add 1246379415 100 seconds -gmt 1}

    # Add : +/- in gmt
    {clock add 1246379415 -5 years +21 months -20 weeks +386 days -19 hours +30 minutes -10 seconds -gmt 1}
    # Add : +/- in system timezone
    {clock add 1246379415 -5 years +21 months -20 weeks +386 days -19 hours +30 minutes -10 seconds -timezone :CET}

    # Add : gmt
    {clock add 1246379415 -5 years 18 months 366 days 5 hours 30 minutes 10 seconds -gmt 1}
    # Add : system timezone
    {clock add 1246379415 -5 years 18 months 366 days 5 hours 30 minutes 10 seconds -timezone :CET}

    # Add : all in gmt
    {clock add 1246379415 4 years 18 months 50 weeks 378 days 3 weekdays 5 hours 30 minutes 10 seconds -gmt 1}
    # Add : all in system timezone
    {clock add 1246379415 4 years 18 months 50 weeks 378 days 3 weekdays 5 hours 30 minutes 10 seconds -timezone :CET}

  }
  # if does not support add of weekdays:
  if {[catch {clock add 0 3 weekdays -gmt 1}]} {
    regsub -all {\mweekdays\M} $tests "days" tests
  }
  _test_run -convert-result {clock format $_(r) -locale en} $reptime $tests
}

proc test-convert {{reptime 1000}} {
  _test_run $reptime {
    # Convert locale (en -> de):
    {clock format [clock scan "Tue May 30 2017" -format "%a %b %d %Y" -gmt 1 -locale en] -format "%a %b %d %Y" -gmt 1 -locale de}
    # Convert locale (de -> en):
    {clock format [clock scan "Di Mai 30 2017" -format "%a %b %d %Y" -gmt 1 -locale de] -format "%a %b %d %Y" -gmt 1 -locale en}

    # Convert TZ: direct
    {clock format [clock scan "19:18:30" -base 148863600 -timezone EST] -timezone MST}
    {clock format [clock scan "19:18:30" -base 148863600 -timezone MST] -timezone EST}
    # Convert TZ: included in scan string & format
    {clock format [clock scan "19:18:30 EST" -base 148863600] -format "%H:%M:%S %z" -timezone MST}
    {clock format [clock scan "19:18:30 EST" -base 148863600] -format "%H:%M:%S %z" -timezone EST}

    # Format locale 1x: comparison values
    {clock format 0 -gmt 1 -locale en}
    {clock format 0 -gmt 1 -locale de}
    {clock format 0 -gmt 1 -locale fr}
    # Format locale 2x: without switching locale (en, en)
    {clock format 0 -gmt 1 -locale en; clock format 0 -gmt 1 -locale en}
    # Format locale 2x: with switching locale (en, de)
    {clock format 0 -gmt 1 -locale en; clock format 0 -gmt 1 -locale de}
    # Format locale 3x: without switching locale (en, en, en)
    {clock format 0 -gmt 1 -locale en; clock format 0 -gmt 1 -locale en; clock format 0 -gmt 1 -locale en}
    # Format locale 3x: with switching locale (en, de, fr)
    {clock format 0 -gmt 1 -locale en; clock format 0 -gmt 1 -locale de; clock format 0 -gmt 1 -locale fr}

    # Scan locale 2x: without switching locale (en, en) + (de, de)
    {clock scan "Tue May 30 2017" -format "%a %b %d %Y" -gmt 1 -locale en; clock scan "Tue May 30 2017" -format "%a %b %d %Y" -gmt 1 -locale en}
    {clock scan "Di Mai 30 2017" -format "%a %b %d %Y" -gmt 1 -locale de; clock scan "Di Mai 30 2017" -format "%a %b %d %Y" -gmt 1 -locale de}
    # Scan locale 2x: with switching locale (en, de)
    {clock scan "Tue May 30 2017" -format "%a %b %d %Y" -gmt 1 -locale en; clock scan "Di Mai 30 2017" -format "%a %b %d %Y" -gmt 1 -locale de}
    # Scan locale 3x: with switching locale (en, de, fr)
    {clock scan "Tue May 30 2017" -format "%a %b %d %Y" -gmt 1 -locale en; clock scan "Di Mai 30 2017" -format "%a %b %d %Y" -gmt 1 -locale de; clock scan "mar. mai 30 2017" -format "%a %b %d %Y" -gmt 1 -locale fr}

    # Format TZ 2x: comparison values
    {clock format 0 -timezone CET -format "%Y-%m-%d %H:%M:%S %z"}
    {clock format 0 -timezone EST -format "%Y-%m-%d %H:%M:%S %z"}
    # Format TZ 2x: without switching
    {clock format 0 -timezone CET -format "%Y-%m-%d %H:%M:%S %z"; clock format 0 -timezone CET -format "%Y-%m-%d %H:%M:%S %z"}
    {clock format 0 -timezone EST -format "%Y-%m-%d %H:%M:%S %z"; clock format 0 -timezone EST -format "%Y-%m-%d %H:%M:%S %z"}
    # Format TZ 2x: with switching
    {clock format 0 -timezone CET -format "%Y-%m-%d %H:%M:%S %z"; clock format 0 -timezone EST -format "%Y-%m-%d %H:%M:%S %z"}
    # Format TZ 3x: with switching (CET, EST, MST)
    {clock format 0 -timezone CET -format "%Y-%m-%d %H:%M:%S %z"; clock format 0 -timezone EST -format "%Y-%m-%d %H:%M:%S %z"; clock format 0 -timezone MST -format "%Y-%m-%d %H:%M:%S %z"}
    # Format TZ 3x: with switching (GMT, EST, MST)
    {clock format 0 -gmt 1 -format "%Y-%m-%d %H:%M:%S %z"; clock format 0 -timezone EST -format "%Y-%m-%d %H:%M:%S %z"; clock format 0 -timezone MST -format "%Y-%m-%d %H:%M:%S %z"}

    # FreeScan TZ 2x (+1 system-default): without switching TZ
    {clock scan "19:18:30 MST" -base 148863600; clock scan "19:18:30 MST" -base 148863600}
    {clock scan "19:18:30 EST" -base 148863600; clock scan "19:18:30 EST" -base 148863600}
    # FreeScan TZ 2x (+1 system-default): with switching TZ
    {clock scan "19:18:30 MST" -base 148863600; clock scan "19:18:30 EST" -base 148863600}
    # FreeScan TZ 2x (+1 gmt, +1 system-default)
    {clock scan "19:18:30 MST" -base 148863600 -gmt 1; clock scan "19:18:30 EST" -base 148863600}

    # Scan TZ: comparison included in scan string vs. given
    {clock scan "2009-06-30T18:30:00 CEST" -format "%Y-%m-%dT%H:%M:%S %z"}
    {clock scan "2009-06-30T18:30:00 CET" -format "%Y-%m-%dT%H:%M:%S %z"}
    {clock scan "2009-06-30T18:30:00" -timezone CET -format "%Y-%m-%dT%H:%M:%S"}
  }
}

proc test-other {{reptime 1000}} {
  _test_run $reptime {
    # Bad zone
    {catch {clock scan "1 day" -timezone BAD_ZONE -locale en}}

    # Scan : julian day (overflow)
    {catch {clock scan 5373485 -format %J}}

    # Scan : test rotate of GC objects (format is dynamic, so tcl-obj removed with last reference)
    {set i 0; time { clock scan "[incr i] - 25.11.2015" -format "$i - %d.%m.%Y" -base 0 -gmt 1 } 50}
    # Scan : test reusability of GC objects (format is dynamic, so tcl-obj removed with last reference)
    {set i 50; time { clock scan "[incr i -1] - 25.11.2015" -format "$i - %d.%m.%Y" -base 0 -gmt 1 } 50}
  }
}

proc test-ensemble-perf {{reptime 1000}} {
  _test_run $reptime {
    # Clock clicks (ensemble)
    {clock clicks}
    # Clock clicks (direct)
    {::tcl::clock::clicks}
    # Clock seconds (ensemble)
    {clock seconds}
    # Clock seconds (direct)
    {::tcl::clock::seconds}
    # Clock microseconds (ensemble)
    {clock microseconds}
    # Clock microseconds (direct)
    {::tcl::clock::microseconds}
    # Clock scan (ensemble)
    {clock scan ""}
    # Clock scan (direct)
    {::tcl::clock::scan ""}
    # Clock format (ensemble)
    {clock format 0 -f %s}
    # Clock format (direct)
    {::tcl::clock::format 0 -f %s}
  }
}

proc test {{reptime 1000}} {
  puts ""
  test-ensemble-perf [expr {$reptime / 2}]; #fast enough
  test-format $reptime
  test-scan $reptime
  test-freescan $reptime
  test-add $reptime
  test-convert [expr {$reptime / 2}]; #fast enough
  test-other $reptime

  puts \n**OK**
}

}; # end of ::tclTestPerf-TclClock

# ------------------------------------------------------------------------

# if calling direct:
if {[info exists ::argv0] && [file tail $::argv0] eq [file tail [info script]]} {
  ::tclTestPerf-TclClock::test $in(-time)
}
