#include <stdio.h>

long pr_movb(long);
long pr_movl0(long);
long pr_movw(long);
long pr_movb_preserve(long);
long pr_movw_preserve(long);
long pr_movb_imm(void);
long pr_movw_imm(void);
long pr_addb(long, long);
long pr_xor_addb(long);
long pr_andb(long, long);
long pr_orb(long, long);
long pr_movzbl(long);
long pr_movzwl(long);
long pr_movzbq(long);
long pr_index(long, long*);
long pr_index_shl(long, long*);
long pr_movw_index(long, long*);
long pr_xor_movb_32use(long);
long pr_xor_movw_32use(long);
long pr_xor_movw_cx_hi16(long);
long pr_loop_redef(long, long*);
long pr_branch(long, long);
long pr_branch2(long, long);
long pr_loop(long, long*);
long pr_slot(long, long);
long pr_slotw(long, long);
long pr_setcc_slot(long, long);
long pr_cwd(long);
long pr_mulw(long, long);
long pr_fstsw(void);

static int fails = 0;

static void expect64(unsigned long got, unsigned long want, const char* what)
{
	if (got != want) {
		printf("%s: got %lx want %lx\n", what, got, want);
		fails = 1;
	}
}

int main()
{
	static long table[256];
	long i;
	for (i = 0; i < 256; i++)
		table[i] = 0x1000 + i;

	/* zero-def + byte/word write: the result is exactly the narrow value */
	expect64(pr_movb(0x1CD), 0xCD, "xorq + movb");
	expect64(pr_movl0(0x1CD), 0xCD, "movl $0 + movb");
	expect64(pr_movw(0x1ABCD), 0xABCD, "xorq + movw");

	/* partial writes preserve a live non-zero upper value */
	expect64(pr_movb_preserve(0x1CD), 0x11223344556677CDUL, "movb preserves bits 8-63");
	expect64(pr_movw_preserve(0x1ABCD), 0x112233445566ABCDUL, "movw preserves bits 16-63");
	expect64(pr_movb_imm(), 0x11223344556677ABUL, "movb imm preserves bits 8-63");
	expect64(pr_movw_imm(), 0x112233445566ABCDUL, "movw imm preserves bits 16-63");

	/* byte ALU ops are read-modify-writes of the whole register */
	expect64(pr_addb(0x11223344556677F0, 0x20),
		0x1122334455667710UL, "addb preserves bits 8-63");
	expect64(pr_xor_addb(0x1CD), 0xCD, "xorq + addb");
	expect64(pr_andb(0x112233445566773C, 0x1A5),
		0x1122334455667724UL, "andb preserves bits 8-63");
	expect64(pr_orb(0x1122334455667704, 0x1A0),
		0x11223344556677A4UL, "orb preserves bits 8-63");

	/* movzx/movsx are full defs */
	expect64(pr_movzbl(0x1CD), 0xCD, "xorq + movzbl (full def)");
	expect64(pr_movzwl(0x1ABCD), 0xABCD, "xorq + movzwl (full def)");
	expect64(pr_movzbq(0x1CD), 0xCD, "movq $-1 + movzbq (full def)");

	/* full-register index reads after partial writes (the ghash shape) */
	expect64(pr_index(0x1C5, table), table[0xC5], "xorq + movb + index");
	expect64(pr_index_shl(0x1C5, table), table[(0xC5 << 2) & 0xff], "xorq + movb + shlb + index");
	expect64(pr_movw_index(0x1ABC5, table), table[0xC5], "xorq + movw + andw + index");

	/* branch interleavings */
	expect64(pr_branch(0, 0x1CD), 0, "branch not-taken");
	expect64(pr_branch(0xAB, 0x1CD), 0xCD, "branch taken");
	expect64(pr_branch2(0, 0x1CD), 0, "branch2 not-taken");
	expect64(pr_branch2(0xAB, 0x1CD), 0xCD, "branch2 taken");

	/* loop with the full-register read in the body */
	expect64(pr_loop(0x1C5, table), 3 * table[0xC5], "loop index");

	/* xor-zeroing + narrow def + wider use (the ghash shape): the xor must
	   stay live across the partial write, so the preserved bits read zero */
	expect64(pr_xor_movb_32use(0x1CD), 0xCD, "xorq + movb + movl");
	expect64(pr_xor_movb_32use(0xFFAA), 0xAA, "xorq + movb + movl (2)");
	expect64(pr_xor_movw_32use(0x1ABCD), 0xABCD, "xorq + movw + movl");
	expect64(pr_xor_movw_cx_hi16(0x1ABCD), 0, "xorq + movw %cx + shr 16");
	expect64(pr_xor_movw_cx_hi16(0xFFABCD), 0, "xorq + movw %cx + shr 16 (2)");
	{
		unsigned long rot = 0x1C5, idx = rot & 0xff, acc = 0;
		int k;
		for (k = 0; k < 8; k++) {
			acc ^= (unsigned long)table[idx];
			rot = (rot << 8) | (rot >> 56);
			idx = ((rot & 0xff) << 4) & 0xff;
		}
		expect64(pr_loop_redef(0x1C5, table), acc, "xorq + per-iter movb + index");
	}

	/* narrow frame-slot stores preserve the slot's other bytes */
	expect64(pr_slot(0x1122334455667788, 0x1CD), 0x11223344556677CDUL, "movb to frame slot");
	expect64(pr_slotw(0x1122334455667788, 0x1ABCD), 0x112233445566ABCDUL, "movw to frame slot");
	expect64(pr_setcc_slot(0x1122334455667788, 0x20), 0x1122334455667701UL, "setcc to frame slot");

	/* pinned 16-bit accumulator writes preserve the upper bits */
	expect64(pr_cwd(0x8000), 0x112233445566FFFFUL, "cwd preserves bits 16-63 (sign)");
	expect64(pr_cwd(0x4000), 0x1122334455660000UL, "cwd preserves bits 16-63 (no sign)");
	/* 0x1234 * 0x5678 = 0x06260060: dx = 0x0626 */
	expect64(pr_mulw(0x1234, 0x5678), 0x1122334455660626UL, "mulw preserves bits 16-63");

	/* fstsw: status word in %ax, upper 48 bits preserved (C3 set: equal) */
	{
		unsigned long r = pr_fstsw();
		expect64(r >> 16, 0x112233445566UL, "fstsw preserves bits 16-63");
		expect64(r & 16384, 16384, "fstsw status word (C3: equal)");
	}

	if (fails)
		return 1;
	printf("partialreg att ok\n");
	return 0;
}
