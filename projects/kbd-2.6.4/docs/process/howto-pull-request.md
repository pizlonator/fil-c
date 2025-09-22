Introduction
============

These instructions are wrote to contributors who tend to send lots of changes.
The basics from [howto-contribute](howto-contribute.md) file are assumed to be
read and understood by the time this file becomes useful.


Setup
=====

1. Find a git server that can be reached from anywhere in internet anonymously.
   Github is for example a popular choice.

2. Create your own kbd contributor repository, and push a upstream clone
   to there.

3. In these instructions the upstream remote repository is called 'origin' and
   the 'yourgit' is the contributor repo.

```
cd ~/projects
git clone git://git.kernel.org/pub/scm/linux/kernel/git/legion/kbd.git
cd kbd
git remote add yourgit git@github.com:yourlogin/kbd.git
git push yourgit
```


Stay up to date
===============

1. Ensure you have the latest from all remote repositories.

2. Merge upstream 'master' branch if needed to your local 'master'.

3. Rebase your working contribution branches.

4. Push the changes to 'yourgit'.

```
git fetch --all
git log --graph --decorate --pretty=oneline --abbrev-commit --all
```

5. If you notice upstream has changed while you were busy with your changes
   rebase on top of the master, but before that:

6. Push a backup of your branch 'textual' to 'yourgit', then

```
git checkout master
git merge origin/master
git checkout textual
git rebase master
```

If rebase reports conflicts fix the conflicts.  In case the rebase conflict is
difficult to fix rebase --abort is good option, or recover from 'yourgit',
either way there is some serious re-work ahead with the change set.

7. Assuming rebase went fine push the latest to 'yourgit'.

```
git push yourgit master:master
git push yourgit --force textual:textual
```

The contributor branch tends to need --force every now and then, don't be afraid
using it.

