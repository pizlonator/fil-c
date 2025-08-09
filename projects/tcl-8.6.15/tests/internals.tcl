# This file contains internal facilities for Tcl tests.
#
# Source this file in the related tests to include from tcl-tests:
#
#   source -encoding utf-8 [file join [file dirname [info script]] internals.tcl]
#
# Copyright (c) 2020 Sergey G. Brester (sebres).
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

if {[namespace which -command ::tcltest::internals::scriptpath] eq ""} {namespace eval ::tcltest::internals {

namespace path ::tcltest

::tcltest::ConstraintInitializer testWithLimit { expr {[testConstraint macOrUnix] && ![catch { exec prlimit --version }]} }

# test-with-limit --
#
# Usage: test-with-limit ?-addmem bytes? ?-maxmem bytes? command
# Options:
#	-addmem - set additional memory limit (in bytes) as difference (extra memory needed to run a test)
#	-maxmem - set absolute maximum address space limit (in bytes)
#
proc testWithLimit args {
    set body [lindex $args end]
    array set in [lrange $args 0 end-1]
    # test in child process (with limits):
    set pipe {}
    if {[catch {
	# start new process:
	set pipe [open |[list [interpreter]] r+]
	set ppid [pid $pipe]
	# create prlimit args:
	set args {}
	# with limited address space:
	if {[info exists in(-addmem)] || [info exists in(-maxmem)]} {
	    if {[info exists in(-addmem)]} {
		# as difference to normal usage, so try to retrieve current memory usage:
		if {[catch {
		    # using ps (vsz is in KB):
		    incr in(-addmem) [expr {[lindex [exec ps -hq $ppid -o vsz] end] * 1024}]
		}]} {
		    # ps failed, use default size 20MB:
		    incr in(-addmem) 20000000
		    # + size of locale-archive (may be up to 100MB):
		    incr in(-addmem) [expr {
			[file exists /usr/lib/locale/locale-archive] ?
			[file size /usr/lib/locale/locale-archive] : 0
		    }]
		}
		if {![info exists in(-maxmem)]} {
		    set in(-maxmem) $in(-addmem)
		}
		set in(-maxmem) [expr { max($in(-addmem), $in(-maxmem)) }]
	    }
	    append args --as=$in(-maxmem)
	}
	# apply limits:
	exec prlimit -p $ppid {*}$args
    } msg opt]} {
	catch {close $pipe}
	tcltest::Warn "testWithLimit: error - [regsub {^\s*([^\n]*).*$} $msg {\1}]"
	tcltest::Skip testWithLimit
    }
    # execute body, close process and return:
    set ret [catch {
	chan configure $pipe -buffering line
	puts $pipe "puts \[$body\]"
	puts $pipe exit
	set result [read $pipe]
	close $pipe
	set pipe {}
	set result
    } result opt]
    if {$pipe ne ""} { catch { close $pipe } }
    if {$ret && [dict get $opt -errorcode] eq "BYPASS-SKIPPED-TEST"} {
	return {*}$opt $result
    }
    if { ( [info exists in(-warn-on-code)] && $ret in $in(-warn-on-code) )
      || ( $ret && [info exists in(-warn-on-alloc-error)] && $in(-warn-on-alloc-error)
      	    && [regexp {\munable to (?:re)?alloc\M} $result] )
    } {
	tcltest::Warn "testWithLimit: wrong limit, result: $result"
	tcltest::Skip testWithLimit
    }
    return {*}$opt $result
}

# export all routines starting with test
namespace export test*

# for script path & as mark for loaded
proc scriptpath {} [list return [info script]]

}}; # end of internals.
