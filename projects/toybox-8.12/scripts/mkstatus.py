#!/usr/bin/python

# Create status.html

import subprocess,sys

def readit(args, shell=False):
  ret={}
  arr=[]
  blob=subprocess.Popen(args, stdout=subprocess.PIPE, shell=shell)
  for i in blob.stdout.read().split("\n"):
    if not i: continue
    i=i.split()
    try: ret[i[0]].extend(i[1:])
    except: ret[i[0]]=i[1:]
    arr.extend(i)
  return ret,arr

# Run sed on roadmap and source to get command lists, and run toybox too
# This gives us a dictionary of types, each with a list of commands

print "Collecting data..."

stuff,blah=readit(["sed","-n", 's/<span id=\\([a-z_]*\\)>/\\1 /;t good;d;:good;h;:loop;n;s@</span>@@;t out;H;b loop;:out;g;s/\\n/ /g;p', "www/roadmap.html"])
blah,toystuff=readit(["./toybox"])
blah,stuff["shell"]=readit(["sed", "-n", "s/.*NEWTOY[(]\\([^,]*\\).*TOYFLAG_NOFORK.*/\\1/p", "toys/pending/sh.c"])
blah,pending=readit(["/bin/bash", "-c", "sed -n 's/[^ \\t].*TOY(\\([^,]*\\),.*/\\1/p' toys/pending/*.c"])
version=readit(["./toybox","--version"])[-1][-1]

print "Analyzing..."

# Create reverse mappings: reverse["command"] gives list of categories it's in
reverse={}
for i in stuff:
  for j in stuff[i]:
    try: reverse[j].append(i)
    except: reverse[j]=[i]
print "all commands=%s" % len(reverse)

# Run a couple sanity checks on input

for i in toystuff:
  if (i in pending): print "Pending command not roadmap: %s" % i

unknowns=[]
for i in toystuff + pending:
  if not i in reverse: unknowns.append(i)

if unknowns: print "uncategorized: %s" % " ".join(unknowns)

conv = [("posix", '<a href="http://pubs.opengroup.org/onlinepubs/9699919799/utilities/%s.html">%%s</a>', "[%s]"),
        ("lsb", '<a href="http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/%s.html">%%s</a>', '&lt;%s&gt;'),
        ("development", '<a href="https://man7.org/linux/man-pages/man1/%s.1.html">%%s</a>', '(%s)'),
        ("toolbox", "", '{%s}'), ("klibc_cmd", "", '=%s='),
        ("sash_cmd", "", '#%s#'), ("sbase_cmd", "", '@%s@'),
        ("beastiebox_cmd", "", '*%s*'), ("tizen_cmd", "", '$%s$'),
        ("fhs_cmd", "", '-%s-'), ("yocto_cmd", "", ".%s."),
        ("shell", "", "%%%s%%"),
        ("request", '<a href="https://man7.org/linux/man-pages/man1/%s.1.html">%%s</a>', '+%s+')]

def categorize(reverse, i, skippy=""):
  linky = "%s"
  out = i

  if skippy: types = filter(lambda a: a != skippy, reverse[i])
  else: types = reverse[i]

  for j in conv:
    if j[0] in types:
      if j[1]: linky = j[1] % i
      out = j[2] % out
      if not skippy: break
  if (not skippy) and out == i:
    sys.stderr.write("unknown %s %s\n" % (i,reverse[i]))

  return linky % out

# Sort/annotate done, pending, and todo item lists

allcmd=[]
done=[]
pend=[]
todo=[]
blah=list(reverse)
blah.sort()
for i in blah:
  out=categorize(reverse, i)
  allcmd.append(out)
  if i in toystuff or i in pending:
    if i in toystuff: done.append(out)
    else: pend.append(out)
    out='<strike>%s</strike>' % out
  else: todo.append(out)

print "implemented=%s" % len(toystuff)

# Write data to output file

outfile=open("www/status.html", "w")
outfile.write("""<html><head><title>toybox current status</title>
<!--#include file="header.html" -->
<title>Toybox Status</title>
""");
outfile.write("<h1>Status of toybox %s</h1>\n" % version);
outfile.write("<h3>Legend: %s <strike>pending</strike></h3>\n"%" ".join(map(lambda i: i[2]%(i[0].split("_")[0]), conv)))

outfile.write("<a name=done><h2><a href=#done>Completed</a></h2><blockquote><p>%s</p></blockquote>\n" % "\n".join(done))
outfile.write("<a name=part><h2><a href=#part>Partially implemented (in toys/pending)</a></h2><blockquote><p>%s</p></blockquote>\n" % "\n".join(pend))
outfile.write("<a name=todo><h2><a href=#todo>Not started yet</a></h2><blockquote><p>%s</p></blockquote>\n" % "\n".join(todo))

# Output unfinished commands by category

outfile.write("<hr><h2>Categories of remaining todo items</h2>")

for i in conv:
  todo = []
  i=i[0]

  for j in stuff[i]:
    if j in toystuff: continue
    if j in pending: todo.append('<strike>%s</strike>' % j)
    else: todo.append(categorize(reverse,j,i))

  if todo:
    k = i
    for j in conv:
      if j[0] == i:
        k = j[2] % i.split("_")[0]

    outfile.write("<a name=%s><h2><a href=#%s>%s<a></h2><blockquote><p>" % (i,i,k))
    outfile.write(" ".join(todo))
    outfile.write("</p></blockquote>\n")

outfile.write("<hr><a name=all><h2><a href=#all>All commands together in one big list</a></h2><blockquote><p>%s</p></blockquote>\n" % "\n".join(allcmd))

outfile.write("""
<p>See the <a href=roadmap.html>Roadmap page</a> for more information.</p>

<!-- #include "footer.html" -->""")
