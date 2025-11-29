#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import re
import os
from collections import *

def add_quotes(s):
    return "\"" + s.lower() + "\""

def wordify(s):
    return s.strip().rstrip(".,;:")

def is_keyword(s):
    if s == "GTK":
      return False
    elif s.startswith(("Gtk", "Gdk", "Pango")):
        return True
    elif s.startswith("G") and s[1].isupper():
        return True
    else:
        return False

out_file = sys.argv[1]
in_files = sys.argv[2:]


file_output = """
typedef GtkWidget *(*GDoDemoFunc) (GtkWidget *do_widget);

typedef struct _DemoData DemoData;

struct _DemoData
{
  const char *name;
  const char *title;
  const char **keywords;
  const char *filename;
  GDoDemoFunc func;
  DemoData *children;
};
"""

# Demo = namedtuple('Demo', ['name', 'title', 'keywords', 'file', 'func'])

demos = []

for demo_file in in_files:
    filename =  demo_file[demo_file.rfind('/')+1:]
    demo_name = filename.replace(".c", "")
    with open(demo_file, 'r', encoding='utf-8') as f:
        title = f.readline().replace("/*", "").strip()
        keywords = set()
        line = f.readline().strip();
        while not line.endswith('*/'):
            if line.startswith("* #Keywords:"):
                keywords = keywords.union(set(map(wordify, line.replace ("* #Keywords:", "").strip().split(","))))
            else:
                keywords = keywords.union(set(filter(is_keyword, map(wordify, line.replace ("* ", "").split()))))
            line = f.readline().strip()

    file_output += "GtkWidget *do_" + demo_name + " (GtkWidget *do_widget);\n"
    demos.append((demo_name, title, keywords, filename, "do_" + demo_name, -1))

# Generate a List of "Parent names"
parents = []
parent_ids = []
parent_index = 0
for demo in demos:
    if "/" in demo[1]:
        slash_index = demo[1].index('/')
        parent_name = demo[1][:slash_index]
        do_break = False

        # Check for duplicates
        if not parent_name in parents:
            parents.append(parent_name)
            parent_ids.append(parent_index)
            demos.append(("NULL", parent_name, set(), "NULL", "NULL", parent_index))

        parent_index = parent_index + 1


# For every child with a parent, generate a list of child demos
i = 0
for parent in parents:
    id = parent_ids[i]
    file_output += "\nDemoData child" + str(id) + "[] = {\n"
    # iterate over all demos and check if the name starts with the given parent name
    for child in demos:
        if child[1].startswith(parent + "/"):
            title = child[1][child[1].rfind('/') + 1:]
            file_output += "  { \"" + child[0] + "\", \"" + title + "\", " + "(const char*[]) {" + ", ".join(list(map(add_quotes, child[2])) + ["NULL"]) + " }, \"" + child[3] + "\", " + child[4] + ", NULL },\n"

    file_output += "  { NULL }\n};\n"
    i = i + 1


# Sort demos by title
demos = sorted(demos, key=lambda x: x[1])

file_output += "\nDemoData gtk_demos[] = {\n"
for demo in demos:
    # Do not generate one of these for demos with a parent demo
    if "/" not in demo[1]:
        child_array = "NULL"
        name = demo[0]
        title = demo[1]
        keywords = demo[2]
        file = demo[3]
        if name != "NULL":
            name = "\"" + name + "\""
        if title != "NULL":
            title = "\"" + title + "\""
        if file != "NULL":
            file = "\"" + file + "\""

        if demo[5] != -1:
            child_array = "child" + str(demo[5])
        file_output += "  { " + name + ", " + title + ", " + "(const char*[]) {" + ", ".join(list(map(add_quotes, keywords)) + ["NULL"]) + " }, " + file + ", " + demo[4] + ", " + child_array + " },\n"

file_output += "  { NULL }\n};\n"

ofile = open(out_file, "w")
ofile.write(file_output)
ofile.close()
