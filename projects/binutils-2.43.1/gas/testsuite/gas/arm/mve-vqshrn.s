.syntax unified
.thumb

@ Test base opcode and different immediates.
vqshrnb.s16 q0, q0, #8
vqshrnb.s16 q0, q0, #1

@ Test different registers.
vqshrnb.s16 q0, q7, #8
vqshrnb.s16 q7, q0, #8

@ Test T = 1
vqshrnt.s16 q0, q0, #8

@ Test U = 1.
vqshrnb.u16 q0, q0, #8

@ Test <dt> = S32 with different immediates.
vqshrnb.s32 q0, q0, #16
vqshrnb.s32 q0, q0, #8
vqshrnb.s32 q0, q0, #1

@ Test <dt> = U32.
vqshrnb.u32 q0, q0, #16

@ Test vqshrun with different immediates.
vqshrunb.s16 q0, q0, #8
vqshrunb.s16 q0, q0, #1

@ Test vqshrun with different registers.
vqshrunb.s16 q0, q7, #8
vqshrunb.s16 q7, q0, #8

@ Test vqshrun with T = 1.
vqshrunt.s16 q0, q0, #8

@ Test vqshrun with <dt> = S32 and different immediates.
vqshrunb.s32 q0, q0, #16
vqshrunb.s32 q0, q0, #8
vqshrunb.s32 q0, q0, #1
