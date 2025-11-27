# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2008-2010 Johan Dahlin
# Copyright (C) 2009 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

import errno
import optparse
import os
import shutil
import stat
import sys
import tempfile
import platform
import shlex

import giscanner
from giscanner import message, pkgconfig
from giscanner.annotationparser import GtkDocCommentBlockParser
from giscanner.ast import Include, Namespace
from giscanner.dumper import compile_introspection_binary
from giscanner.gdumpparser import GDumpParser, IntrospectionBinary
from giscanner.introspectablepass import IntrospectablePass
from giscanner.girparser import GIRParser
from giscanner.girwriter import GIRWriter
from giscanner.maintransformer import MainTransformer
from giscanner.shlibs import resolve_shlibs
from giscanner.sourcescanner import SourceScanner, ALL_EXTS
from giscanner.transformer import Transformer
from . import utils


def process_cflags_begin(option, opt, value, parser):
    cflags = getattr(parser.values, option.dest)
    while len(parser.rargs) > 0 and parser.rargs[0] != '--cflags-end':
        arg = parser.rargs.pop(0)
        if arg == "-I" and parser.rargs and parser.rargs[0] != '--cflags-end':
            # This is a special case where there's a space between -I and the path.
            arg += parser.rargs.pop(0)
        cflags.append(utils.cflag_real_include_path(arg))


def process_cflags_end(option, opt, value, parser):
    pass


def process_cpp_includes(option, opt, value, parser):
    cpp_includes = getattr(parser.values, option.dest)
    cpp_includes.append(os.path.realpath(value))


def get_preprocessor_option_group(parser):
    group = optparse.OptionGroup(parser, "Preprocessor options")
    group.add_option("", "--cflags-begin",
                     help="Start preprocessor/compiler flags",
                     dest="cflags", default=[],
                     action="callback", callback=process_cflags_begin)
    group.add_option("", "--cflags-end",
                     help="End preprocessor/compiler flags",
                     action="callback", callback=process_cflags_end)
    group.add_option("-I", help="Pre-processor include file",
                     dest="cpp_includes", default=[], type="string",
                     action="callback", callback=process_cpp_includes)
    group.add_option("-D", help="Pre-processor define",
                     action="append", dest="cpp_defines",
                     default=[])
    group.add_option("-U", help="Pre-processor undefine",
                     action="append", dest="cpp_undefines",
                     default=[])
    group.add_option("-p", dest="", help="Ignored")
    return group


def get_windows_option_group(parser):
    group = optparse.OptionGroup(parser, "Machine Dependent Options")
    group.add_option("-m", help="some machine dependent option",
                     action="append", dest='m_option',
                     default=[])

    return group


def _get_option_parser():
    parser = optparse.OptionParser('%prog [options] sources',
                                   version='%prog ' + giscanner.__version__)
    parser.add_option('', "--quiet",
                      action="store_true", dest="quiet",
                      default=False,
                      help="If passed, do not print details of normal operation")
    parser.add_option("", "--format",
                      action="store", dest="format",
                      default="gir",
                      help="format to use, one of gidl, gir")
    parser.add_option("-i", "--include",
                      action="append", dest="includes", default=[],
                      help="Add specified gir file as dependency")
    parser.add_option("", "--include-uninstalled",
                      action="append", dest="includes_uninstalled", default=[],
                      help=("""A file path to a dependency; only use this "
                            "when building multiple .gir files inside a "
                            "single module."""))
    parser.add_option("", "--add-include-path",
                      action="append", dest="include_paths", default=[],
                      help="include paths for other GIR files")
    parser.add_option("", "--program",
                      action="store", dest="program", default=None,
                      help="program to execute")
    parser.add_option("", "--use-binary-wrapper",
                      action="store", dest="wrapper", default=None,
                      help="wrapper to use for running programs (useful when cross-compiling)")
    parser.add_option("", "--use-ldd-wrapper",
                      action="store", dest="ldd_wrapper", default=None,
                      help="wrapper to use instead of ldd (useful when cross-compiling)")
    parser.add_option("", "--lib-dirs-envvar",
                      action="store", dest="lib_dirs_envvar", default=None,
                      help="environment variable to write a list of library directories to (for running the transient binary), instead of standard LD_LIBRARY_PATH")
    parser.add_option("", "--program-arg",
                      action="append", dest="program_args", default=[],
                      help="extra arguments to program")
    parser.add_option("", "--libtool",
                      action="store", dest="libtool_path", default=None,
                      help="full path to libtool")
    parser.add_option("", "--no-libtool",
                      action="store_true", dest="nolibtool", default=False,
                      help="do not use libtool")
    parser.add_option("", "--external-library",
                      action="store_true", dest="external_library", default=False,
                      help=("""If true, the library is located on the system,""" +
                            """not in the current directory"""))
    parser.add_option("-l", "--library",
                      action="append", dest="libraries", default=[],
                      help="libraries of this unit")
    parser.add_option("", "--extra-library",
                      action="append", dest="extra_libraries", default=[],
                      help="Extra libraries to link the binary against")
    parser.add_option("-L", "--library-path",
                      action="append", dest="library_paths", default=[],
                      help="directories to search for libraries")
    parser.add_option("", "--header-only",
                      action="store_true", dest="header_only", default=[],
                      help="If specified, just generate a GIR for the given header files")
    parser.add_option("-n", "--namespace",
                      action="store", dest="namespace_name",
                      help=("name of namespace for this unit, also "
                            "used to compute --identifier-prefix and --symbol-prefix"))
    parser.add_option("", "--nsversion",
                      action="store", dest="namespace_version",
                      help="version of namespace for this unit")
    parser.add_option("", "--strip-prefix",
                      action="store", dest="strip_prefix",
                      help="""Option --strip-prefix is deprecated, please see --identifier-prefix
and --symbol-prefix.""")
    parser.add_option("", "--identifier-prefix",
                      action="append", dest="identifier_prefixes", default=[],
                      help="""Remove this prefix from C identifiers (structure typedefs, etc.).
May be specified multiple times.  This is also used as the default for --symbol-prefix if
the latter is not specified.""")
    parser.add_option("", "--identifier-filter-cmd",
                      action="store", dest="identifier_filter_cmd", default='',
                      help='Filter identifiers (struct and union typedefs) through the given '
                           'shell command which will receive the identifier name as input '
                           'to stdin and is expected to output the filtered results to stdout.')
    parser.add_option("", "--symbol-prefix",
                      action="append", dest="symbol_prefixes", default=[],
                      help="Remove this prefix from C symbols (function names)")
    parser.add_option("", "--symbol-filter-cmd",
                      action="store", dest="symbol_filter_cmd", default='',
                      help='Filter symbols (function names) through the given '
                           'shell command which will receive the symbol name as input '
                           'to stdin and is expected to output the filtered results to stdout.')
    parser.add_option("", "--accept-unprefixed",
                      action="store_true", dest="accept_unprefixed", default=False,
                      help="""If specified, accept symbols and identifiers that do not
match the namespace prefix.""")
    parser.add_option("", "--add-init-section",
                      action="append", dest="init_sections", default=[],
            help="add extra initialization code in the introspection program")
    parser.add_option("-o", "--output",
                      action="store", dest="output", default="-",
                      help="output filename to write to, defaults to - (stdout)")
    parser.add_option("", "--pkg",
                      action="append", dest="packages", default=[],
                      help="pkg-config packages to get cflags from")
    parser.add_option("", "--pkg-export",
                      action="append", dest="packages_export", default=[],
                      help="Associated pkg-config packages for this library")
    parser.add_option('', "--warn-all",
                      action="store_true", dest="warn_all", default=False,
                      help="If true, enable all warnings for introspection")
    parser.add_option('', "--warn-error",
                      action="store_true", dest="warn_fatal",
                      help="Turn warnings into fatal errors")
    parser.add_option('', "--strict",
                      action="store_true", dest="warn_strict", default=False,
                      help="If true, enable strict warnings for introspection")
    parser.add_option("-v", "--verbose",
                      action="store_true", dest="verbose",
                      help="be verbose")
    parser.add_option("", "--c-include",
                      action="append", dest="c_includes", default=[],
                      help="headers which should be included in C programs")
    parser.add_option("", "--filelist",
                      action="store", dest="filelist", default=[],
                      help="file containing headers and sources to be scanned")
    parser.add_option("", "--compiler",
                      action="store", dest="compiler", default=None,
                      help="the C compiler to use internally")

    group = get_preprocessor_option_group(parser)
    parser.add_option_group(group)

    msystemenv = os.environ.get('MSYSTEM')
    if msystemenv and msystemenv.startswith('MINGW'):
        group = get_windows_option_group(parser)
        parser.add_option_group(group)

    # Private options
    parser.add_option('', "--generate-typelib-tests",
                      action="store", dest="test_codegen", default=None,
                      help=optparse.SUPPRESS_HELP)
    parser.add_option('', "--passthrough-gir",
                      action="store", dest="passthrough_gir", default=None,
                      help=optparse.SUPPRESS_HELP)
    parser.add_option('', "--reparse-validate",
                      action="store_true", dest="reparse_validate_gir", default=False,
                      help=optparse.SUPPRESS_HELP)
    parser.add_option("", "--typelib-xml",
                      action="store_true", dest="typelib_xml",
                      help=optparse.SUPPRESS_HELP)
    parser.add_option("", "--function-decoration",
                      action="append", dest="function_decoration", default=[],
                      help="Macro to decorate functions in generated code")
    parser.add_option("", "--include-first-in-header",
                      action="append", dest="include_first_header", default=[],
                      help="Header to include first in generated header")
    parser.add_option("", "--include-last-in-header",
                      action="append", dest="include_last_header", default=[],
                      help="Header to include after the other headers in generated header")
    parser.add_option("", "--include-first-in-src",
                      action="append", dest="include_first_src", default=[],
                      help="Header to include first in generated sources")
    parser.add_option("", "--include-last-in-src",
                      action="append", dest="include_last_src", default=[],
                      help="Header to include after the other headers in generated sources")
    parser.add_option("", "--sources-top-dirs", default=[], action='append',
                      help="Paths to the sources directories used to determine"
                      " relative files locations to be used in the gir file."
                      " This is especially useful when build dir and source dir are different"
                      " and mirrored.")

    return parser


def _error(msg):
    raise SystemExit('ERROR: %s' % (msg, ))


def passthrough_gir(path, f):
    parser = GIRParser()
    parser.parse(path)

    writer = GIRWriter(parser.get_namespace())
    f.write(writer.get_encoded_xml())


def test_codegen(optstring,
                 function_decoration,
                 include_first_header,
                 include_last_header,
                 include_first_src,
                 include_last_src):
    (namespace, out_h_filename, out_c_filename) = optstring.split(',')
    if namespace == 'Everything':
        from .testcodegen import EverythingCodeGenerator
        gen = EverythingCodeGenerator(out_h_filename,
                                      out_c_filename,
                                      function_decoration,
                                      include_first_header,
                                      include_last_header,
                                      include_first_src,
                                      include_last_src)
        gen.write()
    else:
        _error("Invaild namespace '%s'" % (namespace, ))
    return 0


def process_options(output, allowed_flags):
    for option in output:
        for flag in allowed_flags:
            if not option.startswith(flag):
                continue
            yield option
            break


def process_packages(options, packages):
    flags = pkgconfig.cflags(packages)
    # Some pkg-config files on Windows have options we don't understand,
    # so we explicitly filter to only the ones we need.
    options_whitelist = ['-I', '-D', '-U', '-l', '-L']
    filtered_output = list(process_options(flags, options_whitelist))
    parser = _get_option_parser()
    pkg_options, unused = parser.parse_args(filtered_output)
    options.cpp_includes.extend([os.path.realpath(f) for f in pkg_options.cpp_includes])
    options.cpp_defines.extend(pkg_options.cpp_defines)
    options.cpp_undefines.extend(pkg_options.cpp_undefines)


def extract_filenames(args):
    filenames = []
    for arg in args:
        # We don't support real C++ parsing yet, but we should be able
        # to understand C API implemented in C++ files.
        if os.path.splitext(arg)[1] in ALL_EXTS:
            if not os.path.exists(arg):
                _error('%s: no such a file or directory' % (arg, ))
            # Make absolute, because we do comparisons inside scannerparser.c
            # against the absolute path that cpp will give us
            filenames.append(arg)
    return filenames


def extract_filelist(options):
    filenames = []
    if not os.path.exists(options.filelist):
        _error('%s: no such filelist file' % (options.filelist, ))
    with open(options.filelist, "r", encoding=None) as filelist_file:
        lines = filelist_file.readlines()
    for line in lines:
        # We don't support real C++ parsing yet, but we should be able
        # to understand C API implemented in C++ files.
        filename = line.strip()
        if (filename.endswith('.c') or filename.endswith('.cpp')
        or filename.endswith('.cc') or filename.endswith('.cxx')
        or filename.endswith('.h') or filename.endswith('.hpp')
        or filename.endswith('.hxx')):
            if not os.path.exists(filename):
                _error('%s: Invalid filelist entry-no such file or directory' % (line, ))
            # Make absolute, because we do comparisons inside scannerparser.c
            # against the absolute path that cpp will give us
            filenames.append(filename)
    return filenames


def create_namespace(options):
    if options.strip_prefix:
        print("""g-ir-scanner: warning: Option --strip-prefix has been deprecated;
see --identifier-prefix and --symbol-prefix.""")
        options.identifier_prefixes.append(options.strip_prefix)

    # We do this dance because the empty list has different semantics from
    # None; if the user didn't specify the options, we want to use None so
    # the Namespace constructor picks the defaults.
    if options.identifier_prefixes:
        identifier_prefixes = options.identifier_prefixes
    else:
        identifier_prefixes = None
    if options.symbol_prefixes:
        for prefix in options.symbol_prefixes:
            # See Transformer._split_c_string_for_namespace_matches() for
            # why this check is needed
            if prefix.lower() != prefix:
                _error("Values for --symbol-prefix must be entirely lowercase")
        symbol_prefixes = options.symbol_prefixes
    else:
        symbol_prefixes = None

    return Namespace(options.namespace_name,
                     options.namespace_version,
                     identifier_prefixes=identifier_prefixes,
                     symbol_prefixes=symbol_prefixes)


def create_transformer(namespace, options):
    identifier_filter_cmd = shlex.split(options.identifier_filter_cmd)
    symbol_filter_cmd = shlex.split(options.symbol_filter_cmd)
    transformer = Transformer(namespace,
                              accept_unprefixed=options.accept_unprefixed,
                              identifier_filter_cmd=identifier_filter_cmd,
                              symbol_filter_cmd=symbol_filter_cmd)
    transformer.set_include_paths(options.include_paths)
    if options.passthrough_gir or options.reparse_validate_gir:
        transformer.disable_cache()
        transformer.set_passthrough_mode()

    for include in options.includes:
        if os.sep in include:
            _error("Invalid include path '%s'" % (include, ))
        try:
            include_obj = Include.from_string(include)
        except Exception:
            _error("Malformed include '%s'\n" % (include, ))
        transformer.register_include(include_obj)
    for include_path in options.includes_uninstalled:
        transformer.register_include_uninstalled(include_path)

    return transformer


def create_binary(transformer, options, args):
    # Transform the C AST nodes into higher level
    # GLib/GObject nodes
    gdump_parser = GDumpParser(transformer)

    # Do enough parsing that we have the get_type() functions to reference
    # when creating the introspection binary
    gdump_parser.init_parse()

    if options.program:
        args = [options.program]
        args.extend(options.program_args)
        binary = IntrospectionBinary(args)
    else:
        binary = compile_introspection_binary(options,
                                              gdump_parser.get_get_type_functions(),
                                              gdump_parser.get_error_quark_functions())

    shlibs = resolve_shlibs(options, binary, options.libraries)
    if options.wrapper:
        binary.args = [options.wrapper] + binary.args

        libtool = utils.get_libtool_command(options)

        if libtool is not None:
            binary.args = libtool + ['--mode=execute'] + binary.args

    gdump_parser.set_introspection_binary(binary)
    gdump_parser.parse()
    return shlibs


def create_source_scanner(options, args):
    if hasattr(options, 'filelist') and options.filelist:
        filenames = extract_filelist(options)
    else:
        filenames = extract_filenames(args)
    filenames = [os.path.realpath(f) for f in filenames]

    if platform.system() == 'Darwin':
        options.cpp_undefines.append('__BLOCKS__')

    # Run the preprocessor, tokenize and construct simple
    # objects representing the raw C symbols
    ss = SourceScanner()
    if hasattr(options, 'compiler') and options.compiler:
        ss.set_compiler(options.compiler)
    ss.set_cpp_options(options.cpp_includes,
                       options.cpp_defines,
                       options.cpp_undefines,
                       cflags=options.cflags)
    try:
        ss.parse_files(filenames)
        ss.parse_macros(filenames)
    finally:
        for error in ss.get_errors():
            print(error, file=sys.stderr)
    return ss, filenames


def write_output(data, options):
    """Write encoded XML 'data' to the filename specified in 'options'."""
    if options.output == "-":
        output = sys.stdout.buffer
        try:
            output.write(data)
        except IOError as e:
            _error("while writing output: %s" % (e.strerror, ))
    elif options.reparse_validate_gir:
        main_f, main_f_name = tempfile.mkstemp(suffix='.gir')

        if (os.path.isfile(options.output)):
            shutil.copystat(options.output, main_f_name)
        else:
            os.chmod(main_f_name,
                     stat.S_IWUSR | stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH)

        with os.fdopen(main_f, 'wb') as main_f:
            main_f.write(data)

        temp_f, temp_f_name = tempfile.mkstemp(suffix='.gir')
        with os.fdopen(temp_f, 'wb') as temp_f:
            passthrough_gir(main_f_name, temp_f)
        if not utils.files_are_identical(main_f_name, temp_f_name):
            _error("Failed to re-parse gir file; scanned='%s' passthrough='%s'" % (
                main_f_name, temp_f_name))
        os.unlink(temp_f_name)
        try:
            shutil.move(main_f_name, options.output)
        except OSError as e:
            if e.errno == errno.EPERM:
                os.unlink(main_f_name)
            raise
        return 0
    else:
        try:
            with open(options.output, 'wb') as output:
                output.write(data)
        except IOError as e:
            _error("opening/writing output: %s" % (e.strerror, ))


def get_source_root_dirs(options, filenames):
    if options.sources_top_dirs:
        return [os.path.realpath(p) for p in options.sources_top_dirs]

    # None passed, we need to guess
    filenames = [os.path.realpath(p) for p in filenames]
    dirs = sorted(set([os.path.dirname(f) for f in filenames]))

    # We need commonpath (3.5+), otherwise give up
    if not hasattr(os.path, "commonpath"):
        return dirs

    if not dirs:
        return []

    try:
        common = os.path.commonpath(dirs)
    except ValueError:
        # ValueError: On Windows in case the paths are on different drives
        return dirs

    # If the only common path is the root directory give up
    if os.path.dirname(common) == common:
        return dirs

    return [common]


def scanner_main(args):
    parser = _get_option_parser()
    (options, args) = parser.parse_args(args)

    if options.verbose:
        import distutils
        distutils.log.set_threshold(distutils.log.DEBUG)
    if options.passthrough_gir:
        passthrough_gir(options.passthrough_gir, sys.stdout)
    if options.test_codegen:
        return test_codegen(options.test_codegen,
                            options.function_decoration,
                            options.include_first_header,
                            options.include_last_header,
                            options.include_first_src,
                            options.include_last_src)

    if hasattr(options, 'filelist') and not options.filelist:
        if len(args) <= 1:
            _error('Need at least one filename')

    if not options.namespace_name:
        _error('Namespace name missing')

    if options.format == 'gir':
        from giscanner.girwriter import GIRWriter as Writer
    else:
        _error("Unknown format: %s" % (options.format, ))

    if not (options.libraries
            or options.program
            or options.header_only):
        _error("Must specify --program or --library")

    namespace = create_namespace(options)
    logger = message.MessageLogger.get(namespace=namespace)
    if options.warn_all:
        logger.enable_warnings(True)
    if options.warn_strict:
        logger.enable_strict(True)

    transformer = create_transformer(namespace, options)

    packages = set(options.packages)
    packages.update(transformer.get_pkgconfig_packages())
    if packages:
        try:
            process_packages(options, packages)
        except pkgconfig.PkgConfigError as e:
            _error(str(e))

    ss, filenames = create_source_scanner(options, args)

    cbp = GtkDocCommentBlockParser()
    blocks = cbp.parse_comment_blocks(ss.get_comments())

    # Transform the C symbols into AST nodes
    transformer.parse(ss.get_symbols())

    if not options.header_only:
        shlibs = create_binary(transformer, options, args)
    else:
        shlibs = []

    transformer.namespace.shared_libraries = shlibs

    main = MainTransformer(transformer, blocks)
    main.transform()

    utils.break_on_debug_flag('tree')

    final = IntrospectablePass(transformer, blocks)
    final.validate()

    show_suppression = options.warn_all is False and options.warn_strict is False and options.quiet is False
    warning_count = logger.get_warning_count()
    if options.warn_fatal and warning_count > 0:
        message.fatal("warnings configured as fatal")
        return 1
    elif warning_count > 0 and show_suppression:
        print("g-ir-scanner: %s: warning: %d warnings suppressed "
              "(use --warn-all to see them)" %
              (transformer.namespace.name, warning_count, ))

    # Write out AST
    if options.packages_export:
        exported_packages = options.packages_export
    else:
        exported_packages = options.packages

    transformer.namespace.c_includes = options.c_includes
    transformer.namespace.exported_packages = exported_packages

    sources_top_dirs = get_source_root_dirs(options, filenames)
    writer = Writer(transformer.namespace, sources_top_dirs)
    data = writer.get_encoded_xml()

    write_output(data, options)

    return 0
