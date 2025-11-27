import unittest

from giscanner.xmlwriter import XMLWriter, collect_attributes, build_xml_tag


class TestXMLWriter(unittest.TestCase):

    def test_various(self):
        w = XMLWriter()
        w.push_tag('repository')
        w.push_tag('namespace')
        w.push_tag('enumeration')
        w.push_tag('member',
                   [('name', 'west'),
                    ('value', '7'),
                    ('c:identifier', 'GTK_ANCHOR_WEST'),
                    ('glib:nick', 'west')])

        w.pop_tag()
        w.pop_tag()
        w.pop_tag()
        x = w.get_xml()
        self.assertTrue('GTK_ANCHOR_WEST' in x)
        lines = x.split('\n')
        self.assertTrue(len(lines[3]) < 80)

    def test_collect_attributes(self):
        ca = collect_attributes
        res = ca('parameters', [], 6, ' ', 12)
        self.assertEqual(res, "")

        res = ca('type', [('name', 'utf8')], 12, ' ', 7)
        self.assertEqual(res, ' name="utf8"')

        res = ca('type', [('name', 'GLib.SList'), ('c:type', 'const GSList*')], 8, ' ', 6)
        self.assertEqual(res, ' name="GLib.SList" c:type="const GSList*"')

    def test_build_xml_tag(self):
        res = build_xml_tag('tag', [('attr', 'utf8')])
        self.assertEqual(res, '<tag attr="utf8"/>')

        res = build_xml_tag('tag', [('attr', 'foo\nbar')])
        self.assertEqual(res, '<tag attr="foo&#10;bar"/>')

        res = build_xml_tag('tag', [('attr', 'foo\tbar')])
        self.assertEqual(res, '<tag attr="foo&#9;bar"/>')

        res = build_xml_tag('tag', [('attr', '\004')])
        self.assertEqual(res, '<tag attr="\x04"/>')

        res = build_xml_tag('tag', [('attr', 'limba1\t\034')])
        self.assertEqual(res, '<tag attr="limba1&#9;\034"/>')

        res = build_xml_tag('tag', [('attr', '')])
        self.assertEqual(res, '<tag attr=""/>')

        res = build_xml_tag('tag', [('attr', ' ')])
        self.assertEqual(res, '<tag attr=" "/>')

        res = build_xml_tag('tag', [('attr', '>&<')])
        self.assertEqual(res, '<tag attr="&gt;&amp;&lt;"/>')

        res = build_xml_tag('tag', [('a', 'b'), ('c', 'd')])
        self.assertEqual(res, '<tag a="b" c="d"/>')

        res = build_xml_tag('tag', [('foo', None), ('bar', 'quux')])
        self.assertEqual(res, '<tag bar="quux"/>')

    def test_build_xml_tag_data(self):
        res = build_xml_tag('tag', [], b'foo')
        self.assertEqual(res, '<tag>foo</tag>')

        res = build_xml_tag('tag', [], u'\xf6\xe4\xfc')
        self.assertEqual(res, u'<tag>\xf6\xe4\xfc</tag>')

        res = build_xml_tag('tag', [], '>&<')
        self.assertEqual(res, '<tag>&gt;&amp;&lt;</tag>')

    def test_build_xml_tag_indent(self):
        res = build_xml_tag(
            'tag', [('a' * 10, 'b' * 30), ('c' * 30, 'd' * 10)], None)
        self.assertEqual(res, '''\
<tag aaaaaaaaaa="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
     cccccccccccccccccccccccccccccc="dddddddddd"/>''')

        res = build_xml_tag(
            'tag', [('a' * 10, 'b' * 30), ('c' * 30, 'd' * 10)], None, 3)
        self.assertEqual(res, '''\
<tag aaaaaaaaaa="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        cccccccccccccccccccccccccccccc="dddddddddd"/>''')


if __name__ == '__main__':
    unittest.main()
