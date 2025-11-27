import unittest

try:
    import mako
except ImportError:
    HAS_MAKO = False
else:
    HAS_MAKO = True
    mako
    from giscanner.docwriter import DocWriter
    from giscanner.transformer import Transformer


@unittest.skipUnless(HAS_MAKO, "mako missing")
class TestDocWriter(unittest.TestCase):

    def test_main(self):
        transformer = Transformer(None)
        DocWriter(transformer, "gjs", "devdocs")


if __name__ == '__main__':
    unittest.main()
