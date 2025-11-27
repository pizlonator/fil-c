from markdown.extensions import Extension
from markdown.treeprocessors import Treeprocessor


class RemoveOuterP(Treeprocessor):
    def run(self, root):
        if len(root) == 1 and root[0].tag == "p":
            root[0].tag = "span"


class InlineMarkdown(Extension):
    def extendMarkdown(self, md, md_globals):
        md.treeprocessors.add("remove_outer_p", RemoveOuterP(md), "_end")
