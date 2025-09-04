# imm_op
slti  $r4,$r5,0
slti  $r4,$r5,0x7ff
slti  $r4,$r5,-0x7ff
sltui  $r4,$r5,0
sltui  $r4,$r5,0x7ff
sltui  $r4,$r5,-0x7ff
addi.w  $r4,$r5,0
addi.w  $r4,$r5,0x7ff
addi.w  $r4,$r5,-0x7ff
andi  $r4,$r5,0
andi  $r4,$r5,0x7ff
ori  $r4,$r5,0
ori  $r4,$r5,0x7ff
xori  $r4,$r5,0
xori  $r4,$r5,0x7ff
lu12i.w  $r4,0
lu12i.w  $r4,0x7ffff
pcaddu12i  $r4,0
pcaddu12i  $r4,0x7ffff
pcaddu12i  $r4,-0x7ffff

# imm_ins
.equ a, 0x123
.equ b, 0xfffff00000
.equ c, 0xfffffffffff
.equ d, 2
.equ e,0x100

alsl.w $r11,$r12,$r13,d
alsl.wu $r11,$r12,$r13,d

break d
dbcl d
syscall d

slli.w $r11,$r12,d
srli.w $r11,$r12,d
srai.w $r12,$r13,d

slti $r12,$r13,a
sltui $r12,$r13,a
addi.w $r12,$r13,a
andi $r12,$r13,d
ori  $r12,$r13,d
xori $r12,$r13,d
lu12i.w $r12,a
pcaddu12i $r12,a

csrrd $r12,a
csrwr $r12,a
csrxchg $r12,$r13,d
cacop d,$r13,d
lddir $r12,$r13,d
ldpte $r12,d

invtlb d,$r13,$r14

# fix_op
rdtimel.w  $r4,$r5
rdtimeh.w  $r4,$r5
alsl.w  $r4,$r5,$r6,1
alsl.w  $r4,$r5,$r6,4
alsl.wu  $r4,$r5,$r6,1
alsl.wu  $r4,$r5,$r6,4
add.w  $r4,$r5,$r6
sub.w  $r4,$r5,$r6
slt  $r4,$r5,$r6
sltu  $r4,$r5,$r6
nor  $r4,$r5,$r6
and  $r4,$r5,$r6
or  $r4,$r5,$r6
xor  $r4,$r5,$r6

# load_store
sll.w  $r4,$r5,$r6
srl.w  $r4,$r5,$r6
sra.w  $r4,$r5,$r6
mul.w  $r4,$r5,$r6
mulh.w  $r4,$r5,$r6
mulh.wu  $r4,$r5,$r6
div.w  $r4,$r5,$r6
mod.w  $r4,$r5,$r6
div.wu  $r4,$r5,$r6
mod.wu  $r4,$r5,$r6
break  0
break  0x7fff
dbcl   0
dbcl   0x7fff
slli.w  $r4,$r5,0
slli.w  $r4,$r5,1
slli.w  $r4,$r5,0x1f
srli.w  $r4,$r5,0
srli.w  $r4,$r5,1
srli.w  $r4,$r5,0x1f
srai.w  $r4,$r5,0
srai.w  $r4,$r5,1
srai.w  $r4,$r5,0x1f
ll.w  $r4,$r5,0
ll.w  $r4,$r5,0x3ffc
sc.w  $r4,$r5,0
sc.w  $r4,$r5,0x3ffc
ld.b  $r4,$r5,0
ld.b  $r4,$r5,0x7ff
ld.b  $r4,$r5,-0x7ff
ld.h  $r4,$r5,0
ld.h  $r4,$r5,0x7ff
ld.h  $r4,$r5,-0x7ff
ld.w  $r4,$r5,0
ld.w  $r4,$r5,0x7ff
ld.w  $r4,$r5,-0x7ff
st.b  $r4,$r5,0
st.b  $r4,$r5,0x7ff
st.b  $r4,$r5,-0x7ff
st.h  $r4,$r5,0
st.h  $r4,$r5,0x7ff
st.h  $r4,$r5,-0x7ff
st.w  $r4,$r5,0
st.w  $r4,$r5,0x7ff
st.w  $r4,$r5,-0x7ff
ld.bu  $r4,$r5,0
ld.bu  $r4,$r5,0x7ff
ld.bu  $r4,$r5,-0x7ff
ld.hu  $r4,$r5,0
ld.hu  $r4,$r5,0x7ff
ld.hu  $r4,$r5,-0x7ff
preld  0,$r5,0
preld  31,$r5,0x7ff
preld  31,$r5,-0x7ff
sc.q  $r4,$r5,$r6,0
sc.q  $r4,$r5,$r6
llacq.w  $r4,$r5,0
llacq.w  $r4,$r5
screl.w  $r4,$r5,0
screl.w  $r4,$r5
dbar  0
dbar  0x7fff
ibar  0
ibar  0x7fff

# jmp_op
.L1:
nop
b  .L1
bl  .L1
beq $r4,$r5,.L1
bne $r4,$r5,.L1
blt $r4,$r5,.L1
bgt $r4,$r5,.L1
bge $r4,$r5,.L1
ble $r4,$r5,.L1
bltu  $r4,$r5,.L1
bgtu  $r4,$r5,.L1
bgeu  $r4,$r5,.L1
bleu  $r4,$r5,.L1
jirl  $zero,$r4,0
