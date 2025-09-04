	.text
curly:
	vaddps	(%eax) {1to16}, %zmm1, %zmm2
	vaddps	(%eax){ 1to16}, %zmm1, %zmm2
	vaddps	(%eax){1to16 }, %zmm1, %zmm2
	vaddps	(%eax){1to16} , %zmm1, %zmm2

	vaddps	{ rn-sae}, %zmm0, %zmm1, %zmm2
	vaddps	{rn-sae }, %zmm0, %zmm1, %zmm2
	vaddps	{rn-sae} , %zmm0, %zmm1, %zmm2

	vaddps	%zmm0, %zmm1, %zmm2 {%k3}{z}
	vaddps	%zmm0, %zmm1, %zmm2{ %k3}{z}
	vaddps	%zmm0, %zmm1, %zmm2{%k3 }{z}
	vaddps	%zmm0, %zmm1, %zmm2{%k3} {z}
	vaddps	%zmm0, %zmm1, %zmm2{%k3}{ z}
	vaddps	%zmm0, %zmm1, %zmm2{%k3}{z }

	{ evex} vaddps %xmm0, %xmm1, %xmm2
	{evex } vaddps %xmm0, %xmm1, %xmm2
	{evex}vaddps %xmm0, %xmm1, %xmm2

	.intel_syntax noprefix
	vaddps	zmm0, zmm1, zmm2 {rn-sae}
	vaddps	zmm0, zmm1, zmm2{ rn-sae}
	vaddps	zmm0, zmm1, zmm2{rn-sae }
