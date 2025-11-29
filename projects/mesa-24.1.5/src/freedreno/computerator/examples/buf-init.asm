@localsize 8, 1, 1
@buf 8 1, 2, 3, 4, 5, 6, 7, 8
@invocationid(r0.x)

ldib.b.untyped.1d.u32.4.imm r1.x, r0.x, 0
(sy)add.u r1.x, r1.x, 5
(rpt5)nop
stib.b.untyped.1d.u32.4.imm r1.x, r0.x, 0
end
