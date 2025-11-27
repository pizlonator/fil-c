import os
import sys
import difflib
import builtins

path = os.getenv('UNINSTALLED_INTROSPECTION_SRCDIR', None)
assert path is not None
# Not correct, but enough to get the tests going uninstalled
builtins.__dict__['DATADIR'] = path
builtins.__dict__['GIR_DIR'] = path

path = os.getenv('TOP_BUILDDIR', None)
assert path is not None
sys.path.insert(0, path)

from giscanner.annotationparser import GtkDocCommentBlockParser
from giscanner.ast import Include, Namespace
from giscanner.introspectablepass import IntrospectablePass
from giscanner.maintransformer import MainTransformer
from giscanner.message import MessageLogger
from giscanner.sourcescanner import SourceScanner
from giscanner.transformer import Transformer
from giscanner.scannermain import process_packages


currentdir = os.path.dirname(os.path.abspath(sys.argv[0]))
current_name = os.path.basename(currentdir)
path = os.path.abspath(os.path.join(currentdir, '..', ''))
top_srcdir = os.environ['UNINSTALLED_INTROSPECTION_SRCDIR']
top_builddir = os.environ['TOP_BUILDDIR']


class ChunkedIO(object):
    def __init__(self):
        self.buffer = []

    def write(self, s):
        self.buffer.append(s)

    def getvalue(self):
        return self.buffer


class Options:
    def __init__(self):
        self.cpp_includes = []
        self.cpp_defines = []
        self.cpp_undefines = []
        self.library_paths = []


def _diff(a, b):
    retval = ''
    started = False

    for group in difflib.SequenceMatcher(None, a, b).get_grouped_opcodes(3):
        if not started:
            started = True
            retval += '--- expected\n'
            retval += '+++ emitted\n'

        for tag, i1, i2, j1, j2 in group:
            if tag == 'equal':
                for line in a[i1:i2]:
                    for l in line.split('\n'):
                        retval += ' ' + l + '\n'
                continue

            if tag in ('replace', 'delete'):
                for line in a[i1:i2]:
                    for l in line.split('\n'):
                        retval += '-' + l + '\n'

            if tag in ('replace', 'insert'):
                for line in b[j1:j2]:
                    for l in line.split('\n'):
                        retval += '+' + l + '\n'

    return retval


def _extract_expected(filename):
    fd = open(filename, 'r', encoding='utf-8')
    data = fd.read()
    fd.close()

    retval = []
    for line in data.split('\n'):
        if line.startswith('// EXPECT:'):
            retval.append(line[10:] + '\n')
        elif line.startswith('//+'):
            retval[-1] += line[3:] + '\n'

    return retval


def check(args):
    filename = args[0]

    output = ChunkedIO()
    namespace = Namespace('Test', '1.0')
    logger = MessageLogger.get(namespace=namespace, output=output)
    logger.enable_warnings(True)

    transformer = Transformer(namespace)
    transformer.set_include_paths([
        os.path.join(top_srcdir, 'gir'),
        top_builddir,
        os.path.join(top_builddir, 'gir'),
    ])
    transformer.register_include(Include.from_string('GObject-2.0'))

    ss = SourceScanner()

    options = Options()
    exit_code = process_packages(options, ['gobject-2.0'])
    if exit_code:
        sys.exit(exit_code)
    ss.set_cpp_options(options.cpp_includes, options.cpp_defines, options.cpp_undefines)
    ss.parse_files([filename])
    ss.parse_macros([filename])
    transformer.parse(ss.get_symbols())

    cbp = GtkDocCommentBlockParser()
    blocks = cbp.parse_comment_blocks(ss.get_comments())

    main = MainTransformer(transformer, blocks)
    main.transform()

    final = IntrospectablePass(transformer, blocks)
    final.validate()

    emitted_warnings = [w[w.find(':') + 1:] for w in output.getvalue()]

    expected_warnings = _extract_expected(filename)

    sortkey = lambda x: int(x.split(':')[0])
    expected_warnings.sort(key=sortkey)
    emitted_warnings.sort(key=sortkey)

    if len(expected_warnings) != len(emitted_warnings):
        raise SystemExit("ERROR in '%s': %d warnings were emitted, "
                         "expected %d:\n%s" % (os.path.basename(filename),
                                               len(emitted_warnings),
                                               len(expected_warnings),
                                               _diff(expected_warnings, emitted_warnings)))

    for emitted_warning, expected_warning in zip(emitted_warnings, expected_warnings):
        if expected_warning != emitted_warning:
            raise SystemExit("ERROR in '%s': expected warning does not match emitted "
                             "warning:\n%s" % (filename,
                                               _diff([expected_warning], [emitted_warning])))


sys.exit(check(sys.argv[1:]))
