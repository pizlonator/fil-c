#!/usr/bin/env python3

# Example invocation
# cd /path/to/gobject-introspection
# python3 misc/update-vulkan-gir.py -i /usr/include/vulkan/vulkan.h -o gir/Vulkan-1.0.gir --extra-cpp-args='-I/path/to/pycparser/utils/fake_libc_include/'

import sys
import argparse

from pycparser import c_parser, c_ast, parse_file

class TypedefVisitor(c_ast.NodeVisitor):
    def __init__ (self, f):
        super().__init__()
        self.f = f

    def visit_Typedef(self, node):
        if node.name.startswith("Vk"):
            self.f.write("<record name=\"" + str(node.name)[2:] + "\" c:type=\"" + str(node.name) + "\"/>\n")

def show_func_defs(filename, output, extra_cpp_args=None):
    # Note that cpp is used. Provide a path to your own cpp or
    # make sure one exists in PATH.
    ast = parse_file(filename, use_cpp=True,
                     cpp_args=extra_cpp_args)

    with open(output, 'w', encoding='utf-8') as f:
        f.write("""<?xml version="1.0"?>
<repository version="1.2"
            xmlns="http://www.gtk.org/introspection/core/1.0"
            xmlns:c="http://www.gtk.org/introspection/c/1.0">
  <namespace name="Vulkan" version="1.0"
	     c:identifier-prefixes="VK"
	     c:symbol-prefixes="vk">
""")
        v = TypedefVisitor(f)
        v.visit(ast)
        f.write("""
  </namespace>
</repository>
""")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Parse Vulkan header files to produce Vulkan-1.0.gir")
    parser.add_argument('-i', '--input', type=str,
        help='The input vulkan header to process')
    parser.add_argument('--extra-cpp-args', type=str, default=None,
        help='Extra arguments to parse to the C Preprocesser.  e.g. the location pycparser\'s fake libc includes')
    parser.add_argument('-o', '--output', type=str, default='Vulkan-1.0.gir',
        help='The output Vulkan-1.0.gir location')
    args = parser.parse_args(sys.argv[1:])
    show_func_defs(args.input, args.output, args.extra_cpp_args)
