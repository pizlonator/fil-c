# Working with man page translations

## Add a new .po file

To enable a new .po file, add its basename (in fact, the language code), to
the first line of po4a.cfg:

    [po4a_langs] de es fr pl uk

It is not crucial to sort the entries alphabetically, but do it anyway for
better readability.

## Update the template and create translated files

This is done in one step using the following simple command:

    po4a po4a.cfg

If there's something wrong with a .po file, the command will fail. In any case,
have a look at the first line of po4a.cfg if all of the mentioned files are
present.

To the man page authors: Please don't forget to add your new man page to po4a.cfg!
