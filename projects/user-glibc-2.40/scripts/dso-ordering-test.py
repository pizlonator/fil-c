#!/usr/bin/python3
# Generate testcase files and Makefile fragments for DSO sorting test
# Copyright (C) 2021-2024 Free Software Foundation, Inc.
# This file is part of the GNU C Library.
#
# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <http://www.gnu.org/licenses/>.

"""Generate testcase files and Makefile fragments for DSO sorting test

This script takes a small description string language, and generates
testcases for displaying the ELF dynamic linker's dependency sorting
behavior, allowing verification.

Testcase descriptions are semicolon-separated description strings, and
this tool generates a testcase from the description, including main program,
associated modules, and Makefile fragments for including into elf/Makefile.

This allows automation of what otherwise would be very laborous manual
construction of complex dependency cases, however it must be noted that this
is only a tool to speed up testcase construction, and thus the generation
features are largely mechanical in nature; inconsistencies or errors may occur
if the input description was itself erroneous or have unforeseen interactions.

The format of the input test description files are:

  # Each test description has a name, lines of description,
  # and an expected output specification.  Comments use '#'.
  testname1: <test-description-line>
  output: <expected-output-string>

  # Tests can be marked to be XFAIL by using 'xfail_output' instead
  testname2: <test-description-line>
  xfail_output: <expected-output-string>

  # A default set of GLIBC_TUNABLES tunables can be specified, for which
  # all following tests will run multiple times, once for each of the
  # GLIBC_TUNABLES=... strings set by the 'tunable_option' command.
  tunable_option: <glibc-tunable-string1>
  tunable_option: <glibc-tunable-string2>

  # Test descriptions can use multiple lines, which will all be merged
  # together, so order is not important.
  testname3: <test-description-line>
  <test-description-line>
  <test-description-line>
  ...
  output: <expected-output-string>

  # 'testname3' will be run and compared two times, for both
  # GLIBC_TUNABLES=<glibc-tunable-string1> and
  # GLIBC_TUNABLES=<glibc-tunable-string2>.  This can be cleared and reset by the
  # 'clear_tunables' command:
  clear_tunables

  # Multiple expected outputs can also be specified, with an associated
  # tunable option in (), which multiple tests will be run with each
  # GLIBC_TUNABLES=... option tried.
  testname4:
  <test-description-line>
  ...
  output(<glibc-tunable-string1>): <expected-output-string-1>
  output(<glibc-tunable-string2>): <expected-output-string-2>
  # Individual tunable output cases can be XFAILed, though note that
  # this will have the effect of XFAILing the entire 'testname4' test
  # in the final top-level tests.sum summary.
  xfail_output(<glibc-tunable-string3>): <expected-output-string-3>

  # When multiple outputs (with specific tunable strings) are specified,
  # these take priority over any active 'tunable_option' settings.

  # When a test is meant to be placed under 'xtests' (not run under
  # "make check", but only when "make xtests" is used), the testcase name can be
  # declared using 'xtest(<test-name>)':
  ...
  xtest(test-too-big1): <test-description>
  output: <expected-output-string>
  ...

  # Do note that under current elf/Makefile organization, for such a xtest case,
  # while the test execution is only run under 'make xtests', the associated
  # DSOs are always built even under 'make check'.

On the description language used, an example description line string:

  a->b!->[cdef];c=>g=>h;{+c;%c;-c}->a

Each identifier represents a shared object module, currently sequences of
letters/digits are allowed, case-sensitive.

All such shared objects have a constructor/destructor generated for them
that emits its name followed by a '>' for constructors, and '<' followed by
its name for destructors, e.g. if the name is 'obj1', then "obj1>" and "<obj1"
is printed by its constructor/destructor respectively.

The -> operator specifies a link time dependency, these can be chained for
convenience (e.g. a->b->c->d).

The => operator creates a call-reference, e.g. for a=>b, an fn_a() function
is created inside module 'a', which calls fn_b() in module 'b'.
These module functions emit 'name()' output in nested form,
e.g. a=>b emits 'a(b())'

For single character object names, square brackets [] in the description
allows specifying multiple objects; e.g. a->[bcd]->e is equivalent to
 a->b->e;a->c->e;a->d->e

The () parenthesis construct with space separated names is also allowed for
specifying objects.  For names with integer suffixes a range can also be used,
e.g. (foo1 bar2-5), specifies DSOs foo1, bar2, bar2, bar3, bar4, bar5.

A {} construct specifies the main test program, and its link dependencies
are also specified using ->.  Inside {}, a few ;-separated constructs are
allowed:
         +a   Loads module a using dlopen(RTLD_LAZY|RTLD_GLOBAL)
         ^a   Loads module a using dlopen(RTLD_LAZY)
         %a   Use dlsym() to load and call fn_a()
         @a   Calls fn_a() directly.
         -a   Unloads module a using dlclose()

The generated main program outputs '{' '}' with all output from above
constructs in between.  The other output before/after {} are the ordered
constructor/destructor output.

If no {} construct is present, a default empty main program is linked
against all objects which have no dependency linked to it. e.g. for
'[ab]->c;d->e', the default main program is equivalent to '{}->[abd]'

Sometimes for very complex or large testcases, besides specifying a
few explicit dependencies from main{}, the above default dependency
behavior is still useful to automatically have, but is turned off
upon specifying a single explicit {}->dso_name.
In this case, add {}->* to explicitly add this generation behavior:

   # Main program links to 'foo', and all other objects which have no
   # dependency linked to it.
   {}->foo,{}->*

Note that '*' works not only on main{}, but can be used as the
dependency target of any object.  Note that it only works as a target,
not a dependency source.

The '!' operator after object names turns on permutation of its
dependencies, e.g. while a->[bcd] only generates one set of objects,
with 'a.so' built with a link line of "b.so c.so d.so", for a!->[bcd]
permutations of a's dependencies creates multiple testcases with
different link line orders: "b.so c.so d.so", "c.so b.so d.so",
"b.so d.so c.so", etc.  Note that for a <test-name> specified on
the script command-line, multiple <test-name_1>, <test-name_2>, etc.
tests will be generated (e.g. for a!->[bc]!->[de], eight tests with
different link orders for a, b, and c will be generated)

It is possible to specify the ELF soname field for an object or the
main program:
   # DSO 'a' will be linked with the appropriate -Wl,-soname=x setting
   a->b->c;soname(a)=x
   # The the main program can also have a soname specified
   soname({})=y

This can be used to test how ld.so behaves when objects and/or the
main program have such a field set.


Strings Output by Generated Testcase Programs

The text output produced by a generated testcase consists of three main
parts:
  1. The constructors' output
  2. Output from the main program
  3. Destructors' output

To see by example, a simple test description "a->b->c" generates a testcase
that when run, outputs: "c>b>a>{}<a<b<c"

Each generated DSO constructor prints its name followed by a '>' character,
and the "c>b>a" part above is the full constructor output by all DSOs, the
order indicating that DSO 'c', which does not depend on any other DSO, has
its constructor run first, followed by 'b' and then 'a'.

Destructor output for each DSO is a '<' character followed by its name,
reflecting its reverse nature of constructors.  In the above example, the
destructor output part is "<a<b<c".

The middle "{}" part is the main program.  In this simple example, nothing
was specified for the main program, so by default it is implicitly linked
to the DSO 'a' (with no other DSOs depending on it) and only prints the
brackets {} with no actions inside.

To see an example with actions inside the main program, lets see an example
description: c->g=>h;{+c;%c;-c}->a->h

This produces a testcase, that when executed outputs:
             h>a>{+c[g>c>];%c();-c[<c<g];}<a<h

The constructor and destructor parts display the a->h dependency as expected.
Inside the main program, the "+c" action triggers a dlopen() of DSO 'c',
causing another chain of constructors "g>c>" to be triggered.  Here it is
displayed inside [] brackets for each dlopen call.  The same is done for "-c",
a dlclose() of 'c'.

The "%c" output is due to calling to fn_c() inside DSO 'c', this comprises
of two parts: the '%' character is printed by the caller, here it is the main
program.  The 'c' character is printed from inside fn_c().  The '%' character
indicates that this is called by a dlsym() of "fn_c".  A '@' character would
mean a direct call (with a symbol reference).  These can all be controlled
by the main test program constructs documented earlier.

The output strings described here is the exact same form placed in
test description files' "output: <expected output>" line.
"""

import sys
import re
import os
import subprocess
import argparse
from collections import OrderedDict
import itertools

# BUILD_GCC is only used under the --build option,
# which builds the generated testcase, including DSOs using BUILD_GCC.
# Mainly for testing purposes, especially debugging of this script,
# and can be changed here to another toolchain path if needed.
build_gcc = "gcc"

def get_parser():
    parser = argparse.ArgumentParser("")
    parser.add_argument("description",
                         help="Description string of DSO dependency test to be "
                         "generated (see script source for documentation of "
                         "description language), either specified here as "
                         "command line argument, or by input file using "
                         "-f/--description-file option",
                         nargs="?", default="")
    parser.add_argument("test_name",
                        help="Identifier for testcase being generated",
                        nargs="?", default="")
    parser.add_argument("--objpfx",
                        help="Path to place generated files, defaults to "
                        "current directory if none specified",
                        nargs="?", default="./")
    parser.add_argument("-m", "--output-makefile",
                        help="File to write Makefile fragment to, defaults to "
                        "stdout when option not present",
                        nargs="?", default="")
    parser.add_argument("-f", "--description-file",
                        help="Input file containing testcase descriptions",
                        nargs="?", default="")
    parser.add_argument("--build", help="After C testcase generated, build it "
                        "using gcc (for manual testing purposes)",
                        action="store_true")
    parser.add_argument("--debug-output",
                        help="Prints some internal data "
                        "structures; used for debugging of this script",
                        action="store_true")
    return parser

# Main script starts here.
cmdlineargs = get_parser().parse_args()
test_name = cmdlineargs.test_name
description = cmdlineargs.description
objpfx = cmdlineargs.objpfx
description_file = cmdlineargs.description_file
output_makefile = cmdlineargs.output_makefile
makefile = ""
default_tunable_options = []

current_input_lineno = 0
def error(msg):
    global current_input_lineno
    print("Error: %s%s" % ((("Line %d, " % current_input_lineno)
                            if current_input_lineno != 0 else ""),
                           msg))
    exit(1)

if(test_name or description) and description_file:
    error("both command-line testcase and input file specified")
if test_name and not description:
    error("command-line testcase name without description string")

# Main class type describing a testcase.
class TestDescr:
    def __init__(self):
        self.objs = []              # list of all DSO objects
        self.deps = OrderedDict()   # map of DSO object -> list of dependencies

        # map of DSO object -> list of call refs
        self.callrefs = OrderedDict()

        # map of DSO object -> list of permutations of dependencies
        self.dep_permutations = OrderedDict()

        # map of DSO object -> SONAME of object (if one is specified)
        self.soname_map = OrderedDict()

        # list of main program operations
        self.main_program = []
        # set if default dependencies added to main
        self.main_program_default_deps = True

        self.test_name = ""                   # name of testcase
        self.expected_outputs = OrderedDict() # expected outputs of testcase
        self.xfail = False                    # set if this is a XFAIL testcase
        self.xtest = False                    # set if this is put under 'xtests'

    # Add 'object -> [object, object, ...]' relations to CURR_MAP
    def __add_deps_internal(self, src_objs, dst_objs, curr_map):
        for src in src_objs:
            for dst in dst_objs:
                if not src in curr_map:
                    curr_map[src] = []
                if not dst in curr_map[src]:
                    curr_map[src].append(dst)
    def add_deps(self, src_objs, dst_objs):
        self.__add_deps_internal(src_objs, dst_objs, self.deps)
    def add_callrefs(self, src_objs, dst_objs):
        self.__add_deps_internal(src_objs, dst_objs, self.callrefs)

# Process commands inside the {} construct.
# Note that throughout this script, the main program object is represented
# by the '#' string.
def process_main_program(test_descr, mainprog_str):
    if mainprog_str:
        test_descr.main_program = mainprog_str.split(';')
    for s in test_descr.main_program:
        m = re.match(r"^([+\-%^@])([0-9a-zA-Z]+)$", s)
        if not m:
            error("'%s' is not recognized main program operation" % (s))
        opr = m.group(1)
        obj = m.group(2)
        if not obj in test_descr.objs:
            test_descr.objs.append(obj)
        if opr == '%' or opr == '@':
            test_descr.add_callrefs(['#'], [obj])
    # We have a main program specified, turn this off
    test_descr.main_program_default_deps = False

# For(a1 a2 b1-12) object set descriptions, expand into an object list
def expand_object_set_string(descr_str):
    obj_list = []
    descr_list = descr_str.split()
    for descr in descr_list:
        m = re.match(r"^([a-zA-Z][0-9a-zA-Z]*)(-[0-9]+)?$", descr)
        if not m:
            error("'%s' is not a valid object set description" % (descr))
        obj = m.group(1)
        idx_end = m.group(2)
        if not idx_end:
            if not obj in obj_list:
                obj_list.append(obj)
        else:
            idx_end = int(idx_end[1:])
            m = re.match(r"^([0-9a-zA-Z][a-zA-Z]*)([0-9]+)$", obj)
            if not m:
                error("object description '%s' is malformed" % (obj))
            obj_name = m.group(1)
            idx_start = int(m.group (2))
            if idx_start > idx_end:
                error("index range %s-%s invalid" % (idx_start, idx_end))
            for i in range(idx_start, idx_end + 1):
                o = obj_name + str(i)
                if not o in obj_list:
                    obj_list.append(o)
    return obj_list

# Lexer for tokens
tokenspec = [ ("SONAME",   r"soname\(([0-9a-zA-Z{}]+)\)=([0-9a-zA-Z]+)"),
              ("OBJ",      r"([0-9a-zA-Z]+)"),
              ("DEP",      r"->"),
              ("CALLREF",  r"=>"),
              ("OBJSET",   r"\[([0-9a-zA-Z]+)\]"),
              ("OBJSET2",  r"\(([0-9a-zA-Z \-]+)\)"),
              ("OBJSET3",  r"\*"),
              ("PROG",     r"{([0-9a-zA-Z;+^\-%@]*)}"),
              ("PERMUTE",  r"!"),
              ("SEMICOL",  r";"),
              ("ERROR",    r".") ]
tok_re = '|'.join('(?P<%s>%s)' % pair for pair in tokenspec)

# Main line parser of description language
def parse_description_string(t, descr_str):
    # State used when parsing dependencies
    curr_objs = []
    in_dep = False
    in_callref = False
    def clear_dep_state():
        nonlocal in_dep, in_callref
        in_dep = in_callref = False

    for m in re.finditer(tok_re, descr_str):
        kind = m.lastgroup
        value = m.group()
        if kind == "SONAME":
            s = re.match(r"soname\(([0-9a-zA-Z{}]+)\)=([0-9a-zA-Z]+)", value)
            obj = s.group(1)
            val = s.group(2)
            if obj == "{}":
                if '#' in t.soname_map:
                    error("soname of main program already set")
                # Adjust to internal name
                obj = '#'
            else:
                if re.match(r"[{}]", obj):
                    error("invalid object name '%s'" % (obj))
                if not obj in t.objs:
                    error("'%s' is not name of already defined object" % (obj))
                if obj in t.soname_map:
                    error("'%s' already has soname of '%s' set"
                          % (obj, t.soname_map[obj]))
            t.soname_map[obj] = val

        elif kind == "OBJ":
            if in_dep:
                t.add_deps(curr_objs, [value])
            elif in_callref:
                t.add_callrefs(curr_objs, [value])
            clear_dep_state()
            curr_objs = [value]
            if not value in t.objs:
                t.objs.append(value)

        elif kind == "OBJSET":
            objset = value[1:len(value)-1]
            if in_dep:
                t.add_deps(curr_objs, list (objset))
            elif in_callref:
                t.add_callrefs(curr_objs, list (objset))
            clear_dep_state()
            curr_objs = list(objset)
            for o in list(objset):
                if not o in t.objs:
                    t.objs.append(o)

        elif kind == "OBJSET2":
            descr_str = value[1:len(value)-1]
            descr_str.strip()
            objs = expand_object_set_string(descr_str)
            if not objs:
                error("empty object set '%s'" % (value))
            if in_dep:
                t.add_deps(curr_objs, objs)
            elif in_callref:
                t.add_callrefs(curr_objs, objs)
            clear_dep_state()
            curr_objs = objs
            for o in objs:
                if not o in t.objs:
                    t.objs.append(o)

        elif kind == "OBJSET3":
            if in_dep:
                t.add_deps(curr_objs, ['*'])
            elif in_callref:
                t.add_callrefs(curr_objs, ['*'])
            else:
                error("non-dependence target set '*' can only be used "
                      "as target of ->/=> operations")
            clear_dep_state()
            curr_objs = ['*']

        elif kind == "PERMUTE":
            if in_dep or in_callref:
                error("syntax error, permute operation invalid here")
            if not curr_objs:
                error("syntax error, no objects to permute here")

            for obj in curr_objs:
                if not obj in t.dep_permutations:
                    # Signal this object has permuted dependencies
                    t.dep_permutations[obj] = []

        elif kind == "PROG":
            if t.main_program:
                error("cannot have more than one main program")
            if in_dep:
                error("objects cannot have dependency on main program")
            if in_callref:
                # TODO: A DSO can resolve to a symbol in the main binary,
                # which we syntactically allow here, but haven't yet
                # implemented.
                t.add_callrefs(curr_objs, ["#"])
            process_main_program(t, value[1:len(value)-1])
            clear_dep_state()
            curr_objs = ["#"]

        elif kind == "DEP":
            if in_dep or in_callref:
                error("syntax error, multiple contiguous ->,=> operations")
            if '*' in curr_objs:
                error("non-dependence target set '*' can only be used "
                      "as target of ->/=> operations")
            in_dep = True

        elif kind == "CALLREF":
            if in_dep or in_callref:
                error("syntax error, multiple contiguous ->,=> operations")
            if '*' in curr_objs:
                error("non-dependence target set '*' can only be used "
                      "as target of ->/=> operations")
            in_callref = True

        elif kind == "SEMICOL":
            curr_objs = []
            clear_dep_state()

        else:
            error("unknown token '%s'" % (value))
    return t

# Main routine to process each testcase description
def process_testcase(t):
    global objpfx
    assert t.test_name

    base_test_name = t.test_name
    test_subdir = base_test_name + "-dir"
    testpfx = objpfx + test_subdir + "/"
    test_srcdir = "dso-sort-tests-src/"
    testpfx_src = objpfx + test_srcdir

    if not os.path.exists(testpfx):
        os.mkdir(testpfx)
    if not os.path.exists(testpfx_src):
        os.mkdir(testpfx_src)

    def find_objs_not_depended_on(t):
        objs_not_depended_on = []
        for obj in t.objs:
            skip = False
            for r in t.deps.items():
                if obj in r[1]:
                    skip = True
                    break
            if not skip:
                objs_not_depended_on.append(obj)
        return objs_not_depended_on

    non_dep_tgt_objs = find_objs_not_depended_on(t)
    for obj in t.objs:
        if obj in t.deps:
            deps = t.deps[obj]
            if '*' in deps:
                deps.remove('*')
                t.add_deps([obj], non_dep_tgt_objs)
        if obj in t.callrefs:
            deps = t.callrefs[obj]
            if '*' in deps:
                deps.remove('*')
                t.add_callrefs([obj], non_dep_tgt_objs)
    if "#" in t.deps:
        deps = t.deps["#"]
        if '*' in deps:
            deps.remove('*')
            t.add_deps(["#"], non_dep_tgt_objs)

    # If no main program was specified in dependency description, make a
    # default main program with deps pointing to all DSOs which are not
    # depended by another DSO.
    if t.main_program_default_deps:
        main_deps = non_dep_tgt_objs
        if not main_deps:
            error("no objects for default main program to point "
                  "dependency to(all objects strongly connected?)")
        t.add_deps(["#"], main_deps)

    # Some debug output
    if cmdlineargs.debug_output:
        print("Testcase: %s" % (t.test_name))
        print("All objects: %s" % (t.objs))
        print("--- Static link dependencies ---")
        for r in t.deps.items():
            print("%s -> %s" % (r[0], r[1]))
        print("--- Objects whose dependencies are to be permuted ---")
        for r in t.dep_permutations.items():
            print("%s" % (r[0]))
        print("--- Call reference dependencies ---")
        for r in t.callrefs.items():
            print("%s => %s" % (r[0], r[1]))
        print("--- main program ---")
        print(t.main_program)

    # Main testcase generation routine, does Makefile fragment generation,
    # testcase source generation, and if --build specified builds testcase.
    def generate_testcase(test_descr, test_suffix):

        test_name = test_descr.test_name + test_suffix

        # Print out needed Makefile fragments for use in glibc/elf/Makefile.
        module_names = ""
        for o in test_descr.objs:
            rule = ("$(objpfx)" + test_subdir + "/" + test_name
                    + "-" + o + ".os: $(objpfx)" + test_srcdir
                    + test_name + "-" + o + ".c\n"
                    "\t$(compile.c) $(OUTPUT_OPTION)\n")
            makefile.write (rule)
            module_names += " " + test_subdir + "/" + test_name + "-" + o
        makefile.write("modules-names +=%s\n" % (module_names))

        # Depth-first traversal, executing FN(OBJ) in post-order
        def dfs(t, fn):
            def dfs_rec(obj, fn, obj_visited):
                if obj in obj_visited:
                    return
                obj_visited[obj] = True
                if obj in t.deps:
                    for dep in t.deps[obj]:
                        dfs_rec(dep, fn, obj_visited)
                fn(obj)

            obj_visited = {}
            for obj in t.objs:
                dfs_rec(obj, fn, obj_visited)

        # Generate link dependencies for all DSOs, done in a DFS fashion.
        # Usually this doesn't need to be this complex, just listing the direct
        # dependencies is enough.  However to support creating circular
        # dependency situations, traversing it by DFS and tracking processing
        # status is the natural way to do it.
        obj_processed = {}
        fake_created = {}
        def gen_link_deps(obj):
            if obj in test_descr.deps:
                dso = test_subdir + "/" + test_name + "-" + obj + ".so"
                dependencies = ""
                for dep in test_descr.deps[obj]:
                    if dep in obj_processed:
                        depstr = (" $(objpfx)" + test_subdir + "/"
                                  + test_name + "-" + dep + ".so")
                    else:
                        # A circular dependency is satisfied by making a
                        # fake DSO tagged with the correct SONAME
                        depstr = (" $(objpfx)" + test_subdir + "/"
                                  + test_name + "-" + dep + ".FAKE.so")
                        # Create empty C file and Makefile fragments for fake
                        # object.  This only needs to be done at most once for
                        # an object name.
                        if not dep in fake_created:
                            f = open(testpfx_src + test_name + "-" + dep
                                     + ".FAKE.c", "w")
                            f.write(" \n")
                            f.close()
                            # Generate rule to create fake object
                            makefile.write \
                                ("LDFLAGS-%s = -Wl,--no-as-needed "
                                 "-Wl,-soname=%s\n"
                                 % (test_name + "-" + dep + ".FAKE.so",
                                    ("$(objpfx)" + test_subdir + "/"
                                     + test_name + "-" + dep + ".so")))
                            rule = ("$(objpfx)" + test_subdir + "/"
                                    + test_name + "-" + dep + ".FAKE.os: "
                                    "$(objpfx)" + test_srcdir
                                    + test_name + "-" + dep + ".FAKE.c\n"
                                    "\t$(compile.c) $(OUTPUT_OPTION)\n")
                            makefile.write (rule)
                            makefile.write \
                                ("modules-names += %s\n"
                                 % (test_subdir + "/"
                                    + test_name + "-" + dep + ".FAKE"))
                            fake_created[dep] = True
                    dependencies += depstr
                makefile.write("$(objpfx)%s:%s\n" % (dso, dependencies))
            # Mark obj as processed
            obj_processed[obj] = True

        dfs(test_descr, gen_link_deps)

        # Print LDFLAGS-* and *-no-z-defs
        for o in test_descr.objs:
            dso = test_name + "-" + o + ".so"
            ldflags = "-Wl,--no-as-needed"
            if o in test_descr.soname_map:
                soname = ("$(objpfx)" + test_subdir + "/"
                          + test_name + "-"
                          + test_descr.soname_map[o] + ".so")
                ldflags += (" -Wl,-soname=" + soname)
            makefile.write("LDFLAGS-%s = %s\n" % (dso, ldflags))
            if o in test_descr.callrefs:
                makefile.write("%s-no-z-defs = yes\n" % (dso))

        # Print dependencies for main test program.
        depstr = ""
        if '#' in test_descr.deps:
            for o in test_descr.deps['#']:
                depstr += (" $(objpfx)" + test_subdir + "/"
                           + test_name + "-" + o + ".so")
        makefile.write("$(objpfx)%s/%s:%s\n" % (test_subdir, test_name, depstr))
        ldflags = "-Wl,--no-as-needed"
        if '#' in test_descr.soname_map:
            soname = ("$(objpfx)" + test_subdir + "/"
                      + test_name + "-"
                      + test_descr.soname_map['#'] + ".so")
            ldflags += (" -Wl,-soname=" + soname)
        makefile.write("LDFLAGS-%s = %s\n" % (test_name, ldflags))
        rule = ("$(objpfx)" + test_subdir + "/" + test_name + ".o: "
                "$(objpfx)" + test_srcdir + test_name + ".c\n"
                "\t$(compile.c) $(OUTPUT_OPTION)\n")
        makefile.write (rule)

        # Ensure that all shared objects are built before running the
        # test, whether there link-time dependencies or not.
        depobjs = ["$(objpfx){}/{}-{}.so".format(test_subdir, test_name, dep)
                   for dep in test_descr.objs]
        makefile.write("$(objpfx){}.out: {}\n".format(
            base_test_name, " ".join(depobjs)))

        # Add main executable to test-srcs
        makefile.write("test-srcs += %s/%s\n" % (test_subdir, test_name))
        # Add dependency on main executable of test
        makefile.write("$(objpfx)%s.out: $(objpfx)%s/%s\n"
                        % (base_test_name, test_subdir, test_name))

        for r in test_descr.expected_outputs.items():
            tunable_options = []
            specific_tunable = r[0]
            xfail = r[1][1]
            if specific_tunable != "":
                tunable_options = [specific_tunable]
            else:
                tunable_options = default_tunable_options
                if not tunable_options:
                    tunable_options = [""]

            for tunable in tunable_options:
                tunable_env = ""
                tunable_sfx = ""
                exp_tunable_sfx = ""
                if tunable:
                    tunable_env = "GLIBC_TUNABLES=%s " % tunable
                    tunable_sfx = "-" + tunable.replace("=","_")
                if specific_tunable:
                    tunable_sfx = "-" + specific_tunable.replace("=","_")
                    exp_tunable_sfx = tunable_sfx
                tunable_descr = ("(%s)" % tunable_env.strip()
                                 if tunable_env else "")
                # Write out fragment of shell script for this single test.
                test_descr.sh.write \
                    ("${test_wrapper_env} ${run_program_env} %s\\\n"
                     "${common_objpfx}support/test-run-command \\\n"
                     "${common_objpfx}elf/ld.so \\\n"
                     "--library-path ${common_objpfx}elf/%s:"
                     "${common_objpfx}elf:${common_objpfx}.:"
                     "${common_objpfx}dlfcn \\\n"
                     "${common_objpfx}elf/%s/%s > \\\n"
                     "  ${common_objpfx}elf/%s/%s%s.output\n"
                     % (tunable_env ,test_subdir,
                        test_subdir, test_name, test_subdir, test_name,
                        tunable_sfx))
                # Generate a run of each test and compare with expected out
                test_descr.sh.write \
                    ("if [ $? -ne 0 ]; then\n"
                     "  echo '%sFAIL: %s%s execution test'\n"
                     "  something_failed=true\n"
                     "else\n"
                     "  diff -wu ${common_objpfx}elf/%s/%s%s.output \\\n"
                     "           ${common_objpfx}elf/%s%s%s.exp\n"
                     "  if [ $? -ne 0 ]; then\n"
                     "    echo '%sFAIL: %s%s expected output comparison'\n"
                     "    something_failed=true\n"
                     "  fi\n"
                     "fi\n"
                     % (("X" if xfail else ""), test_name, tunable_descr,
                        test_subdir, test_name, tunable_sfx,
                        test_srcdir, base_test_name, exp_tunable_sfx,
                        ("X" if xfail else ""), test_name, tunable_descr))

        # Generate C files according to dependency and calling relations from
        # description string.
        for obj in test_descr.objs:
            src_name = test_name + "-" + obj + ".c"
            f = open(testpfx_src + src_name, "w")
            if obj in test_descr.callrefs:
                called_objs = test_descr.callrefs[obj]
                for callee in called_objs:
                    f.write("extern void fn_%s (void);\n" % (callee))
            if len(obj) == 1:
                f.write("extern int putchar(int);\n")
                f.write("static void __attribute__((constructor)) " +
                         "init(void){putchar('%s');putchar('>');}\n" % (obj))
                f.write("static void __attribute__((destructor)) " +
                         "fini(void){putchar('<');putchar('%s');}\n" % (obj))
            else:
                f.write('extern int printf(const char *, ...);\n')
                f.write('static void __attribute__((constructor)) ' +
                         'init(void){printf("%s>");}\n' % (obj))
                f.write('static void __attribute__((destructor)) ' +
                         'fini(void){printf("<%s");}\n' % (obj))
            if obj in test_descr.callrefs:
                called_objs = test_descr.callrefs[obj]
                if len(obj) != 1:
                    f.write("extern int putchar(int);\n")
                f.write("void fn_%s (void) {\n" % (obj))
                if len(obj) == 1:
                    f.write("  putchar ('%s');\n" % (obj));
                    f.write("  putchar ('(');\n");
                else:
                    f.write('  printf ("%s(");\n' % (obj));
                for callee in called_objs:
                    f.write("  fn_%s ();\n" % (callee))
                f.write("  putchar (')');\n");
                f.write("}\n")
            else:
                for callref in test_descr.callrefs.items():
                    if obj in callref[1]:
                        if len(obj) == 1:
                            # We need to declare printf here in this case.
                            f.write('extern int printf(const char *, ...);\n')
                        f.write("void fn_%s (void) {\n" % (obj))
                        f.write('  printf ("%s()");\n' % (obj))
                        f.write("}\n")
                        break
            f.close()

        # Open C file for writing main program
        f = open(testpfx_src + test_name + ".c", "w")

        # if there are some operations in main(), it means we need -ldl
        f.write("#include <stdio.h>\n")
        f.write("#include <stdlib.h>\n")
        f.write("#include <dlfcn.h>\n")
        for s in test_descr.main_program:
            if s[0] == '@':
                f.write("extern void fn_%s (void);\n" % (s[1:]));
        f.write("int main (void) {\n")
        f.write("  putchar('{');\n")

        # Helper routine for generating sanity checking code.
        def put_fail_check(fail_cond, action_desc):
            f.write('  if (%s) { printf ("\\n%s failed: %%s\\n", '
                     'dlerror()); exit (1);}\n' % (fail_cond, action_desc))
        i = 0
        while i < len(test_descr.main_program):
            s = test_descr.main_program[i]
            obj = s[1:]
            dso = test_name + "-" + obj
            if s[0] == '+' or s[0] == '^':
                if s[0] == '+':
                    dlopen_flags = "RTLD_LAZY|RTLD_GLOBAL"
                    f.write("  putchar('+');\n");
                else:
                    dlopen_flags = "RTLD_LAZY"
                    f.write("  putchar(':');\n");
                if len(obj) == 1:
                    f.write("  putchar('%s');\n" % (obj));
                else:
                    f.write('  printf("%s");\n' % (obj));
                f.write("  putchar('[');\n");
                f.write('  void *%s = dlopen ("%s.so", %s);\n'
                         % (obj, dso, dlopen_flags))
                put_fail_check("!%s" % (obj),
                                "%s.so dlopen" % (dso))
                f.write("  putchar(']');\n");
            elif s[0] == '-':
                f.write("  putchar('-');\n");
                if len(obj) == 1:
                    f.write("  putchar('%s');\n" % (obj));
                else:
                    f.write('  printf("%s");\n' % (obj));
                f.write("  putchar('[');\n");
                put_fail_check("dlclose (%s) != 0" % (obj),
                                "%s.so dlclose" % (dso))
                f.write("  putchar(']');\n");
            elif s[0] == '%':
                f.write("  putchar('%');\n");
                f.write('  void (*fn_%s)(void) = dlsym (%s, "fn_%s");\n'
                         % (obj, obj, obj))
                put_fail_check("!fn_%s" % (obj),
                                "dlsym(fn_%s) from %s.so" % (obj, dso))
                f.write("  fn_%s ();\n" % (obj))
            elif s[0] == '@':
                f.write("  putchar('@');\n");
                f.write("  fn_%s ();\n" % (obj))
            f.write("  putchar(';');\n");
            i += 1
        f.write("  putchar('}');\n")
        f.write("  return 0;\n")
        f.write("}\n")
        f.close()

        # --build option processing: build generated sources using 'build_gcc'
        if cmdlineargs.build:
            # Helper routine to run a shell command, for running GCC below
            def run_cmd(args):
                cmd = str.join(' ', args)
                if cmdlineargs.debug_output:
                    print(cmd)
                p = subprocess.Popen(args)
                p.wait()
                if p.returncode != 0:
                    error("error running command: %s" % (cmd))

            # Compile individual .os files
            for obj in test_descr.objs:
                src_name = test_name + "-" + obj + ".c"
                obj_name = test_name + "-" + obj + ".os"
                run_cmd([build_gcc, "-c", "-fPIC", testpfx_src + src_name,
                          "-o", testpfx + obj_name])

            obj_processed = {}
            fake_created = {}
            # Function to create <test_name>-<obj>.so
            def build_dso(obj):
                obj_name = test_name + "-" + obj + ".os"
                dso_name = test_name + "-" + obj + ".so"
                deps = []
                if obj in test_descr.deps:
                    for dep in test_descr.deps[obj]:
                        if dep in obj_processed:
                            deps.append(dep)
                        else:
                            deps.append(dep + ".FAKE")
                            if not dep in fake_created:
                                base_name = testpfx + test_name + "-" + dep
                                src_base_name = (testpfx_src + test_name
                                                 + "-" + dep)
                                cmd = [build_gcc, "-Wl,--no-as-needed",
                                       ("-Wl,-soname=" + base_name + ".so"),
                                       "-shared", base_name + ".FAKE.c",
                                       "-o", src_base_name + ".FAKE.so"]
                                run_cmd(cmd)
                                fake_created[dep] = True
                dso_deps = map(lambda d: testpfx + test_name + "-" + d + ".so",
                               deps)
                cmd = [build_gcc, "-shared", "-o", testpfx + dso_name,
                       testpfx + obj_name, "-Wl,--no-as-needed"]
                if obj in test_descr.soname_map:
                    soname = ("-Wl,-soname=" + testpfx + test_name + "-"
                              + test_descr.soname_map[obj] + ".so")
                    cmd += [soname]
                cmd += list(dso_deps)
                run_cmd(cmd)
                obj_processed[obj] = True

            # Build all DSOs, this needs to be in topological dependency order,
            # or link will fail
            dfs(test_descr, build_dso)

            # Build main program
            deps = []
            if '#' in test_descr.deps:
                deps = test_descr.deps['#']
            main_deps = map(lambda d: testpfx + test_name + "-" + d + ".so",
                            deps)
            cmd = [build_gcc, "-Wl,--no-as-needed", "-o", testpfx + test_name,
                   testpfx_src + test_name + ".c", "-L%s" % (os.getcwd()),
                   "-Wl,-rpath-link=%s" % (os.getcwd())]
            if '#' in test_descr.soname_map:
                soname = ("-Wl,-soname=" + testpfx + test_name + "-"
                          + test_descr.soname_map['#'] + ".so")
                cmd += [soname]
            cmd += list(main_deps)
            run_cmd(cmd)

    # Check if we need to enumerate permutations of dependencies
    need_permutation_processing = False
    if t.dep_permutations:
        # Adjust dep_permutations into map of object -> dependency permutations
        for r in t.dep_permutations.items():
            obj = r[0]
            if obj in t.deps and len(t.deps[obj]) > 1:
                deps = t.deps[obj]
                t.dep_permutations[obj] = list(itertools.permutations (deps))
                need_permutation_processing = True

    def enum_permutations(t, perm_list):
        test_subindex = 1
        curr_perms = []
        def enum_permutations_rec(t, perm_list):
            nonlocal test_subindex, curr_perms
            if len(perm_list) >= 1:
                curr = perm_list[0]
                obj = curr[0]
                perms = curr[1]
                if not perms:
                    # This may be an empty list if no multiple dependencies to
                    # permute were found, skip to next in this case
                    enum_permutations_rec(t, perm_list[1:])
                else:
                    for deps in perms:
                        t.deps[obj] = deps
                        permstr = "" if obj == "#" else obj + "_"
                        permstr += str.join('', deps)
                        curr_perms.append(permstr)
                        enum_permutations_rec(t, perm_list[1:])
                        curr_perms = curr_perms[0:len(curr_perms)-1]
            else:
                # t.deps is now instantiated with one dependency order
                # permutation(across all objects that have multiple
                # permutations), now process a testcase
                generate_testcase(t, ("_" + str (test_subindex)
                                       + "-" + str.join('-', curr_perms)))
                test_subindex += 1
        enum_permutations_rec(t, perm_list)

    # Create *.exp files with expected outputs
    for r in t.expected_outputs.items():
        sfx = ""
        if r[0] != "":
            sfx = "-" + r[0].replace("=","_")
        f = open(testpfx_src + t.test_name + sfx + ".exp", "w")
        (output, xfail) = r[1]
        f.write('%s' % output)
        f.close()

    # Create header part of top-level testcase shell script, to wrap execution
    # and output comparison together.
    t.sh = open(testpfx_src + t.test_name + ".sh", "w")
    t.sh.write("#!/bin/sh\n")
    t.sh.write("# Test driver for %s, generated by "
                "dso-ordering-test.py\n" % (t.test_name))
    t.sh.write("common_objpfx=$1\n")
    t.sh.write("test_wrapper_env=$2\n")
    t.sh.write("run_program_env=$3\n")
    t.sh.write("something_failed=false\n")

    # Starting part of Makefile fragment
    makefile.write("ifeq (yes,$(build-shared))\n")

    if need_permutation_processing:
        enum_permutations(t, list (t.dep_permutations.items()))
    else:
        # We have no permutations to enumerate, just process testcase normally
        generate_testcase(t, "")

    # If testcase is XFAIL, indicate so
    if t.xfail:
        makefile.write("test-xfail-%s = yes\n" % t.test_name)

    # Output end part of Makefile fragment
    expected_output_files = ""
    for r in t.expected_outputs.items():
        sfx = ""
        if r[0] != "":
            sfx = "-" + r[0].replace("=","_")
        expected_output_files += " $(objpfx)%s%s%s.exp" % (test_srcdir,
                                                            t.test_name, sfx)
    makefile.write \
    ("$(objpfx)%s.out: $(objpfx)%s%s.sh%s "
     "$(common-objpfx)support/test-run-command\n"
     % (t.test_name, test_srcdir, t.test_name,
        expected_output_files))
    makefile.write("\t$(SHELL) $< $(common-objpfx) '$(test-wrapper-env)' "
                    "'$(run-program-env)' > $@; $(evaluate-test)\n")
    makefile.write("ifeq ($(run-built-tests),yes)\n")
    if t.xtest:
        makefile.write("xtests-special += $(objpfx)%s.out\n" % (t.test_name))
    else:
        makefile.write("tests-special += $(objpfx)%s.out\n" % (t.test_name))
    makefile.write("endif\n")
    makefile.write("endif\n")

    # Write ending part of shell script generation
    t.sh.write("if $something_failed; then\n"
                "  exit 1\n"
                "else\n"
                "  echo '%sPASS: all tests for %s succeeded'\n"
                "  exit 0\n"
                "fi\n" % (("X" if t.xfail else ""),
                          t.test_name))
    t.sh.close()

# Description file parsing
def parse_description_file(filename):
    global default_tunable_options
    global current_input_lineno
    f = open(filename)
    if not f:
        error("cannot open description file %s" % (filename))
    descrfile_lines = f.readlines()
    t = None
    for line in descrfile_lines:
        p = re.compile(r"#.*$")
        line = p.sub("", line) # Filter out comments
        line = line.strip() # Remove excess whitespace
        current_input_lineno += 1

        m = re.match(r"^tunable_option:\s*(.*)$", line)
        if m:
            if m.group(1) == "":
                error("tunable option cannot be empty")
            default_tunable_options.append(m.group (1))
            continue

        m = re.match(r"^clear_tunables$", line)
        if m:
            default_tunable_options = []
            continue

        m = re.match(r"^([^:]+):\s*(.*)$", line)
        if m:
            lhs = m.group(1)
            o = re.match(r"^output(.*)$", lhs)
            xfail = False
            if not o:
                o = re.match(r"^xfail_output(.*)$", lhs)
                if o:
                    xfail = True;
            if o:
                if not t:
                    error("output specification without testcase description")
                tsstr = ""
                if o.group(1):
                    ts = re.match(r"^\(([a-zA-Z0-9_.=]*)\)$", o.group (1))
                    if not ts:
                        error("tunable option malformed '%s'" % o.group(1))
                    tsstr = ts.group(1)
                t.expected_outputs[tsstr] = (m.group(2), xfail)
                # Any tunable option XFAILed means entire testcase
                # is XFAIL/XPASS
                t.xfail |= xfail
            else:
                if t:
                    # Starting a new test description, end and process
                    # current one.
                    process_testcase(t)
                t = TestDescr()
                x = re.match(r"^xtest\((.*)\)$", lhs)
                if x:
                    t.xtest = True
                    t.test_name = x.group(1)
                else:
                    t.test_name = lhs
                descr_string = m.group(2)
                parse_description_string(t, descr_string)
            continue
        else:
            if line:
                if not t:
                    error("no active testcase description")
                parse_description_string(t, line)
    # Process last completed test description
    if t:
        process_testcase(t)

# Setup Makefile output to file or stdout as selected
if output_makefile:
    output_makefile_dir = os.path.dirname(output_makefile)
    if output_makefile_dir:
        os.makedirs(output_makefile_dir, exist_ok = True)
    makefile = open(output_makefile, "w")
else:
    makefile = open(sys.stdout.fileno (), "w")

# Finally, the main top-level calling of above parsing routines.
if description_file:
    parse_description_file(description_file)
else:
    t = TestDescr()
    t.test_name = test_name
    parse_description_string(t, description)
    process_testcase(t)

# Close Makefile fragment output
makefile.close()
