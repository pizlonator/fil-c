#readelf: --section-groups
#name: Attaching a section to a group
#source: attach-1.s

#...
group section \[    1\] `\.group' \[foo\.group\] contains [24] sections:
   \[Index\]    Name
.* foo
.* (\.text|P)
#?.* \.xt\.prop
#?.* \.rela\.xt\.prop
