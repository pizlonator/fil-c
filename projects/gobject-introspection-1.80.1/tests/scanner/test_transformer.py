import unittest
import tempfile
import os
import sys
import textwrap


os.environ['GI_SCANNER_DISABLE_CACHE'] = '1'

from giscanner import ast
from giscanner.sourcescanner import SourceScanner
from giscanner.transformer import Transformer
from giscanner.message import MessageLogger


def create_scanner_from_source_string(source):
    ss = SourceScanner()
    tmp_fd, tmp_name = tempfile.mkstemp(suffix='.h', text=True)

    try:
        with os.fdopen(tmp_fd, 'w', encoding='utf-8') as file:
            file.write(source)
        ss.parse_files([tmp_name])
    finally:
        os.unlink(tmp_name)

    return ss


def load_namespace_from_source_string(namespace, source):
    ss = create_scanner_from_source_string(source)
    xformer = Transformer(namespace)
    xformer.parse(ss.get_symbols())


class TestIdentifierFilter(unittest.TestCase):
    def test_underscore_t_identifier_filter(self):
        cmd = [sys.executable, '-c', textwrap.dedent("""
            import sys, re
            for line in sys.stdin:
                line = re.sub(r"^test_t$", "TestContext", line)
                line = re.sub(r"_t$", "", line)
                line = re.sub(r"^test_", "Test_", line)
                line = re.sub(r"_([a-z])", lambda m: m.group(1).title(),
                              line)
                sys.stdout.write(line)""")]

        namespace = ast.Namespace('Test', '1.0')
        xformer = Transformer(namespace, identifier_filter_cmd=cmd)

        self.assertEqual(xformer.strip_identifier('test_t'), 'Context')
        self.assertEqual(xformer.strip_identifier('test_foo_t'), 'Foo')
        self.assertEqual(xformer.strip_identifier('test_foot'), 'Foot')
        self.assertEqual(xformer.strip_identifier('test_foo_bart'), 'FooBart')
        self.assertEqual(xformer.strip_identifier('test_foo_tart'), 'FooTart')

    def test_invalid_command(self):
        cmd = ['this-is-not-a-real-command']
        namespace = ast.Namespace('Test', '1.0')
        xformer = Transformer(namespace, identifier_filter_cmd=cmd)
        self.assertRaises(OSError, xformer.strip_identifier, 'test_t')

    def test_invalid_argument(self):
        cmd = [sys.executable, '--not-a-valid-argument']
        namespace = ast.Namespace('Test', '1.0')
        xformer = Transformer(namespace, identifier_filter_cmd=cmd)
        self.assertRaises(ValueError, xformer.strip_identifier, 'test_t')


class TestSymbolFilter(unittest.TestCase):

    def test_split_csymbol(self):
        cmd = [
            sys.executable, '-c',
            'import sys; sys.stdout.write("test_" + sys.stdin.read())']
        namespace = ast.Namespace('Test', '1.0')
        xformer = Transformer(namespace, symbol_filter_cmd=cmd)

        self.assertEqual(
            xformer.split_csymbol('foo_bar_quux')[1], "foo_bar_quux")


class TestStructTypedefs(unittest.TestCase):
    def setUp(self):
        # Hack to set logging singleton
        self.namespace = ast.Namespace('Test', '1.0')
        logger = MessageLogger.get(namespace=self.namespace)
        logger.enable_warnings(True)

    def test_anonymous_typedef(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct {
                int value;
            } TestStruct;
            """)
        self.assertEqual(len(self.namespace.names), 1)
        node = self.namespace.get('Struct')
        self.assertIsNotNone(node)
        self.assertIsInstance(node, ast.Record)
        self.assertFalse(node.opaque)
        self.assertFalse(node.pointer)
        self.assertFalse(node.disguised)
        self.assertEqual(len(node.fields), 1)

    def test_typedef_before(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct _TestStruct TestStruct;
            struct _TestStruct {
                int value;
            };
            """)
        self.assertEqual(len(self.namespace.names), 1)
        node = self.namespace.get('Struct')
        self.assertIsNotNone(node)
        self.assertIsInstance(node, ast.Record)
        self.assertFalse(node.opaque)
        self.assertFalse(node.pointer)
        self.assertFalse(node.disguised)
        self.assertEqual(len(node.fields), 1)

    def test_typedef_after(self):
        load_namespace_from_source_string(self.namespace, """
            struct _TestStruct {
                int value;
            };
            typedef struct _TestStruct TestStruct;
            """)
        self.assertEqual(len(self.namespace.names), 1)
        node = self.namespace.get('Struct')
        self.assertIsNotNone(node)
        self.assertIsInstance(node, ast.Record)
        self.assertFalse(node.opaque)
        self.assertFalse(node.pointer)
        self.assertFalse(node.disguised)
        self.assertEqual(len(node.fields), 1)

    def test_tag_and_typedef(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct _TestStruct {
                int value;
            } TestStruct;
            """)
        self.assertEqual(len(self.namespace.names), 1)
        node = self.namespace.get('Struct')
        self.assertIsNotNone(node)
        self.assertIsInstance(node, ast.Record)
        self.assertFalse(node.opaque)
        self.assertFalse(node.pointer)
        self.assertFalse(node.disguised)
        self.assertEqual(len(node.fields), 1)

    def test_struct_tag_only(self):
        load_namespace_from_source_string(self.namespace, """
            struct TestStruct {
                int value;
            };
            """)
        self.assertEqual(len(self.namespace.names), 1)
        node = self.namespace.get('Struct')
        self.assertIsNotNone(node)
        self.assertIsInstance(node, ast.Record)
        self.assertFalse(node.opaque)
        self.assertFalse(node.pointer)
        self.assertFalse(node.disguised)
        self.assertEqual(len(node.fields), 1)

    def test_struct_aliases(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct {
                int value;
            } TestStruct;
            typedef TestStruct TestStructAlias1;
            typedef TestStruct TestStructAlias2;
            """)
        self.assertEqual(len(self.namespace.names), 3)

        node = self.namespace.get('Struct')
        self.assertIsNotNone(node)
        self.assertIsInstance(node, ast.Record)
        self.assertFalse(node.opaque)
        self.assertFalse(node.pointer)
        self.assertFalse(node.disguised)
        self.assertEqual(len(node.fields), 1)

        alias = self.namespace.get('StructAlias1')
        self.assertIsInstance(alias, ast.Alias)
        self.assertEqual(alias.target, node)
        self.assertEqual(alias.ctype, 'TestStructAlias1')

        alias = self.namespace.get('StructAlias2')
        self.assertIsInstance(alias, ast.Alias)
        self.assertEqual(alias.target, node)
        self.assertEqual(alias.ctype, 'TestStructAlias2')

    def test_struct_tag_aliases_before(self):
        # This test is similar to how GObject and GInitiallyUnowned are setup
        load_namespace_from_source_string(self.namespace, """
            typedef struct _TestStruct TestStruct;
            typedef struct _TestStruct TestStructAlias;
            struct _TestStruct {
                int value;
            };
            """)
        self.assertEqual(len(self.namespace.names), 2)

        node = self.namespace.get('Struct')
        self.assertIsNotNone(node)
        self.assertIsInstance(node, ast.Record)
        self.assertFalse(node.opaque)
        self.assertFalse(node.pointer)
        self.assertFalse(node.disguised)
        self.assertEqual(len(node.fields), 1)
        self.assertEqual(node.ctype, 'TestStruct')

        shared = self.namespace.get('StructAlias')
        self.assertIsNotNone(shared)
        self.assertIsInstance(shared, ast.Record)
        self.assertFalse(shared.opaque)
        self.assertFalse(shared.pointer)
        self.assertFalse(shared.disguised)
        self.assertEqual(len(shared.fields), 1)
        self.assertEqual(shared.ctype, 'TestStructAlias')

    def test_struct_tag_aliases_after(self):
        load_namespace_from_source_string(self.namespace, """
            struct _TestStruct {
                int value;
            };
            typedef struct _TestStruct TestStruct;
            typedef struct _TestStruct TestStructAlias;
            """)
        self.assertEqual(len(self.namespace.names), 2)

        node = self.namespace.get('Struct')
        self.assertIsNotNone(node)
        self.assertIsInstance(node, ast.Record)
        self.assertFalse(node.opaque)
        self.assertFalse(node.pointer)
        self.assertFalse(node.disguised)
        self.assertEqual(len(node.fields), 1)
        self.assertEqual(node.ctype, 'TestStruct')

        shared = self.namespace.get('StructAlias')
        self.assertIsNotNone(shared)
        self.assertIsInstance(shared, ast.Record)
        self.assertFalse(shared.opaque)
        self.assertFalse(shared.pointer)
        self.assertFalse(shared.disguised)
        self.assertEqual(len(shared.fields), 1)
        self.assertEqual(shared.ctype, 'TestStructAlias')

    def test_struct_pointer(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct {
                int value;
            } TestStruct;
            typedef TestStruct* TestStructPtr;
            """)
        self.assertEqual(len(self.namespace.names), 2)

        node = self.namespace.get('Struct')
        self.assertIsNotNone(node)
        self.assertIsInstance(node, ast.Record)
        self.assertEqual(len(node.fields), 1)
        self.assertEqual(node.ctype, 'TestStruct')

        ptr = self.namespace.get('StructPtr')
        self.assertIsNotNone(ptr)
        self.assertIsInstance(ptr, ast.Alias)
        self.assertEqual(ptr.ctype, 'TestStructPtr')

    def test_struct_tag_pointer(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct _TestStruct TestStruct;
            struct _TestStruct{
                int value;
            };
            typedef struct _TestStruct* TestStructPtr;
            """)
        self.assertEqual(len(self.namespace.names), 2)

        node = self.namespace.get('Struct')
        self.assertIsNotNone(node)
        self.assertIsInstance(node, ast.Record)
        self.assertFalse(node.opaque)
        self.assertFalse(node.pointer)
        self.assertFalse(node.disguised)
        self.assertEqual(node.ctype, 'TestStruct')
        self.assertEqual(len(node.fields), 1)

        ptr = self.namespace.get('StructPtr')
        self.assertIsNotNone(ptr)
        # This currently gives a disguised Record instead of an Alias
        self.assertIsInstance(ptr, ast.Record)
        self.assertFalse(ptr.opaque)
        self.assertTrue(ptr.pointer)
        self.assertTrue(ptr.disguised)
        self.assertEqual(len(ptr.fields), 0)
        self.assertEqual(ptr.ctype, 'TestStructPtr')

    def test_opaque_struct(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct _TestStruct TestStruct;
            """)
        self.assertEqual(len(self.namespace.names), 1)

        node = self.namespace.get('Struct')
        self.assertIsNotNone(node)
        self.assertIsInstance(node, ast.Record)
        self.assertTrue(node.opaque)
        self.assertFalse(node.pointer)
        self.assertTrue(node.disguised)
        self.assertEqual(node.ctype, 'TestStruct')
        self.assertFalse(node.fields)


class TestNestedStructs(unittest.TestCase):
    def setUp(self):
        # Hack to set logging singleton
        self.namespace = ast.Namespace('Test', '1.0')
        logger = MessageLogger.get(namespace=self.namespace)
        logger.enable_warnings(True)

    def test_nested_named(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct {
                int value;
            } TestSimpleStruct;

            typedef struct {
                TestSimpleStruct nested_struct;
            } TestStructWithNested;
            """)
        self.assertEqual(len(self.namespace.names), 2)
        node = self.namespace.get('StructWithNested')
        self.assertEqual(len(node.fields), 1)

        simple = self.namespace.get('SimpleStruct')
        self.assertIsNotNone(node)

        field = node.fields[0]
        self.assertIsNotNone(field)
        self.assertIsInstance(field, ast.Field)
        self.assertEqual(field.type, simple)
        self.assertEqual(field.name, 'nested_struct')

    def test_nested_anonymous(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct {
                struct {
                    int value;
                };
            } TestStructWithNestedAnon;
            """)
        self.assertEqual(len(self.namespace.names), 1)
        node = self.namespace.get('StructWithNestedAnon')
        self.assertEqual(len(node.fields), 1)

        field = node.fields[0]
        self.assertIsNotNone(field)
        self.assertIsInstance(field, ast.Field)
        self.assertIsNone(field.name)

        anon = field.anonymous_node
        self.assertIsNotNone(anon)
        self.assertIsInstance(anon, ast.Record)
        self.assertEqual(len(anon.fields), 1)

        anon_field = anon.fields[0]
        self.assertIsNotNone(anon_field)
        self.assertIsInstance(anon_field, ast.Field)
        self.assertEqual(anon_field.name, 'value')

    def test_nested(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct {
                struct {
                    int value;
                } nested;
            } TestStructWithNested;
            """)
        self.assertEqual(len(self.namespace.names), 1)
        node = self.namespace.get('StructWithNested')
        self.assertEqual(len(node.fields), 1)

        field = node.fields[0]
        self.assertIsNotNone(field)
        self.assertIsInstance(field, ast.Field)
        self.assertEqual(field.name, 'nested')

        nested = field.anonymous_node
        self.assertIsNotNone(nested)
        self.assertIsInstance(nested, ast.Record)
        self.assertEqual(len(nested.fields), 1)
        self.assertEqual(nested.name, 'nested')

        nested_field = nested.fields[0]
        self.assertIsNotNone(nested_field)
        self.assertIsInstance(nested_field, ast.Field)
        self.assertEqual(nested_field.name, 'value')

    def test_struct_ptr(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct {
                int value;
            } TestSimpleStruct;

            typedef struct {
                TestSimpleStruct *struct_ptr;
            } TestStructWithNestedPtr;
            """)
        self.assertEqual(len(self.namespace.names), 2)
        node = self.namespace.get('StructWithNestedPtr')
        self.assertEqual(len(node.fields), 1)

        simple = self.namespace.get('SimpleStruct')
        self.assertIsNotNone(simple)

        field = node.fields[0]
        self.assertIsNotNone(field)
        self.assertIsInstance(field, ast.Field)
        self.assertEqual(field.type.ctype, 'TestSimpleStruct*')
        self.assertEqual(field.name, 'struct_ptr')


class TestUnions(unittest.TestCase):
    def setUp(self):
        # Hack to set logging singleton
        self.namespace = ast.Namespace('Test', '1.0')
        logger = MessageLogger.get(namespace=self.namespace)
        logger.enable_warnings(True)

    def test_union_with_struct(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct {
                int value;
            } TestSimpleStruct;

            typedef union {
                TestSimpleStruct nested_struct;
                int value;
            } TestUnionWithNested;
            """)
        self.assertEqual(len(self.namespace.names), 2)
        node = self.namespace.get('UnionWithNested')
        self.assertEqual(len(node.fields), 2)

        simple = self.namespace.get('SimpleStruct')
        self.assertIsNotNone(node)

        field = node.fields[0]
        self.assertEqual(field.type, simple)
        self.assertEqual(field.name, 'nested_struct')

        field = node.fields[1]
        self.assertEqual(field.type.ctype, 'int')
        self.assertEqual(field.name, 'value')

    def test_nested(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct {
                union {
                    int ival;
                    float fval;
                } nested;
            } TestNestedUnion;
            """)
        self.assertEqual(len(self.namespace.names), 1)
        node = self.namespace.get('NestedUnion')
        self.assertEqual(len(node.fields), 1)

        field = node.fields[0]
        self.assertEqual(field.name, 'nested')

        nested = field.anonymous_node
        self.assertIsInstance(nested, ast.Union)
        self.assertEqual(nested.name, 'nested')
        self.assertEqual(len(nested.fields), 2)

        self.assertEqual(nested.fields[0].name, 'ival')
        self.assertEqual(nested.fields[1].name, 'fval')

    def test_nested_anonymous(self):
        load_namespace_from_source_string(self.namespace, """
            typedef struct {
                union {
                    int ival;
                    float fval;
                };
            } TestStructWithNestedUnion;
            """)
        self.assertEqual(len(self.namespace.names), 1)
        node = self.namespace.get('StructWithNestedUnion')
        self.assertEqual(len(node.fields), 1)

        field = node.fields[0]
        self.assertIsNone(field.name)

        nested = field.anonymous_node
        self.assertIsInstance(nested, ast.Union)
        self.assertIsNone(nested.name)
        self.assertEqual(len(nested.fields), 2)

        self.assertEqual(nested.fields[0].name, 'ival')
        self.assertEqual(nested.fields[0].type.ctype, 'int')

        self.assertEqual(nested.fields[1].name, 'fval')
        self.assertEqual(nested.fields[1].type.ctype, 'float')


class TestCallbacks(unittest.TestCase):
    def setUp(self):
        # Hack to set logging singleton
        self.namespace = ast.Namespace('Test', '1.0')
        logger = MessageLogger.get(namespace=self.namespace)
        logger.enable_warnings(True)

    def test_union_with_struct(self):
        load_namespace_from_source_string(self.namespace, """
            typedef void (*TestCallback)(int value);
            """)
        self.assertEqual(len(self.namespace.names), 1)
        node = self.namespace.get('Callback')
        self.assertIsInstance(node, ast.Callback)


class TestArrays(unittest.TestCase):
    def setUp(self):
        # Hack to set logging singleton
        self.namespace = ast.Namespace('Test', '1.0')
        logger = MessageLogger.get(namespace=self.namespace)
        logger.enable_warnings(True)

    def test_multidimensional_arrays(self):
        """Multidimensional arrays are flattend into a single dimension."""

        load_namespace_from_source_string(self.namespace, """
            typedef struct {
              int data[2][3][5][7][11];
            } TestMultiDimArray;
            """)

        node = self.namespace.get('MultiDimArray')
        self.assertIsNotNone(node)

        field = node.fields[0]
        self.assertIsInstance(field, ast.Field)
        self.assertIsInstance(field.type, ast.Array)
        self.assertEqual(field.type.element_type, ast.TYPE_INT)
        self.assertEqual(field.type.size, 2 * 3 * 5 * 7 * 11)

    def test_flexible_array_member(self):
        """Flexible array members don't break transformer.

        They are generally unsupported, so nothing else is validated.
        """

        load_namespace_from_source_string(self.namespace, """
            typedef struct {
              int length;
              unsigned char data[];
            } TestFlexibleArray;
            """)

        node = self.namespace.get('FlexibleArray')
        self.assertIsNotNone(node)


if __name__ == '__main__':
    unittest.main()
