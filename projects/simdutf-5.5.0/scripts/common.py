import textwrap
import sys
if sys.version_info[0] < 3:
    print('You need to run this with Python 3')
    sys.exit(1)

indent = ' ' * 4

def fill(text):
    tmp = textwrap.fill(text)
    return textwrap.indent(tmp, indent)

def filltab(text):
    tmp = textwrap.fill(text, width=120)
    return textwrap.indent(tmp, '\t')

def cpp_array_initializer(arr):
    return '{%s}' % (', '.join(map(str, arr)))

def compose2(f, g):
    return lambda x: f(g(x))

def cpp_arrayarray_initializer(arr):
    return '{%s}' % (',\n '.join(map(compose2(filltab,cpp_array_initializer), arr)))
