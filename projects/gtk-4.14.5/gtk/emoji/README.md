Emoji data
==========

We use Emoji data from Unicode and the CLDR, stored in a GVariant.
The immediate source for our data is the json files from

  https://github.com/milesj/emojibase.git

To convert the data from that repository to a GVariant that GTK
can use, the convert-emoji tool can be used:

  convert-emoji $emojibase/packages/data/de/data.raw.json de.data

for example (for German).

To make these usable by GTK, we wrap them in a resource bundle
that has the GVariant as

   /org/gtk/libgtk/emoji/de.data

and install the resulting resource bundle at this location:

  /usr/share/gtk-4.0/emoji/de.gresource
