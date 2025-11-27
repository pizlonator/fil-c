import unittest
import tempfile
import os

os.environ['GI_SCANNER_DISABLE_CACHE'] = '1'

from giscanner import ast
from giscanner.sourcescanner import SourceScanner
from giscanner.transformer import Transformer
from giscanner.annotationparser import GtkDocCommentBlockParser
from giscanner.maintransformer import MainTransformer


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
    transformer = Transformer(namespace)
    transformer.parse(ss.get_symbols())
    cbp = GtkDocCommentBlockParser()
    blocks = cbp.parse_comment_blocks(ss.get_comments())
    main = MainTransformer(transformer, blocks)
    main.transform()


class TestDeprecations(unittest.TestCase):

    def test_record_deprecated(self):
        namespace = ast.Namespace('Test', '1.0')
        load_namespace_from_source_string(namespace, """
            /** TestStruct:
            * foo
            * Deprecated: 1.2: something
            */
            typedef struct _TestStruct TestStruct;
            """)

        node = namespace.get('Struct')
        self.assertEqual(node.doc, "foo")
        self.assertEqual(node.deprecated, "1.2")
        self.assertEqual(node.deprecated_doc, "something")

    def test_record_deprecated_from_section(self):
        namespace = ast.Namespace('Test', '1.0')
        load_namespace_from_source_string(namespace, """
            /** SECTION:teststruct
            * foo
            * Deprecated: 1.2: something
            */
            typedef struct _TestStruct TestStruct;
            """)

        node = namespace.get('Struct')
        self.assertEqual(node.doc, "foo")
        self.assertEqual(node.deprecated, "1.2")
        self.assertEqual(node.deprecated_doc, "something")


if __name__ == '__main__':
    unittest.main()
