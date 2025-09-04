#readelf: --section-groups
#name: Attaching a section to group defined later
#source: attach-2.s

#...
group section \[    1\] `\.group' \[foo\.group\] contains [24] sections:
   \[Index\]    Name
.* bar
.* foo
#?.* \.xt\.prop
#?.* \.rela\.xt\.prop
