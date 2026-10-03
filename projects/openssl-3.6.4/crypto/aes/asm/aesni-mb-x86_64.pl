#! /usr/bin/env perl
# Copyright 2013-2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


# ====================================================================
# Written by Andy Polyakov, @dot-asm, initially for use in the OpenSSL
# project. The module is, however, dual licensed under OpenSSL and
# CRYPTOGAMS licenses depending on where you obtain it. For further
# details see https://github.com/dot-asm/cryptogams/.
# ====================================================================

# Multi-buffer AES-NI procedures process several independent buffers
# in parallel by interleaving independent instructions.
#
# Cycles per byte for interleave factor 4:
#
#			asymptotic	measured
#			---------------------------
# Westmere		5.00/4=1.25	5.13/4=1.28
# Atom			15.0/4=3.75	?15.7/4=3.93
# Sandy Bridge		5.06/4=1.27	5.18/4=1.29
# Ivy Bridge		5.06/4=1.27	5.14/4=1.29
# Haswell		4.44/4=1.11	4.44/4=1.11
# Bulldozer		5.75/4=1.44	5.76/4=1.44
#
# Cycles per byte for interleave factor 8 (not implemented for
# pre-AVX processors, where higher interleave factor incidentally
# doesn't result in improvement):
#
#			asymptotic	measured
#			---------------------------
# Sandy Bridge		5.06/8=0.64	7.10/8=0.89(*)
# Ivy Bridge		5.06/8=0.64	7.14/8=0.89(*)
# Haswell		5.00/8=0.63	5.00/8=0.63
# Bulldozer		5.75/8=0.72	5.77/8=0.72
#
# (*)	Sandy/Ivy Bridge are known to handle high interleave factors
#	suboptimally;

# $output is the last argument if it looks like a file (it has an extension)
# $flavour is the first argument if it doesn't look like a file
$output = $#ARGV >= 0 && $ARGV[$#ARGV] =~ m|\.\w+$| ? pop : undef;
$flavour = $#ARGV >= 0 && $ARGV[0] !~ m|\.| ? shift : undef;

$win64=0; $win64=1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../perlasm/x86_64-xlate.pl" and -f $xlate) or
die "can't locate x86_64-xlate.pl";

push(@INC,"${dir}","${dir}../../perlasm");
require "x86_64-support.pl";

$ptr_size=&pointer_size($flavour);

$avx=0;

if (`$ENV{CC} -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1`
		=~ /GNU assembler version ([2-9]\.[0-9]+)/) {
	$avx = ($1>=2.19) + ($1>=2.22);
}

if (!$avx && $win64 && ($flavour =~ /nasm/ || $ENV{ASM} =~ /nasm/) &&
	   `nasm -v 2>&1` =~ /NASM version ([2-9]\.[0-9]+)/) {
	$avx = ($1>=2.09) + ($1>=2.10);
}

if (!$avx && $win64 && ($flavour =~ /masm/ || $ENV{ASM} =~ /ml64/) &&
	   `ml64 2>&1` =~ /Version ([0-9]+)\./) {
	$avx = ($1>=10) + ($1>=11);
}

if (!$avx && `$ENV{CC} -v 2>&1` =~ /((?:clang|LLVM) version|.*based on LLVM) ([0-9]+\.[0-9]+)/) {
	$avx = ($2>=3.0) + ($2>3.0);
}

if (!$avx && `$ENV{CC} -x c /dev/null -dM -E|grep __clang_major__`
	=~ /#define __clang_major__.([0-9]+)/) {
	if ($1) {
		$avx = ($1>=11); #icx started with clang 11
	}
}

open OUT,"| \"$^X\" \"$xlate\" $flavour \"$output\""
    or die "can't call $xlate: $!";
*STDOUT=*OUT;

# void aesni_multi_cbc_encrypt (
#     struct {	void *inp,*out; int blocks; double iv[2]; } inp[8];
#     const AES_KEY *key,
#     int num);		/* 1 or 2 */
#
$inp="%rdi";	# 1st arg
$key="%rsi";	# 2nd arg
$num="%edx";

$inp_elm_size=2*$ptr_size+8+16;

@inptr=map("%r$_",(8..11));
@outptr=map("%r$_",(12..15));

($rndkey0,$rndkey1)=("%xmm0","%xmm1");
@out=map("%xmm$_",(2..5));
@inp=map("%xmm$_",(6..9));
($counters,$mask,$zero)=map("%xmm$_",(10..12));

($rounds,$one,$sink,$offset)=("%eax","%ecx","%rbp","%rbx");
my $cancel_src=$ENV{SARCASM}?"%rbp":"%rsp";

$code.=<<___;
.text

.extern	OPENSSL_ia32cap_P

.globl	aesni_multi_cbc_encrypt
.type	aesni_multi_cbc_encrypt,\@function,3
.align	32
aesni_multi_cbc_encrypt: #! void(ptr,ptr,int)
.cfi_startproc
___
$code.=<<___ if ($avx);
	cmp	\$2,$num
	jb	.Lenc_non_avx
	mov	OPENSSL_ia32cap_P+4(%rip),%ecx #! global ptr
	test	\$`1<<28`,%ecx			# AVX bit
	jnz	_avx_cbc_enc_shortcut
	jmp	.Lenc_non_avx
.align	16
.Lenc_non_avx:
___
$code.=<<___;
	mov	%rsp,%rax
.cfi_def_cfa_register	%rax
	push	%rbx
.cfi_push	%rbx
	push	%rbp
.cfi_push	%rbp
	push	%r12
.cfi_push	%r12
	push	%r13
.cfi_push	%r13
	push	%r14
.cfi_push	%r14
	push	%r15
.cfi_push	%r15
___
$code.=<<___ if ($win64);
	lea	-0xa8(%rsp),%rsp
	movaps	%xmm6,(%rsp)
	movaps	%xmm7,0x10(%rsp)
	movaps	%xmm8,0x20(%rsp)
	movaps	%xmm9,0x30(%rsp)
	movaps	%xmm10,0x40(%rsp)
	movaps	%xmm11,0x50(%rsp)
	movaps	%xmm12,0x60(%rsp)
	movaps	%xmm13,-0x68(%rax)	# not used, saved to share se_handler
	movaps	%xmm14,-0x58(%rax)
	movaps	%xmm15,-0x48(%rax)
___
$code.=<<___;
	# stack layout
	#
	# +0	output sink
	# +16	input sink [original %rsp and $num]
	# +32	counters

	sub	\$48,%rsp
	and	\$-64,%rsp
	mov	%rax,16(%rsp)			# original %rsp
.cfi_cfa_expression	%rsp+16,deref,+8

.Lenc4x_body:
	movdqu	($key),$zero			# 0-round key
	lea	0x78($key),$key			# size optimization
	lea	$inp_elm_size*2($inp),$inp

.Lenc4x_loop_grande:
	mov	$num,24(%rsp)			# original $num
	xor	$num,$num
___
$code.=<<___ if ($ENV{SARCASM});	# sink dummy must be a proper object, not %rsp
	leaq	aesni_mb_sink(%rip),$sink
___
for($i=0;$i<4;$i++) {
    $inptr_reg=&pointer_register($flavour,@inptr[$i]);
    $outptr_reg=&pointer_register($flavour,@outptr[$i]);
    $code.=<<___;
	# borrow $one for number of blocks
	mov	`$inp_elm_size*$i+2*$ptr_size-$inp_elm_size*2`($inp),$one
	mov	`$inp_elm_size*$i+0-$inp_elm_size*2`($inp),$inptr_reg		#! load ptr
	cmp	$num,$one
	mov	`$inp_elm_size*$i+$ptr_size-$inp_elm_size*2`($inp),$outptr_reg		#! load ptr
	cmovg	$one,$num			# find maximum
	test	$one,$one
	# load IV
	movdqu	`$inp_elm_size*$i+2*$ptr_size+8-$inp_elm_size*2`($inp),@out[$i]
	mov	$one,`32+4*$i`(%rsp)		# initialize counters
	cmovle	$cancel_src,@inptr[$i]			# cancel input
___
}
$code.=<<___;
	test	$num,$num
	jz	.Lenc4x_done

	movups	0x10-0x78($key),$rndkey1
	 pxor	$zero,@out[0]
	movups	0x20-0x78($key),$rndkey0
	 pxor	$zero,@out[1]
	mov	0xf0-0x78($key),$rounds
	 pxor	$zero,@out[2]
	movdqu	(@inptr[0]),@inp[0]		# load inputs
	 pxor	$zero,@out[3]
	movdqu	(@inptr[1]),@inp[1]
	 pxor	@inp[0],@out[0]
	movdqu	(@inptr[2]),@inp[2]
	 pxor	@inp[1],@out[1]
	movdqu	(@inptr[3]),@inp[3]
	 pxor	@inp[2],@out[2]
	 pxor	@inp[3],@out[3]
	movdqa	32(%rsp),$counters		# load counters
	xor	$offset,$offset
	jmp	.Loop_enc4x

.align	32
.Loop_enc4x:
	add	\$16,$offset
___
if ($ENV{SARCASM}) {
  # sink into a proper object, not below the frame
  $code.=<<___;
	mov	\$1,$one			# constant of 1
	leaq	aesni_mb_sink(%rip),$sink
___
} else {
  $code.=<<___;
	lea	16(%rsp),$sink			# sink pointer
	mov	\$1,$one			# constant of 1
	sub	$offset,$sink
___
}
$code.=<<___;

	aesenc		$rndkey1,@out[0]
	prefetcht0	31(@inptr[0],$offset)	# prefetch input
	prefetcht0	31(@inptr[1],$offset)
	aesenc		$rndkey1,@out[1]
	prefetcht0	31(@inptr[2],$offset)
	prefetcht0	31(@inptr[2],$offset)
	aesenc		$rndkey1,@out[2]
	aesenc		$rndkey1,@out[3]
	movups		0x30-0x78($key),$rndkey1
___
for($i=0;$i<4;$i++) {
my $rndkey = ($i&1) ? $rndkey1 : $rndkey0;
$code.=<<___;
	 cmp		`32+4*$i`(%rsp),$one
	aesenc		$rndkey,@out[0]
	aesenc		$rndkey,@out[1]
	aesenc		$rndkey,@out[2]
	 cmovge		$sink,@inptr[$i]	# cancel input
	 cmovg		$sink,@outptr[$i]	# sink output
	aesenc		$rndkey,@out[3]
	movups		`0x40+16*$i-0x78`($key),$rndkey
___
}
$code.=<<___;
	 movdqa		$counters,$mask
	aesenc		$rndkey0,@out[0]
	prefetcht0	15(@outptr[0],$offset)	# prefetch output
	prefetcht0	15(@outptr[1],$offset)
	aesenc		$rndkey0,@out[1]
	prefetcht0	15(@outptr[2],$offset)
	prefetcht0	15(@outptr[3],$offset)
	aesenc		$rndkey0,@out[2]
	aesenc		$rndkey0,@out[3]
	movups		0x80-0x78($key),$rndkey0
	 pxor		$zero,$zero

	aesenc		$rndkey1,@out[0]
	 pcmpgtd	$zero,$mask
	 movdqu		-0x78($key),$zero	# reload 0-round key
	aesenc		$rndkey1,@out[1]
	 paddd		$mask,$counters		# decrement counters
	 movdqa		$counters,32(%rsp)	# update counters
	aesenc		$rndkey1,@out[2]
	aesenc		$rndkey1,@out[3]
	movups		0x90-0x78($key),$rndkey1

	cmp	\$11,$rounds

	aesenc		$rndkey0,@out[0]
	aesenc		$rndkey0,@out[1]
	aesenc		$rndkey0,@out[2]
	aesenc		$rndkey0,@out[3]
	movups		0xa0-0x78($key),$rndkey0

	jb	.Lenc4x_tail

	aesenc		$rndkey1,@out[0]
	aesenc		$rndkey1,@out[1]
	aesenc		$rndkey1,@out[2]
	aesenc		$rndkey1,@out[3]
	movups		0xb0-0x78($key),$rndkey1

	aesenc		$rndkey0,@out[0]
	aesenc		$rndkey0,@out[1]
	aesenc		$rndkey0,@out[2]
	aesenc		$rndkey0,@out[3]
	movups		0xc0-0x78($key),$rndkey0

	je	.Lenc4x_tail

	aesenc		$rndkey1,@out[0]
	aesenc		$rndkey1,@out[1]
	aesenc		$rndkey1,@out[2]
	aesenc		$rndkey1,@out[3]
	movups		0xd0-0x78($key),$rndkey1

	aesenc		$rndkey0,@out[0]
	aesenc		$rndkey0,@out[1]
	aesenc		$rndkey0,@out[2]
	aesenc		$rndkey0,@out[3]
	movups		0xe0-0x78($key),$rndkey0
	jmp	.Lenc4x_tail

.align	32
.Lenc4x_tail:
	aesenc		$rndkey1,@out[0]
	aesenc		$rndkey1,@out[1]
	aesenc		$rndkey1,@out[2]
	aesenc		$rndkey1,@out[3]
	 movdqu		(@inptr[0],$offset),@inp[0]
	movdqu		0x10-0x78($key),$rndkey1

	aesenclast	$rndkey0,@out[0]
	 movdqu		(@inptr[1],$offset),@inp[1]
	 pxor		$zero,@inp[0]
	aesenclast	$rndkey0,@out[1]
	 movdqu		(@inptr[2],$offset),@inp[2]
	 pxor		$zero,@inp[1]
	aesenclast	$rndkey0,@out[2]
	 movdqu		(@inptr[3],$offset),@inp[3]
	 pxor		$zero,@inp[2]
	aesenclast	$rndkey0,@out[3]
	movdqu		0x20-0x78($key),$rndkey0
	 pxor		$zero,@inp[3]

	movups		@out[0],-16(@outptr[0],$offset)
	 pxor		@inp[0],@out[0]
	movups		@out[1],-16(@outptr[1],$offset)
	 pxor		@inp[1],@out[1]
	movups		@out[2],-16(@outptr[2],$offset)
	 pxor		@inp[2],@out[2]
	movups		@out[3],-16(@outptr[3],$offset)
	 pxor		@inp[3],@out[3]

	dec	$num
	jnz	.Loop_enc4x

	mov	16(%rsp),%rax			# original %rsp
.cfi_def_cfa	%rax,8
	mov	24(%rsp),$num

	#pxor	@inp[0],@out[0]
	#pxor	@inp[1],@out[1]
	# output iv FIX ME!
	#movdqu	@out[0],`$inp_elm_size*0+2*$ptr_size+8-$inp_elm_size*2`($inp)
	#pxor	@inp[2],@out[2]
	#movdqu	@out[1],`$inp_elm_size*1+2*$ptr_size+8-$inp_elm_size*2`($inp)
	#pxor	@inp[3],@out[3]
	#movdqu	@out[2],`$inp_elm_size*2+2*$ptr_size+8-$inp_elm_size*2`($inp)	# won't fix, let caller
	#movdqu	@out[3],`$inp_elm_size*3+2*$ptr_size+8-$inp_elm_size*2`($inp)	# figure this out...

	lea	`$inp_elm_size*4`($inp),$inp
	dec	$num
	jnz	.Lenc4x_loop_grande

.Lenc4x_done:
___
$code.=<<___ if ($win64);
	movaps	-0xd8(%rax),%xmm6
	movaps	-0xc8(%rax),%xmm7
	movaps	-0xb8(%rax),%xmm8
	movaps	-0xa8(%rax),%xmm9
	movaps	-0x98(%rax),%xmm10
	movaps	-0x88(%rax),%xmm11
	movaps	-0x78(%rax),%xmm12
	#movaps	-0x68(%rax),%xmm13
	#movaps	-0x58(%rax),%xmm14
	#movaps	-0x48(%rax),%xmm15
___
if ($ENV{SARCASM}) {
  # SARCASM-linux reloads the saved stack pointer first: the body reused
  # %rax (it holds $rounds, not the saved rsp), so the carrier must be
  # re-materialized from its slot before the restores below can read through
  # it. (Gas skips the reload: %rax still holds the saved rsp on its paths.)
  $code.=<<___;
	mov	16(%rsp),%rax			# original %rsp
___
}
$code.=<<___;
	mov	-48(%rax),%r15
.cfi_restore	%r15
	mov	-40(%rax),%r14
.cfi_restore	%r14
	mov	-32(%rax),%r13
.cfi_restore	%r13
	mov	-24(%rax),%r12
.cfi_restore	%r12
	mov	-16(%rax),%rbp
.cfi_restore	%rbp
	mov	-8(%rax),%rbx
.cfi_restore	%rbx
	lea	(%rax),%rsp
.cfi_def_cfa_register	%rsp
.Lenc4x_epilogue:
	ret
.cfi_endproc
.size	aesni_multi_cbc_encrypt,.-aesni_multi_cbc_encrypt

.globl	aesni_multi_cbc_decrypt
.type	aesni_multi_cbc_decrypt,\@function,3
.align	32
aesni_multi_cbc_decrypt: #! void(ptr,ptr,int)
.cfi_startproc
___
$code.=<<___ if ($avx);
	cmp	\$2,$num
	jb	.Ldec_non_avx
	mov	OPENSSL_ia32cap_P+4(%rip),%ecx #! global ptr
	test	\$`1<<28`,%ecx			# AVX bit
	jnz	_avx_cbc_dec_shortcut
	jmp	.Ldec_non_avx
.align	16
.Ldec_non_avx:
___
$code.=<<___;
	mov	%rsp,%rax
.cfi_def_cfa_register	%rax
	push	%rbx
.cfi_push	%rbx
	push	%rbp
.cfi_push	%rbp
	push	%r12
.cfi_push	%r12
	push	%r13
.cfi_push	%r13
	push	%r14
.cfi_push	%r14
	push	%r15
.cfi_push	%r15
___
$code.=<<___ if ($win64);
	lea	-0xa8(%rsp),%rsp
	movaps	%xmm6,(%rsp)
	movaps	%xmm7,0x10(%rsp)
	movaps	%xmm8,0x20(%rsp)
	movaps	%xmm9,0x30(%rsp)
	movaps	%xmm10,0x40(%rsp)
	movaps	%xmm11,0x50(%rsp)
	movaps	%xmm12,0x60(%rsp)
	movaps	%xmm13,-0x68(%rax)	# not used, saved to share se_handler
	movaps	%xmm14,-0x58(%rax)
	movaps	%xmm15,-0x48(%rax)
___
$code.=<<___;
	# stack layout
	#
	# +0	output sink
	# +16	input sink [original %rsp and $num]
	# +32	counters

	sub	\$48,%rsp
	and	\$-64,%rsp
	mov	%rax,16(%rsp)			# original %rsp
.cfi_cfa_expression	%rsp+16,deref,+8

.Ldec4x_body:
	movdqu	($key),$zero			# 0-round key
	lea	0x78($key),$key			# size optimization
	lea	$inp_elm_size*2($inp),$inp

.Ldec4x_loop_grande:
	mov	$num,24(%rsp)			# original $num
	xor	$num,$num
___
$code.=<<___ if ($ENV{SARCASM});	# sink dummy must be a proper object, not %rsp
	leaq	aesni_mb_sink(%rip),$sink
___
for($i=0;$i<4;$i++) {
    $inptr_reg=&pointer_register($flavour,@inptr[$i]);
    $outptr_reg=&pointer_register($flavour,@outptr[$i]);
    $code.=<<___;
	# borrow $one for number of blocks
	mov	`$inp_elm_size*$i+2*$ptr_size-$inp_elm_size*2`($inp),$one
	mov	`$inp_elm_size*$i+0-$inp_elm_size*2`($inp),$inptr_reg		#! load ptr
	cmp	$num,$one
	mov	`$inp_elm_size*$i+$ptr_size-$inp_elm_size*2`($inp),$outptr_reg		#! load ptr
	cmovg	$one,$num			# find maximum
	test	$one,$one
	# load IV
	movdqu	`$inp_elm_size*$i+2*$ptr_size+8-$inp_elm_size*2`($inp),@inp[$i]
	mov	$one,`32+4*$i`(%rsp)		# initialize counters
	cmovle	$cancel_src,@inptr[$i]			# cancel input
___
}
$code.=<<___;
	test	$num,$num
	jz	.Ldec4x_done

	movups	0x10-0x78($key),$rndkey1
	movups	0x20-0x78($key),$rndkey0
	mov	0xf0-0x78($key),$rounds
	movdqu	(@inptr[0]),@out[0]		# load inputs
	movdqu	(@inptr[1]),@out[1]
	 pxor	$zero,@out[0]
	movdqu	(@inptr[2]),@out[2]
	 pxor	$zero,@out[1]
	movdqu	(@inptr[3]),@out[3]
	 pxor	$zero,@out[2]
	 pxor	$zero,@out[3]
	movdqa	32(%rsp),$counters		# load counters
	xor	$offset,$offset
	jmp	.Loop_dec4x

.align	32
.Loop_dec4x:
	add	\$16,$offset
___
if ($ENV{SARCASM}) {
  # sink into a proper object, not below the frame
  $code.=<<___;
	mov	\$1,$one			# constant of 1
	leaq	aesni_mb_sink(%rip),$sink
___
} else {
  $code.=<<___;
	lea	16(%rsp),$sink			# sink pointer
	mov	\$1,$one			# constant of 1
	sub	$offset,$sink
___
}
$code.=<<___;

	aesdec		$rndkey1,@out[0]
	prefetcht0	31(@inptr[0],$offset)	# prefetch input
	prefetcht0	31(@inptr[1],$offset)
	aesdec		$rndkey1,@out[1]
	prefetcht0	31(@inptr[2],$offset)
	prefetcht0	31(@inptr[3],$offset)
	aesdec		$rndkey1,@out[2]
	aesdec		$rndkey1,@out[3]
	movups		0x30-0x78($key),$rndkey1
___
for($i=0;$i<4;$i++) {
my $rndkey = ($i&1) ? $rndkey1 : $rndkey0;
$code.=<<___;
	 cmp		`32+4*$i`(%rsp),$one
	aesdec		$rndkey,@out[0]
	aesdec		$rndkey,@out[1]
	aesdec		$rndkey,@out[2]
	 cmovge		$sink,@inptr[$i]	# cancel input
	 cmovg		$sink,@outptr[$i]	# sink output
	aesdec		$rndkey,@out[3]
	movups		`0x40+16*$i-0x78`($key),$rndkey
___
}
$code.=<<___;
	 movdqa		$counters,$mask
	aesdec		$rndkey0,@out[0]
	prefetcht0	15(@outptr[0],$offset)	# prefetch output
	prefetcht0	15(@outptr[1],$offset)
	aesdec		$rndkey0,@out[1]
	prefetcht0	15(@outptr[2],$offset)
	prefetcht0	15(@outptr[3],$offset)
	aesdec		$rndkey0,@out[2]
	aesdec		$rndkey0,@out[3]
	movups		0x80-0x78($key),$rndkey0
	 pxor		$zero,$zero

	aesdec		$rndkey1,@out[0]
	 pcmpgtd	$zero,$mask
	 movdqu		-0x78($key),$zero	# reload 0-round key
	aesdec		$rndkey1,@out[1]
	 paddd		$mask,$counters		# decrement counters
	 movdqa		$counters,32(%rsp)	# update counters
	aesdec		$rndkey1,@out[2]
	aesdec		$rndkey1,@out[3]
	movups		0x90-0x78($key),$rndkey1

	cmp	\$11,$rounds

	aesdec		$rndkey0,@out[0]
	aesdec		$rndkey0,@out[1]
	aesdec		$rndkey0,@out[2]
	aesdec		$rndkey0,@out[3]
	movups		0xa0-0x78($key),$rndkey0

	jb	.Ldec4x_tail

	aesdec		$rndkey1,@out[0]
	aesdec		$rndkey1,@out[1]
	aesdec		$rndkey1,@out[2]
	aesdec		$rndkey1,@out[3]
	movups		0xb0-0x78($key),$rndkey1

	aesdec		$rndkey0,@out[0]
	aesdec		$rndkey0,@out[1]
	aesdec		$rndkey0,@out[2]
	aesdec		$rndkey0,@out[3]
	movups		0xc0-0x78($key),$rndkey0

	je	.Ldec4x_tail

	aesdec		$rndkey1,@out[0]
	aesdec		$rndkey1,@out[1]
	aesdec		$rndkey1,@out[2]
	aesdec		$rndkey1,@out[3]
	movups		0xd0-0x78($key),$rndkey1

	aesdec		$rndkey0,@out[0]
	aesdec		$rndkey0,@out[1]
	aesdec		$rndkey0,@out[2]
	aesdec		$rndkey0,@out[3]
	movups		0xe0-0x78($key),$rndkey0
	jmp	.Ldec4x_tail

.align	32
.Ldec4x_tail:
	aesdec		$rndkey1,@out[0]
	aesdec		$rndkey1,@out[1]
	aesdec		$rndkey1,@out[2]
	 pxor		$rndkey0,@inp[0]
	 pxor		$rndkey0,@inp[1]
	aesdec		$rndkey1,@out[3]
	movdqu		0x10-0x78($key),$rndkey1
	 pxor		$rndkey0,@inp[2]
	 pxor		$rndkey0,@inp[3]
	movdqu		0x20-0x78($key),$rndkey0

	aesdeclast	@inp[0],@out[0]
	aesdeclast	@inp[1],@out[1]
	 movdqu		-16(@inptr[0],$offset),@inp[0]	# load next IV
	 movdqu		-16(@inptr[1],$offset),@inp[1]
	aesdeclast	@inp[2],@out[2]
	aesdeclast	@inp[3],@out[3]
	 movdqu		-16(@inptr[2],$offset),@inp[2]
	 movdqu		-16(@inptr[3],$offset),@inp[3]

	movups		@out[0],-16(@outptr[0],$offset)
	 movdqu		(@inptr[0],$offset),@out[0]
	movups		@out[1],-16(@outptr[1],$offset)
	 movdqu		(@inptr[1],$offset),@out[1]
	 pxor		$zero,@out[0]
	movups		@out[2],-16(@outptr[2],$offset)
	 movdqu		(@inptr[2],$offset),@out[2]
	 pxor		$zero,@out[1]
	movups		@out[3],-16(@outptr[3],$offset)
	 movdqu		(@inptr[3],$offset),@out[3]
	 pxor		$zero,@out[2]
	 pxor		$zero,@out[3]

	dec	$num
	jnz	.Loop_dec4x

	mov	16(%rsp),%rax			# original %rsp
.cfi_def_cfa	%rax,8
	mov	24(%rsp),$num

	lea	`$inp_elm_size*4`($inp),$inp
	dec	$num
	jnz	.Ldec4x_loop_grande

.Ldec4x_done:
___
$code.=<<___ if ($win64);
	movaps	-0xd8(%rax),%xmm6
	movaps	-0xc8(%rax),%xmm7
	movaps	-0xb8(%rax),%xmm8
	movaps	-0xa8(%rax),%xmm9
	movaps	-0x98(%rax),%xmm10
	movaps	-0x88(%rax),%xmm11
	movaps	-0x78(%rax),%xmm12
	#movaps	-0x68(%rax),%xmm13
	#movaps	-0x58(%rax),%xmm14
	#movaps	-0x48(%rax),%xmm15
___
if ($ENV{SARCASM}) {
  # SARCASM-linux reloads the saved stack pointer first: the body reused
  # %rax (it holds $rounds, not the saved rsp), so the carrier must be
  # re-materialized from its slot before the restores below can read through
  # it. (Gas skips the reload: %rax still holds the saved rsp on its paths.)
  $code.=<<___;
	mov	16(%rsp),%rax			# original %rsp
___
}
$code.=<<___;
	mov	-48(%rax),%r15
.cfi_restore	%r15
	mov	-40(%rax),%r14
.cfi_restore	%r14
	mov	-32(%rax),%r13
.cfi_restore	%r13
	mov	-24(%rax),%r12
.cfi_restore	%r12
	mov	-16(%rax),%rbp
.cfi_restore	%rbp
	mov	-8(%rax),%rbx
.cfi_restore	%rbx
	lea	(%rax),%rsp
.cfi_def_cfa_register	%rsp
.Ldec4x_epilogue:
	ret
.cfi_endproc
.size	aesni_multi_cbc_decrypt,.-aesni_multi_cbc_decrypt
___

						if ($avx) {{{
my @ptr=map("%r$_",(8..15));
my $offload=$sink;

my @out=map("%xmm$_",(2..9));
my @inp=map("%xmm$_",(10..13));
my ($counters,$zero)=("%xmm14","%xmm15");
# The 64+8*$i slots hold the OUTPUT pointers as capabilities (`store ptr`/
# `load ptr`) and the output stores go through them directly.  The distance
# scheme the plain mode uses (a sub/and frame keeping the input->output
# distance in $offset, the output pointer rebuilt from the input
# capability) capability-checks every output store against the input
# object, so it is not available under SARCASM.  The frame slots survive
# sarcasm's virtualized round-trips, so a capability stored into one comes
# back intact.  Every teardown reloads the saved-rsp carrier
# (`mov 16(%rsp),%rax`) because sarcasm drops the entry `mov %rsp,%rax`
# save and the body clobbers %rax; `mov -K(%rax),%reg` restores the pushed
# saves and `lea (%rax),%rsp` revives the entry rsp.  All 16B vector slots
# stay 16-aligned on both paths (sarcasm materializes the FP-touched slot
# ranges into 16-aligned areas of its synthesized frame; enc8x %rsp is
# 128-aligned, dec8x %rsp%256==64).
$code.=<<___;
.type	aesni_multi_cbc_encrypt_avx,\@function,3
.align	32
aesni_multi_cbc_encrypt_avx: #! void(ptr,ptr,int)
.cfi_startproc
_avx_cbc_enc_shortcut:
	mov	%rsp,%rax
.cfi_def_cfa_register	%rax
	push	%rbx
.cfi_push	%rbx
	push	%rbp
.cfi_push	%rbp
	push	%r12
.cfi_push	%r12
	push	%r13
.cfi_push	%r13
	push	%r14
.cfi_push	%r14
	push	%r15
.cfi_push	%r15
___
$code.=<<___ if ($win64);
	lea	-0xa8(%rsp),%rsp
	movaps	%xmm6,(%rsp)
	movaps	%xmm7,0x10(%rsp)
	movaps	%xmm8,0x20(%rsp)
	movaps	%xmm9,0x30(%rsp)
	movaps	%xmm10,0x40(%rsp)
	movaps	%xmm11,0x50(%rsp)
	movaps	%xmm12,-0x78(%rax)
	movaps	%xmm13,-0x68(%rax)
	movaps	%xmm14,-0x58(%rax)
	movaps	%xmm15,-0x48(%rax)
___
$code.=<<___;
	# stack layout
	#
	# +0	output sink
	# +16	input sink [original %rsp and $num]
	# +32	counters
	# +64	distances between inputs and outputs
	# +128	off-load area for @inp[0..3]

	sub	\$192,%rsp
	and	\$-128,%rsp
	mov	%rax,16(%rsp)			# original %rsp
.cfi_cfa_expression	%rsp+16,deref,+8

.Lenc8x_body:
	vzeroupper
	vmovdqu	($key),$zero			# 0-round key
	lea	0x78($key),$key			# size optimization
	lea	`$inp_elm_size*4`($inp),$inp
	shr	\$1,$num

.Lenc8x_loop_grande:
	#mov	$num,24(%rsp)			# original $num
	xor	$num,$num
___
for($i=0;$i<8;$i++) {
  my $temp = $i ? $offload : $offset;
    $ptr_reg=&pointer_register($flavour,@ptr[$i]);
    $temp_reg=&pointer_register($flavour,$temp);
    $code.=<<___;
	# borrow $one for number of blocks
	mov	`$inp_elm_size*$i+2*$ptr_size-$inp_elm_size*4`($inp),$one
	# input pointer
	mov	`$inp_elm_size*$i+0-$inp_elm_size*4`($inp),$ptr_reg		#! load ptr
	cmp	$num,$one
	# output pointer
	mov	`$inp_elm_size*$i+$ptr_size-$inp_elm_size*4`($inp),$temp_reg		#! load ptr
	cmovg	$one,$num			# find maximum
	test	$one,$one
	# load IV
	vmovdqu	`$inp_elm_size*$i+2*$ptr_size+8-$inp_elm_size*4`($inp),@out[$i]
	mov	$one,`32+4*$i`(%rsp)		# initialize counters
___
  # The canceled input pointer becomes the sink object.  The sink pointer is
  # materialized into a register that is dead here -- $sink while $i==0
  # (%rbp is not claimed until the offload setup below) and $offset once
  # $i>0 (the $i==0 store below already consumed the output pointer %rbx
  # held) -- because $temp occupies the other register at each $i.  The
  # cmov then selects between the input capability and the sink, and the
  # output position is kept as a proper capability in the slot.
  if ($ENV{SARCASM}) {
    my $tmp = $i ? $offset : $sink;
    $code.=<<___;
	leaq	aesni_mb_sink(%rip),$tmp
	cmovle	$tmp,@ptr[$i]			# cancel input
	mov	$temp,`64+8*$i`(%rsp)		#! store ptr
___
  } else {
    $code.=<<___;
	cmovle	%rsp,@ptr[$i]			# cancel input
	sub	@ptr[$i],$temp			# distance between input and output
	mov	$temp,`64+8*$i`(%rsp)		# initialize distances
___
  }
}
$code.=<<___;
	test	$num,$num
	jz	.Lenc8x_done

	vmovups	0x10-0x78($key),$rndkey1
	vmovups	0x20-0x78($key),$rndkey0
	mov	0xf0-0x78($key),$rounds

	vpxor	(@ptr[0]),$zero,@inp[0]		# load inputs and xor with 0-round
___
	# The offload area is a declared stack buffer (%rsp+128..%rsp+192): the
	# accesses below carry `#! stack buffer` annotations, so sarcasm lowers
	# them into its synthesized frame with runtime bounds checks and the
	# base lea stays a plain buffer-address value.
$code.=<<___;
	 lea	128(%rsp),$offload		# offload area
___
$code.=<<___;
	vpxor	(@ptr[1]),$zero,@inp[1]
	vpxor	(@ptr[2]),$zero,@inp[2]
	vpxor	(@ptr[3]),$zero,@inp[3]
	 vpxor	@inp[0],@out[0],@out[0]
	vpxor	(@ptr[4]),$zero,@inp[0]
	 vpxor	@inp[1],@out[1],@out[1]
	vpxor	(@ptr[5]),$zero,@inp[1]
	 vpxor	@inp[2],@out[2],@out[2]
	vpxor	(@ptr[6]),$zero,@inp[2]
	 vpxor	@inp[3],@out[3],@out[3]
	vpxor	(@ptr[7]),$zero,@inp[3]
	 vpxor	@inp[0],@out[4],@out[4]
	mov	\$1,$one			# constant of 1
	 vpxor	@inp[1],@out[5],@out[5]
	 vpxor	@inp[2],@out[6],@out[6]
	 vpxor	@inp[3],@out[7],@out[7]
	jmp	.Loop_enc8x

.align	32
.Loop_enc8x:
___
for($i=0;$i<8;$i++) {
my $rndkey=($i&1)?$rndkey0:$rndkey1;
  if ($ENV{SARCASM}) {
    # Capability-safe form: the output position lives in the 64+8*$i
    # slot as a real capability (stored in the prologue, advanced by the
    # tail); @ptr[$i] stays on the input side.  The original form keeps
    # a distance in $offset and switches @ptr[$i] to the output side with
    # lea, which would make the output store use the *input* capability.
    $code.=<<___;
	vaesenc		$rndkey,@out[0],@out[0]
	 mov		64+8*$i(%rsp),$offset		#! load ptr
	 cmp		32+4*$i(%rsp),$one
	vaesenc		$rndkey,@out[1],@out[1]
	prefetcht0	31(@ptr[$i])			# prefetch input
	vaesenc		$rndkey,@out[2],@out[2]
___
    $code.=<<___ if ($i>1);
	prefetcht0	15(@ptr[$i-2])			# prefetch output
___
    $code.=<<___;
	vaesenc		$rndkey,@out[3],@out[3]
	jl	.LcinE$i
	leaq	aesni_mb_sink(%rip),@ptr[$i]
.LcinE$i:
	vaesenc		$rndkey,@out[4],@out[4]
	jle	.LcoutE$i
	leaq	aesni_mb_sink(%rip),$offset
.LcoutE$i:
	vaesenc		$rndkey,@out[5],@out[5]
	 vpxor		16(@ptr[$i]),$zero,@inp[$i%4]	# load input and xor with 0-round
	 mov		$offset,64+8*$i(%rsp)		#! store ptr
	vaesenc		$rndkey,@out[6],@out[6]
	vaesenc		$rndkey,@out[7],@out[7]
	vmovups		`16*(3+$i)-0x78`($key),$rndkey
	 lea		16(@ptr[$i]),@ptr[$i]	# advance input
___
    # All enc8x offload accesses sit in a B2-shared body, so every annotation
    # is the long form (a clone's buffer window is private to its own frame
    # context and cannot share a short form's file-wide range).
    my $offe = " #! stack buffer (offe, %rsp + 128, %rsp + 192)";
    $code.=<<___ if ($i<4)
	 vmovdqu	@inp[$i%4],`16*$i`($offload)$offe	# off-load
___
  } else {
$code.=<<___;
	vaesenc		$rndkey,@out[0],@out[0]
	 cmp		32+4*$i(%rsp),$one
___
$code.=<<___ if ($i);
	 mov		64+8*$i(%rsp),$offset
___
$code.=<<___;
	vaesenc		$rndkey,@out[1],@out[1]
	prefetcht0	31(@ptr[$i])			# prefetch input
	vaesenc		$rndkey,@out[2],@out[2]
___
$code.=<<___ if ($i>1);
	prefetcht0	15(@ptr[$i-2])			# prefetch output
___
$code.=<<___;
	vaesenc		$rndkey,@out[3],@out[3]
	 lea		(@ptr[$i],$offset),$offset
	 cmovge		%rsp,@ptr[$i]			# cancel input
	vaesenc		$rndkey,@out[4],@out[4]
	 cmovg		%rsp,$offset			# sink output
	vaesenc		$rndkey,@out[5],@out[5]
	 sub		@ptr[$i],$offset
	vaesenc		$rndkey,@out[6],@out[6]
	 vpxor		16(@ptr[$i]),$zero,@inp[$i%4]	# load input and xor with 0-round
	 mov		$offset,64+8*$i(%rsp)
	vaesenc		$rndkey,@out[7],@out[7]
	vmovups		`16*(3+$i)-0x78`($key),$rndkey
	 lea		16(@ptr[$i],$offset),@ptr[$i]	# switch to output
___
$code.=<<___ if ($i<4)
	 vmovdqu	@inp[$i%4],`16*$i`($offload)	# off-load
___
  }
}
$code.=<<___;
	 vmovdqu	32(%rsp),$counters
	prefetcht0	15(@ptr[$i-2])			# prefetch output
	prefetcht0	15(@ptr[$i-1])
	cmp	\$11,$rounds
	jb	.Lenc8x_tail

	vaesenc		$rndkey1,@out[0],@out[0]
	vaesenc		$rndkey1,@out[1],@out[1]
	vaesenc		$rndkey1,@out[2],@out[2]
	vaesenc		$rndkey1,@out[3],@out[3]
	vaesenc		$rndkey1,@out[4],@out[4]
	vaesenc		$rndkey1,@out[5],@out[5]
	vaesenc		$rndkey1,@out[6],@out[6]
	vaesenc		$rndkey1,@out[7],@out[7]
	vmovups		0xb0-0x78($key),$rndkey1

	vaesenc		$rndkey0,@out[0],@out[0]
	vaesenc		$rndkey0,@out[1],@out[1]
	vaesenc		$rndkey0,@out[2],@out[2]
	vaesenc		$rndkey0,@out[3],@out[3]
	vaesenc		$rndkey0,@out[4],@out[4]
	vaesenc		$rndkey0,@out[5],@out[5]
	vaesenc		$rndkey0,@out[6],@out[6]
	vaesenc		$rndkey0,@out[7],@out[7]
	vmovups		0xc0-0x78($key),$rndkey0
	je	.Lenc8x_tail

	vaesenc		$rndkey1,@out[0],@out[0]
	vaesenc		$rndkey1,@out[1],@out[1]
	vaesenc		$rndkey1,@out[2],@out[2]
	vaesenc		$rndkey1,@out[3],@out[3]
	vaesenc		$rndkey1,@out[4],@out[4]
	vaesenc		$rndkey1,@out[5],@out[5]
	vaesenc		$rndkey1,@out[6],@out[6]
	vaesenc		$rndkey1,@out[7],@out[7]
	vmovups		0xd0-0x78($key),$rndkey1

	vaesenc		$rndkey0,@out[0],@out[0]
	vaesenc		$rndkey0,@out[1],@out[1]
	vaesenc		$rndkey0,@out[2],@out[2]
	vaesenc		$rndkey0,@out[3],@out[3]
	vaesenc		$rndkey0,@out[4],@out[4]
	vaesenc		$rndkey0,@out[5],@out[5]
	vaesenc		$rndkey0,@out[6],@out[6]
	vaesenc		$rndkey0,@out[7],@out[7]
	vmovups		0xe0-0x78($key),$rndkey0

.Lenc8x_tail:
	vaesenc		$rndkey1,@out[0],@out[0]
	 vpxor		$zero,$zero,$zero
	vaesenc		$rndkey1,@out[1],@out[1]
	vaesenc		$rndkey1,@out[2],@out[2]
	 vpcmpgtd	$zero,$counters,$zero
	vaesenc		$rndkey1,@out[3],@out[3]
	vaesenc		$rndkey1,@out[4],@out[4]
	 vpaddd		$counters,$zero,$zero		# decrement counters
	 vmovdqu	48(%rsp),$counters
	vaesenc		$rndkey1,@out[5],@out[5]
	 mov		64(%rsp),$offset		#! load ptr		# pre-load 1st offset
	vaesenc		$rndkey1,@out[6],@out[6]
	vaesenc		$rndkey1,@out[7],@out[7]
	vmovups		0x10-0x78($key),$rndkey1

	vaesenclast	$rndkey0,@out[0],@out[0]
	 vmovdqa	$zero,32(%rsp)			# update counters
	 vpxor		$zero,$zero,$zero
	vaesenclast	$rndkey0,@out[1],@out[1]
	vaesenclast	$rndkey0,@out[2],@out[2]
	 vpcmpgtd	$zero,$counters,$zero
	vaesenclast	$rndkey0,@out[3],@out[3]
	vaesenclast	$rndkey0,@out[4],@out[4]
	 vpaddd		$zero,$counters,$counters	# decrement counters
	 vmovdqu	-0x78($key),$zero		# 0-round
	vaesenclast	$rndkey0,@out[5],@out[5]
	vaesenclast	$rndkey0,@out[6],@out[6]
	 vmovdqa	$counters,48(%rsp)		# update counters
	vaesenclast	$rndkey0,@out[7],@out[7]
	vmovups		0x20-0x78($key),$rndkey0

___
if ($ENV{SARCASM}) {
  # write outputs through the output-position capabilities
  $code.=<<___;
	vmovups		@out[0],($offset)		# write output
	 lea		16($offset),$offset
	 mov		$offset,64(%rsp)		#! store ptr
	 vpxor		0x00($offload),@out[0],@out[0]	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	mov		64+8(%rsp),$offset		#! load ptr
	vmovups		@out[1],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+8(%rsp)		#! store ptr
	 vpxor		0x10($offload),@out[1],@out[1]	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	mov		64+16(%rsp),$offset		#! load ptr
	vmovups		@out[2],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+16(%rsp)		#! store ptr
	 vpxor		0x20($offload),@out[2],@out[2]	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	mov		64+24(%rsp),$offset		#! load ptr
	vmovups		@out[3],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+24(%rsp)		#! store ptr
	 vpxor		0x30($offload),@out[3],@out[3]	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	mov		64+32(%rsp),$offset		#! load ptr
	vmovups		@out[4],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+32(%rsp)		#! store ptr
	 vpxor		@inp[0],@out[4],@out[4]
	mov		64+40(%rsp),$offset		#! load ptr
	vmovups		@out[5],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+40(%rsp)		#! store ptr
	 vpxor		@inp[1],@out[5],@out[5]
	mov		64+48(%rsp),$offset		#! load ptr
	vmovups		@out[6],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+48(%rsp)		#! store ptr
	 vpxor		@inp[2],@out[6],@out[6]
	mov		64+56(%rsp),$offset		#! load ptr
	vmovups		@out[7],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+56(%rsp)		#! store ptr
	 vpxor		@inp[3],@out[7],@out[7]

	dec	$num
	jnz	.Loop_enc8x
___
} else {
  $code.=<<___;
	vmovups		@out[0],-16(@ptr[0])		# write output
	 sub		$offset,@ptr[0]			# switch to input
	 vpxor		0x00($offload),@out[0],@out[0]
	vmovups		@out[1],-16(@ptr[1])
	 sub		`64+1*8`(%rsp),@ptr[1]
	 vpxor		0x10($offload),@out[1],@out[1]
	vmovups		@out[2],-16(@ptr[2])
	 sub		`64+2*8`(%rsp),@ptr[2]
	 vpxor		0x20($offload),@out[2],@out[2]
	vmovups		@out[3],-16(@ptr[3])
	 sub		`64+3*8`(%rsp),@ptr[3]
	 vpxor		0x30($offload),@out[3],@out[3]
	vmovups		@out[4],-16(@ptr[4])
	 sub		`64+4*8`(%rsp),@ptr[4]
	 vpxor		@inp[0],@out[4],@out[4]
	vmovups		@out[5],-16(@ptr[5])
	 sub		`64+5*8`(%rsp),@ptr[5]
	 vpxor		@inp[1],@out[5],@out[5]
	vmovups		@out[6],-16(@ptr[6])
	 sub		`64+6*8`(%rsp),@ptr[6]
	 vpxor		@inp[2],@out[6],@out[6]
	vmovups		@out[7],-16(@ptr[7])
	 sub		`64+7*8`(%rsp),@ptr[7]
	 vpxor		@inp[3],@out[7],@out[7]

	dec	$num
	jnz	.Loop_enc8x

	mov	16(%rsp),%rax			# original %rsp
___
}
$code.=<<___;
.cfi_def_cfa	%rax,8
	#mov	24(%rsp),$num
	#lea	`$inp_elm_size*8`($inp),$inp
	#dec	$num
	#jnz	.Lenc8x_loop_grande

.Lenc8x_done:
	vzeroupper
___
$code.=<<___ if ($win64);
	movaps	-0xd8(%rax),%xmm6
	movaps	-0xc8(%rax),%xmm7
	movaps	-0xb8(%rax),%xmm8
	movaps	-0xa8(%rax),%xmm9
	movaps	-0x98(%rax),%xmm10
	movaps	-0x88(%rax),%xmm11
	movaps	-0x78(%rax),%xmm12
	movaps	-0x68(%rax),%xmm13
	movaps	-0x58(%rax),%xmm14
	movaps	-0x48(%rax),%xmm15
___
if ($ENV{SARCASM}) {
  # SARCASM-linux reloads the saved stack pointer first: the body reused
  # %rax (it holds $rounds, not the saved rsp), so the carrier must be
  # re-materialized from its slot before the restores below can read through
  # it. (Gas skips the reload: %rax still holds the saved rsp on its paths.)
  $code.=<<___;
	mov	16(%rsp),%rax			# original %rsp
___
}
$code.=<<___;
	mov	-48(%rax),%r15
.cfi_restore	%r15
	mov	-40(%rax),%r14
.cfi_restore	%r14
	mov	-32(%rax),%r13
.cfi_restore	%r13
	mov	-24(%rax),%r12
.cfi_restore	%r12
	mov	-16(%rax),%rbp
.cfi_restore	%rbp
	mov	-8(%rax),%rbx
.cfi_restore	%rbx
	lea	(%rax),%rsp
.cfi_def_cfa_register	%rsp
.Lenc8x_epilogue:
	ret
.cfi_endproc
.size	aesni_multi_cbc_encrypt_avx,.-aesni_multi_cbc_encrypt_avx

.type	aesni_multi_cbc_decrypt_avx,\@function,3
.align	32
aesni_multi_cbc_decrypt_avx: #! void(ptr,ptr,int)
.cfi_startproc
_avx_cbc_dec_shortcut:
	mov	%rsp,%rax
.cfi_def_cfa_register	%rax
	push	%rbx
.cfi_push	%rbx
	push	%rbp
.cfi_push	%rbp
	push	%r12
.cfi_push	%r12
	push	%r13
.cfi_push	%r13
	push	%r14
.cfi_push	%r14
	push	%r15
.cfi_push	%r15
___
$code.=<<___ if ($win64);
	lea	-0xa8(%rsp),%rsp
	movaps	%xmm6,(%rsp)
	movaps	%xmm7,0x10(%rsp)
	movaps	%xmm8,0x20(%rsp)
	movaps	%xmm9,0x30(%rsp)
	movaps	%xmm10,0x40(%rsp)
	movaps	%xmm11,0x50(%rsp)
	movaps	%xmm12,-0x78(%rax)
	movaps	%xmm13,-0x68(%rax)
	movaps	%xmm14,-0x58(%rax)
	movaps	%xmm15,-0x48(%rax)
___
$code.=<<___;
	# stack layout
	#
	# +0	output sink
	# +16	input sink [original %rsp and $num]
	# +32	counters
	# +64	distances between inputs and outputs
	# +128	off-load area for @inp[0..3]
	# +192	IV/input offload

	sub	\$256,%rsp
	and	\$-256,%rsp
	sub	\$192,%rsp
	mov	%rax,16(%rsp)			# original %rsp
.cfi_cfa_expression	%rsp+16,deref,+8

.Ldec8x_body:
	vzeroupper
	vmovdqu	($key),$zero			# 0-round key
	lea	0x78($key),$key			# size optimization
	lea	`$inp_elm_size*4`($inp),$inp
	shr	\$1,$num

.Ldec8x_loop_grande:
	#mov	$num,24(%rsp)			# original $num
	xor	$num,$num
___
for($i=0;$i<8;$i++) {
  my $temp = $i ? $offload : $offset;
    $ptr_reg=&pointer_register($flavour,@ptr[$i]);
    $temp_reg=&pointer_register($flavour,$temp);
    $code.=<<___;
	# borrow $one for number of blocks
	mov	`$inp_elm_size*$i+2*$ptr_size-$inp_elm_size*4`($inp),$one
	# input pointer
	mov	`$inp_elm_size*$i+0-$inp_elm_size*4`($inp),$ptr_reg		#! load ptr
	cmp	$num,$one
	# output pointer
	mov	`$inp_elm_size*$i+$ptr_size-$inp_elm_size*4`($inp),$temp_reg		#! load ptr
	cmovg	$one,$num			# find maximum
	test	$one,$one
	# load IV
	vmovdqu	`$inp_elm_size*$i+2*$ptr_size+8-$inp_elm_size*4`($inp),@out[$i]
	mov	$one,`32+4*$i`(%rsp)		# initialize counters
___
  # The canceled input pointer becomes the sink object and the output
  # position is kept as a proper capability in the slot; see the encrypt
  # loop above for the register choice.
  if ($ENV{SARCASM}) {
    my $tmp = $i ? $offset : $sink;
    $code.=<<___;
	leaq	aesni_mb_sink(%rip),$tmp
	cmovle	$tmp,@ptr[$i]			# cancel input
	mov	$temp,`64+8*$i`(%rsp)		#! store ptr
	vmovdqu	@out[$i],`192+16*$i`(%rsp)	# offload IV
___
  } else {
    $code.=<<___;
	cmovle	%rsp,@ptr[$i]			# cancel input
	sub	@ptr[$i],$temp			# distance between input and output
	mov	$temp,`64+8*$i`(%rsp)		# initialize distances
	vmovdqu	@out[$i],`192+16*$i`(%rsp)	# offload IV
___
  }
}
$code.=<<___;
	test	$num,$num
	jz	.Ldec8x_done

	vmovups	0x10-0x78($key),$rndkey1
	vmovups	0x20-0x78($key),$rndkey0
	mov	0xf0-0x78($key),$rounds
___
$code.=<<___;
	 lea	192+128(%rsp),$offload		# offload area
___
$code.=<<___;

	vmovdqu	(@ptr[0]),@out[0]		# load inputs
	vmovdqu	(@ptr[1]),@out[1]
	vmovdqu	(@ptr[2]),@out[2]
	vmovdqu	(@ptr[3]),@out[3]
	vmovdqu	(@ptr[4]),@out[4]
	vmovdqu	(@ptr[5]),@out[5]
	vmovdqu	(@ptr[6]),@out[6]
	vmovdqu	(@ptr[7]),@out[7]
	vmovdqu	@out[0],0x00($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)	# offload inputs
	vpxor	$zero,@out[0],@out[0]		# xor inputs with 0-round
	vmovdqu	@out[1],0x10($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	vpxor	$zero,@out[1],@out[1]
	vmovdqu	@out[2],0x20($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	vpxor	$zero,@out[2],@out[2]
	vmovdqu	@out[3],0x30($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	vpxor	$zero,@out[3],@out[3]
	vmovdqu	@out[4],0x40($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	vpxor	$zero,@out[4],@out[4]
	vmovdqu	@out[5],0x50($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	vpxor	$zero,@out[5],@out[5]
	vmovdqu	@out[6],0x60($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	vpxor	$zero,@out[6],@out[6]
	vmovdqu	@out[7],0x70($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	vpxor	$zero,@out[7],@out[7]
___
  # The ping-pong areas are a declared stack buffer (offd, %rsp+192 ..
  # %rsp+448): the ($offload) accesses carry `#! stack buffer` annotations,
  # so sarcasm lowers the base lea into its synthesized buffer region and
  # they become runtime-checked by-address accesses.  $offload ping-pongs
  # between the IV area (%rsp+192) and the ciphertext area (%rsp+320): the
  # sub256/and/sub192 frame leaves %rsp%256==64, so the areas sit exactly
  # 0x80 apart, and the xor $0x80 below (matching xor $128 at the loop
  # bottom) flips the base between them, keeping it inside the declared
  # range.
$code.=<<___;
	xor	\$0x80,$offload
___
$code.=<<___;
	mov	\$1,$one			# constant of 1
	jmp	.Loop_dec8x

.align	32
.Loop_dec8x:
___
for($i=0;$i<8;$i++) {
my $rndkey=($i&1)?$rndkey0:$rndkey1;
  if ($ENV{SARCASM}) {
    # Capability-safe form: see the encrypt loop above.
    $code.=<<___;
	vaesdec		$rndkey,@out[0],@out[0]
	 mov		64+8*$i(%rsp),$offset		#! load ptr
	 cmp		32+4*$i(%rsp),$one
	vaesdec		$rndkey,@out[1],@out[1]
	prefetcht0	31(@ptr[$i])			# prefetch input
	vaesdec		$rndkey,@out[2],@out[2]
___
    $code.=<<___ if ($i>1);
	prefetcht0	15(@ptr[$i-2])			# prefetch output
___
    $code.=<<___;
	vaesdec		$rndkey,@out[3],@out[3]
	jl	.LcinD$i
	leaq	aesni_mb_sink(%rip),@ptr[$i]
.LcinD$i:
	vaesdec		$rndkey,@out[4],@out[4]
	jle	.LcoutD$i
	leaq	aesni_mb_sink(%rip),$offset
.LcoutD$i:
	vaesdec		$rndkey,@out[5],@out[5]
	 vmovdqu	16(@ptr[$i]),@inp[$i%4]		# load input
	 mov		$offset,64+8*$i(%rsp)		#! store ptr
	vaesdec		$rndkey,@out[6],@out[6]
	vaesdec		$rndkey,@out[7],@out[7]
	vmovups		`16*(3+$i)-0x78`($key),$rndkey
	 lea		16(@ptr[$i]),@ptr[$i]	# advance input
___
    $code.=<<___ if ($i<4);
	 vmovdqu	@inp[$i%4],`128+16*$i`(%rsp)	# off-load
___
  } else {
$code.=<<___;
	vaesdec		$rndkey,@out[0],@out[0]
	 cmp		32+4*$i(%rsp),$one
___
$code.=<<___ if ($i);
	 mov		64+8*$i(%rsp),$offset
___
$code.=<<___;
	vaesdec		$rndkey,@out[1],@out[1]
	prefetcht0	31(@ptr[$i])			# prefetch input
	vaesdec		$rndkey,@out[2],@out[2]
___
$code.=<<___ if ($i>1);
	prefetcht0	15(@ptr[$i-2])			# prefetch output
___
$code.=<<___;
	vaesdec		$rndkey,@out[3],@out[3]
	 lea		(@ptr[$i],$offset),$offset
	 cmovge		%rsp,@ptr[$i]			# cancel input
	vaesdec		$rndkey,@out[4],@out[4]
	 cmovg		%rsp,$offset			# sink output
	vaesdec		$rndkey,@out[5],@out[5]
	 sub		@ptr[$i],$offset
	vaesdec		$rndkey,@out[6],@out[6]
	 vmovdqu	16(@ptr[$i]),@inp[$i%4]		# load input
	 mov		$offset,64+8*$i(%rsp)
	vaesdec		$rndkey,@out[7],@out[7]
	vmovups		`16*(3+$i)-0x78`($key),$rndkey
	 lea		16(@ptr[$i],$offset),@ptr[$i]	# switch to output
___
$code.=<<___ if ($i<4);
	 vmovdqu	@inp[$i%4],`128+16*$i`(%rsp)	# off-load
___
  }
}
$code.=<<___;
	 vmovdqu	32(%rsp),$counters
	prefetcht0	15(@ptr[$i-2])			# prefetch output
	prefetcht0	15(@ptr[$i-1])
	cmp	\$11,$rounds
	jb	.Ldec8x_tail

	vaesdec		$rndkey1,@out[0],@out[0]
	vaesdec		$rndkey1,@out[1],@out[1]
	vaesdec		$rndkey1,@out[2],@out[2]
	vaesdec		$rndkey1,@out[3],@out[3]
	vaesdec		$rndkey1,@out[4],@out[4]
	vaesdec		$rndkey1,@out[5],@out[5]
	vaesdec		$rndkey1,@out[6],@out[6]
	vaesdec		$rndkey1,@out[7],@out[7]
	vmovups		0xb0-0x78($key),$rndkey1

	vaesdec		$rndkey0,@out[0],@out[0]
	vaesdec		$rndkey0,@out[1],@out[1]
	vaesdec		$rndkey0,@out[2],@out[2]
	vaesdec		$rndkey0,@out[3],@out[3]
	vaesdec		$rndkey0,@out[4],@out[4]
	vaesdec		$rndkey0,@out[5],@out[5]
	vaesdec		$rndkey0,@out[6],@out[6]
	vaesdec		$rndkey0,@out[7],@out[7]
	vmovups		0xc0-0x78($key),$rndkey0
	je	.Ldec8x_tail

	vaesdec		$rndkey1,@out[0],@out[0]
	vaesdec		$rndkey1,@out[1],@out[1]
	vaesdec		$rndkey1,@out[2],@out[2]
	vaesdec		$rndkey1,@out[3],@out[3]
	vaesdec		$rndkey1,@out[4],@out[4]
	vaesdec		$rndkey1,@out[5],@out[5]
	vaesdec		$rndkey1,@out[6],@out[6]
	vaesdec		$rndkey1,@out[7],@out[7]
	vmovups		0xd0-0x78($key),$rndkey1

	vaesdec		$rndkey0,@out[0],@out[0]
	vaesdec		$rndkey0,@out[1],@out[1]
	vaesdec		$rndkey0,@out[2],@out[2]
	vaesdec		$rndkey0,@out[3],@out[3]
	vaesdec		$rndkey0,@out[4],@out[4]
	vaesdec		$rndkey0,@out[5],@out[5]
	vaesdec		$rndkey0,@out[6],@out[6]
	vaesdec		$rndkey0,@out[7],@out[7]
	vmovups		0xe0-0x78($key),$rndkey0

.Ldec8x_tail:
	vaesdec		$rndkey1,@out[0],@out[0]
	 vpxor		$zero,$zero,$zero
	vaesdec		$rndkey1,@out[1],@out[1]
	vaesdec		$rndkey1,@out[2],@out[2]
	 vpcmpgtd	$zero,$counters,$zero
	vaesdec		$rndkey1,@out[3],@out[3]
	vaesdec		$rndkey1,@out[4],@out[4]
	 vpaddd		$counters,$zero,$zero		# decrement counters
	 vmovdqu	48(%rsp),$counters
	vaesdec		$rndkey1,@out[5],@out[5]
	 mov		64(%rsp),$offset		#! load ptr		# pre-load 1st offset
	vaesdec		$rndkey1,@out[6],@out[6]
	vaesdec		$rndkey1,@out[7],@out[7]
	vmovups		0x10-0x78($key),$rndkey1

	vaesdeclast	$rndkey0,@out[0],@out[0]
	 vmovdqa	$zero,32(%rsp)			# update counters
	 vpxor		$zero,$zero,$zero
	vaesdeclast	$rndkey0,@out[1],@out[1]
	vpxor		0x00($offload),@out[0],@out[0]	#! stack buffer (offd, %rsp + 192, %rsp + 448)	# xor with IV
	vaesdeclast	$rndkey0,@out[2],@out[2]
	vpxor		0x10($offload),@out[1],@out[1]	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpcmpgtd	$zero,$counters,$zero
	vaesdeclast	$rndkey0,@out[3],@out[3]
	vpxor		0x20($offload),@out[2],@out[2]	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	vaesdeclast	$rndkey0,@out[4],@out[4]
	vpxor		0x30($offload),@out[3],@out[3]	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpaddd		$zero,$counters,$counters	# decrement counters
	 vmovdqu	-0x78($key),$zero		# 0-round
	vaesdeclast	$rndkey0,@out[5],@out[5]
	vpxor		0x40($offload),@out[4],@out[4]	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	vaesdeclast	$rndkey0,@out[6],@out[6]
	vpxor		0x50($offload),@out[5],@out[5]	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vmovdqa	$counters,48(%rsp)		# update counters
	vaesdeclast	$rndkey0,@out[7],@out[7]
	vpxor		0x60($offload),@out[6],@out[6]	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	vmovups		0x20-0x78($key),$rndkey0

___
if ($ENV{SARCASM}) {
  # write outputs through the output-position capabilities
  $code.=<<___;
	vmovups		@out[0],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+0(%rsp)		#! store ptr
	 vmovdqu	128+0(%rsp),@out[0]
	vpxor		0x70($offload),@out[7],@out[7]	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	mov		64+8(%rsp),$offset		#! load ptr
	vmovups		@out[1],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+8(%rsp)		#! store ptr
	 vmovdqu	@out[0],0x00($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		$zero,@out[0],@out[0]
	 vmovdqu	128+16(%rsp),@out[1]
	mov		64+16(%rsp),$offset		#! load ptr
	vmovups		@out[2],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+16(%rsp)		#! store ptr
	 vmovdqu	@out[1],0x10($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		$zero,@out[1],@out[1]
	 vmovdqu	128+32(%rsp),@out[2]
	mov		64+24(%rsp),$offset		#! load ptr
	vmovups		@out[3],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+24(%rsp)		#! store ptr
	 vmovdqu	@out[2],0x20($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		$zero,@out[2],@out[2]
	 vmovdqu	128+48(%rsp),@out[3]
	mov		64+32(%rsp),$offset		#! load ptr
	vmovups		@out[4],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+32(%rsp)		#! store ptr
	 vmovdqu	@out[3],0x30($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		$zero,@out[3],@out[3]
	 vmovdqu	@inp[0],0x40($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		@inp[0],$zero,@out[4]
	mov		64+40(%rsp),$offset		#! load ptr
	vmovups		@out[5],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+40(%rsp)		#! store ptr
	 vmovdqu	@inp[1],0x50($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		@inp[1],$zero,@out[5]
	mov		64+48(%rsp),$offset		#! load ptr
	vmovups		@out[6],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+48(%rsp)		#! store ptr
	 vmovdqu	@inp[2],0x60($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		@inp[2],$zero,@out[6]
	mov		64+56(%rsp),$offset		#! load ptr
	vmovups		@out[7],($offset)
	 lea		16($offset),$offset
	 mov		$offset,64+56(%rsp)		#! store ptr
	 vmovdqu	@inp[3],0x70($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		@inp[3],$zero,@out[7]

	xor	\$128,$offload
	dec	$num
	jnz	.Loop_dec8x
___
} else {
  $code.=<<___;
	vmovups		@out[0],-16(@ptr[0])		# write output
	 sub		$offset,@ptr[0]			# switch to input
	 vmovdqu	128+0(%rsp),@out[0]
	vpxor		0x70($offload),@out[7],@out[7]	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	vmovups		@out[1],-16(@ptr[1])
	 sub		`64+1*8`(%rsp),@ptr[1]
	 vmovdqu	@out[0],0x00($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		$zero,@out[0],@out[0]
	 vmovdqu	128+16(%rsp),@out[1]
	vmovups		@out[2],-16(@ptr[2])
	 sub		`64+2*8`(%rsp),@ptr[2]
	 vmovdqu	@out[1],0x10($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		$zero,@out[1],@out[1]
	 vmovdqu	128+32(%rsp),@out[2]
	vmovups		@out[3],-16(@ptr[3])
	 sub		`64+3*8`(%rsp),@ptr[3]
	 vmovdqu	@out[2],0x20($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		$zero,@out[2],@out[2]
	 vmovdqu	128+48(%rsp),@out[3]
	vmovups		@out[4],-16(@ptr[4])
	 sub		`64+4*8`(%rsp),@ptr[4]
	 vmovdqu	@out[3],0x30($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		$zero,@out[3],@out[3]
	 vmovdqu	@inp[0],0x40($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		@inp[0],$zero,@out[4]
	vmovups		@out[5],-16(@ptr[5])
	 sub		`64+5*8`(%rsp),@ptr[5]
	 vmovdqu	@inp[1],0x50($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		@inp[1],$zero,@out[5]
	vmovups		@out[6],-16(@ptr[6])
	 sub		`64+6*8`(%rsp),@ptr[6]
	 vmovdqu	@inp[2],0x60($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		@inp[2],$zero,@out[6]
	vmovups		@out[7],-16(@ptr[7])
	 sub		`64+7*8`(%rsp),@ptr[7]
	 vmovdqu	@inp[3],0x70($offload)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	 vpxor		@inp[3],$zero,@out[7]

	xor	\$128,$offload
	dec	$num
	jnz	.Loop_dec8x

	mov	16(%rsp),%rax			# original %rsp
___
}
$code.=<<___;
.cfi_def_cfa	%rax,8
	#mov	24(%rsp),$num
	#lea	`$inp_elm_size*8`($inp),$inp
	#dec	$num
	#jnz	.Ldec8x_loop_grande

.Ldec8x_done:
	vzeroupper
___
$code.=<<___ if ($win64);
	movaps	-0xd8(%rax),%xmm6
	movaps	-0xc8(%rax),%xmm7
	movaps	-0xb8(%rax),%xmm8
	movaps	-0xa8(%rax),%xmm9
	movaps	-0x98(%rax),%xmm10
	movaps	-0x88(%rax),%xmm11
	movaps	-0x78(%rax),%xmm12
	movaps	-0x68(%rax),%xmm13
	movaps	-0x58(%rax),%xmm14
	movaps	-0x48(%rax),%xmm15
___
if ($ENV{SARCASM}) {
  # SARCASM-linux reloads the saved stack pointer first: the body reused
  # %rax (it holds $rounds, not the saved rsp), so the carrier must be
  # re-materialized from its slot before the restores below can read through
  # it. (Gas skips the reload: %rax still holds the saved rsp on its paths.)
  $code.=<<___;
	mov	16(%rsp),%rax			# original %rsp
___
}
$code.=<<___;
	mov	-48(%rax),%r15
.cfi_restore	%r15
	mov	-40(%rax),%r14
.cfi_restore	%r14
	mov	-32(%rax),%r13
.cfi_restore	%r13
	mov	-24(%rax),%r12
.cfi_restore	%r12
	mov	-16(%rax),%rbp
.cfi_restore	%rbp
	mov	-8(%rax),%rbx
.cfi_restore	%rbx
	lea	(%rax),%rsp
.cfi_def_cfa_register	%rsp
.Ldec8x_epilogue:
	ret
.cfi_endproc
.size	aesni_multi_cbc_decrypt_avx,.-aesni_multi_cbc_decrypt_avx
___
						}}}

if ($win64) {
# EXCEPTION_DISPOSITION handler (EXCEPTION_RECORD *rec,ULONG64 frame,
#		CONTEXT *context,DISPATCHER_CONTEXT *disp)
$rec="%rcx";
$frame="%rdx";
$context="%r8";
$disp="%r9";

$code.=<<___;
.extern	__imp_RtlVirtualUnwind
.type	se_handler,\@abi-omnipotent
.align	16
se_handler:
	push	%rsi
	push	%rdi
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	pushfq
	sub	\$64,%rsp

	mov	120($context),%rax	# pull context->Rax
	mov	248($context),%rbx	# pull context->Rip

	mov	8($disp),%rsi		# disp->ImageBase
	mov	56($disp),%r11		# disp->HandlerData

	mov	0(%r11),%r10d		# HandlerData[0]
	lea	(%rsi,%r10),%r10	# prologue label
	cmp	%r10,%rbx		# context->Rip<.Lprologue
	jb	.Lin_prologue

	mov	152($context),%rax	# pull context->Rsp

	mov	4(%r11),%r10d		# HandlerData[1]
	lea	(%rsi,%r10),%r10	# epilogue label
	cmp	%r10,%rbx		# context->Rip>=.Lepilogue
	jae	.Lin_prologue

	mov	16(%rax),%rax		# pull saved stack pointer

	mov	-8(%rax),%rbx
	mov	-16(%rax),%rbp
	mov	-24(%rax),%r12
	mov	-32(%rax),%r13
	mov	-40(%rax),%r14
	mov	-48(%rax),%r15
	mov	%rbx,144($context)	# restore context->Rbx
	mov	%rbp,160($context)	# restore context->Rbp
	mov	%r12,216($context)	# restore context->R12
	mov	%r13,224($context)	# restore context->R13
	mov	%r14,232($context)	# restore context->R14
	mov	%r15,240($context)	# restore context->R15

	lea	-56-10*16(%rax),%rsi
	lea	512($context),%rdi	# &context.Xmm6
	mov	\$20,%ecx
	.long	0xa548f3fc		# cld; rep movsq

.Lin_prologue:
	mov	8(%rax),%rdi
	mov	16(%rax),%rsi
	mov	%rax,152($context)	# restore context->Rsp
	mov	%rsi,168($context)	# restore context->Rsi
	mov	%rdi,176($context)	# restore context->Rdi

	mov	40($disp),%rdi		# disp->ContextRecord
	mov	$context,%rsi		# context
	mov	\$154,%ecx		# sizeof(CONTEXT)
	.long	0xa548f3fc		# cld; rep movsq

	mov	$disp,%rsi
	xor	%rcx,%rcx		# arg1, UNW_FLAG_NHANDLER
	mov	8(%rsi),%rdx		# arg2, disp->ImageBase
	mov	0(%rsi),%r8		# arg3, disp->ControlPc
	mov	16(%rsi),%r9		# arg4, disp->FunctionEntry
	mov	40(%rsi),%r10		# disp->ContextRecord
	lea	56(%rsi),%r11		# &disp->HandlerData
	lea	24(%rsi),%r12		# &disp->EstablisherFrame
	mov	%r10,32(%rsp)		# arg5
	mov	%r11,40(%rsp)		# arg6
	mov	%r12,48(%rsp)		# arg7
	mov	%rcx,56(%rsp)		# arg8, (NULL)
	call	*__imp_RtlVirtualUnwind(%rip)

	mov	\$1,%eax		# ExceptionContinueSearch
	add	\$64,%rsp
	popfq
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	pop	%rdi
	pop	%rsi
	ret
.size	se_handler,.-se_handler

.section	.pdata
.align	4
	.rva	.LSEH_begin_aesni_multi_cbc_encrypt
	.rva	.LSEH_end_aesni_multi_cbc_encrypt
	.rva	.LSEH_info_aesni_multi_cbc_encrypt
	.rva	.LSEH_begin_aesni_multi_cbc_decrypt
	.rva	.LSEH_end_aesni_multi_cbc_decrypt
	.rva	.LSEH_info_aesni_multi_cbc_decrypt
___
$code.=<<___ if ($avx);
	.rva	.LSEH_begin_aesni_multi_cbc_encrypt_avx
	.rva	.LSEH_end_aesni_multi_cbc_encrypt_avx
	.rva	.LSEH_info_aesni_multi_cbc_encrypt_avx
	.rva	.LSEH_begin_aesni_multi_cbc_decrypt_avx
	.rva	.LSEH_end_aesni_multi_cbc_decrypt_avx
	.rva	.LSEH_info_aesni_multi_cbc_decrypt_avx
___
$code.=<<___;
.section	.xdata
.align	8
.LSEH_info_aesni_multi_cbc_encrypt:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Lenc4x_body,.Lenc4x_epilogue		# HandlerData[]
.LSEH_info_aesni_multi_cbc_decrypt:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Ldec4x_body,.Ldec4x_epilogue		# HandlerData[]
___
$code.=<<___ if ($avx);
.LSEH_info_aesni_multi_cbc_encrypt_avx:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Lenc8x_body,.Lenc8x_epilogue		# HandlerData[]
.LSEH_info_aesni_multi_cbc_decrypt_avx:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Ldec8x_body,.Ldec8x_epilogue		# HandlerData[]
___
}
####################################################################

sub rex {
  local *opcode=shift;
  my ($dst,$src)=@_;
  my $rex=0;

    $rex|=0x04			if($dst>=8);
    $rex|=0x01			if($src>=8);
    push @opcode,$rex|0x40	if($rex);
}

sub aesni {
  my $line=shift;
  my @opcode=(0x66);

    if ($line=~/(aeskeygenassist)\s+\$([x0-9a-f]+),\s*%xmm([0-9]+),\s*%xmm([0-9]+)/) {
	rex(\@opcode,$4,$3);
	push @opcode,0x0f,0x3a,0xdf;
	push @opcode,0xc0|($3&7)|(($4&7)<<3);	# ModR/M
	my $c=$2;
	push @opcode,$c=~/^0/?oct($c):$c;
	return ".byte\t".join(',',@opcode);
    }
    elsif ($line=~/(aes[a-z]+)\s+%xmm([0-9]+),\s*%xmm([0-9]+)/) {
	my %opcodelet = (
		"aesimc" => 0xdb,
		"aesenc" => 0xdc,	"aesenclast" => 0xdd,
		"aesdec" => 0xde,	"aesdeclast" => 0xdf
	);
	return undef if (!defined($opcodelet{$1}));
	rex(\@opcode,$3,$2);
	push @opcode,0x0f,0x38,$opcodelet{$1};
	push @opcode,0xc0|($2&7)|(($3&7)<<3);	# ModR/M
	return ".byte\t".join(',',@opcode);
    }
    elsif ($line=~/(aes[a-z]+)\s+([0x1-9a-fA-F]*)\(%rsp\),\s*%xmm([0-9]+)/) {
	my %opcodelet = (
		"aesenc" => 0xdc,	"aesenclast" => 0xdd,
		"aesdec" => 0xde,	"aesdeclast" => 0xdf
	);
	return undef if (!defined($opcodelet{$1}));
	my $off = $2;
	push @opcode,0x44 if ($3>=8);
	push @opcode,0x0f,0x38,$opcodelet{$1};
	push @opcode,0x44|(($3&7)<<3),0x24;	# ModR/M
	push @opcode,($off=~/^0/?oct($off):$off)&0xff;
	return ".byte\t".join(',',@opcode);
    }
    return $line;
}

if ($ENV{SARCASM}) {
  # Sink for cancelled descriptors (see the sink-register notes at the
  # cmov sites).
  $code.=<<___;
	.comm	aesni_mb_sink,4096,16
___
}
$code =~ s/\`([^\`]*)\`/eval($1)/gem;
$code =~ s/\b(aes.*%xmm[0-9]+).*$/aesni($1)/gem;

print $code;
close STDOUT or die "error closing STDOUT: $!";
