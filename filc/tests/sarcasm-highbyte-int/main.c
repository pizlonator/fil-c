#include <stdio.h>
#include <stdlib.h>

void hb_ops(unsigned char* p);
long hb_xor_ah(long);

static int fails = 0;

static void expect64(unsigned long got, unsigned long want, const char* what)
{
	if (got != want) {
		printf("%s: got %lx want %lx\n", what, got, want);
		fails = 1;
	}
}

static void expect32(unsigned long got, unsigned long want, const char* what)
{
	if ((got & 0xffffffffUL) != want) {
		printf("%s: got %lx want %lx\n", what, got & 0xffffffffUL, want);
		fails = 1;
	}
}

int main()
{
	unsigned char* p = calloc(23, 8);
	if (!p)
		return 1;
	hb_ops(p);
	unsigned long* q = (unsigned long*)p;
	unsigned* u = (unsigned*)p;
	/* movzbl %ah/%ch/%dh/%bh of ...77 88 / ...88 99 / ...99 AA / ...AA BB */
	expect32(u[0], 0x77, "movzbl ah");
	expect32(u[1], 0x88, "movzbl ch");
	expect32(u[2], 0x99, "movzbl dh");
	expect32(u[3], 0xaa, "movzbl bh");
	/* movb %cl(0x99),%ah */
	expect64(q[2], 0x1122334455669988UL, "movb cl,ah");
	/* movb %dh(0x99),%al */
	expect64(q[3], 0x1122334455669999UL, "movb dh,al");
	expect64(q[4], 0x2233445566778899UL, "rcx preserved");
	/* movb %bh(0xAA),%ch */
	expect64(q[5], 0x223344556677AA99UL, "movb bh,ch");
	/* movb $0x5A,%dh */
	expect64(q[6], 0x3344556677885AAAUL, "movb imm,dh");
	/* andb $0x0F,%ah (0x99 & 0x0F = 0x09) */
	expect64(q[7], 0x1122334455660999UL, "andb ah");
	/* orb $0x10,%ch (0xAA | 0x10 = 0xBA) */
	expect64(q[8], 0x223344556677BA99UL, "orb ch");
	/* cmpb $0x09,%ah (equal -> setne = 0) */
	expect64(q[9], 0, "cmpb ah / setne");
	/* testb $0x04,%ch (0xBA & 4 = 0 -> setz = 1) */
	expect64(q[10], 1, "testb ch / setz");
	/* incb %ah (0x07 + 1) */
	expect64(q[11], 0x0102030405060808UL, "incb ah");
	/* decb %dh (0x5A - 1) */
	expect64(q[12], 0x33445566778859AAUL, "decb dh");
	/* negb %bh (-0xAA = 0x56) */
	expect64(q[13], 0x44556677889956BBUL, "negb bh");
	/* xchgb %ah,%ch */
	expect64(q[14], 0x1111111111114433UL, "xchgb rax");
	expect64(q[15], 0x2222222222222255UL, "xchgb rcx");
	/* the pinned movzbl def + pressure registers */
	expect32(u[32], 0x22, "movzbl of post-xchgb ch");
	expect64(q[17], 0x1111111111111111UL, "r12 pressure");
	expect64(q[18], 0x2222222222222222UL, "r13 pressure");
	expect64(q[19], 0x3333333333333333UL, "r14 pressure");
	expect64(q[20], 0x4444444444444444UL, "r15 pressure");
	expect64(q[21], 0x5555555555555555UL, "rbp pressure");
	free(p);
	/* xor-zeroing + high-byte def + full use: the movb merges into bits
	   8-15, preserving the zeroed rest */
	expect64(hb_xor_ah(0x1CD), 0xCD00, "xorq + movb ah");
	expect64(hb_xor_ah(0xFF3A), 0x3A00, "xorq + movb ah (2)");
	if (fails)
		return 1;
	printf("highbyte int ok\n");
	return 0;
}
