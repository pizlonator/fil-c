#undef FORCED_FLAG
#ifdef FORCE_FLAGS
#define FORCED_FLAG 1LL
#else
#define FORCED_FLAG 0LL
#endif

// basename ^<1as: ^<1as:
#undef OPTSTR_basename
#define OPTSTR_basename "^<1as:"
#ifdef CLEANUP_basename
#undef CLEANUP_basename
#undef FOR_basename
#undef FLAG_s
#undef FLAG_a
#endif

// cat uvte uvte
#undef OPTSTR_cat
#define OPTSTR_cat "uvte"
#ifdef CLEANUP_cat
#undef CLEANUP_cat
#undef FOR_cat
#undef FLAG_e
#undef FLAG_t
#undef FLAG_v
#undef FLAG_u
#endif

// chmod <2?vfR[-vf] <2?vfR[-vf]
#undef OPTSTR_chmod
#define OPTSTR_chmod "<2?vfR[-vf]"
#ifdef CLEANUP_chmod
#undef CLEANUP_chmod
#undef FOR_chmod
#undef FLAG_R
#undef FLAG_f
#undef FLAG_v
#endif

// cmp <1>4ls(silent)(quiet)n#<1[!ls] <1>4ls(silent)(quiet)n#<1[!ls]
#undef OPTSTR_cmp
#define OPTSTR_cmp "<1>4ls(silent)(quiet)n#<1[!ls]"
#ifdef CLEANUP_cmp
#undef CLEANUP_cmp
#undef FOR_cmp
#undef FLAG_n
#undef FLAG_s
#undef FLAG_l
#endif

// echo ^?Een[-eE] ^?Een[-eE]
#undef OPTSTR_echo
#define OPTSTR_echo "^?Een[-eE]"
#ifdef CLEANUP_echo
#undef CLEANUP_echo
#undef FOR_echo
#undef FLAG_n
#undef FLAG_e
#undef FLAG_E
#endif

// fold bsw#<1=80 bsw#<1=80
#undef OPTSTR_fold
#define OPTSTR_fold "bsw#<1=80"
#ifdef CLEANUP_fold
#undef CLEANUP_fold
#undef FOR_fold
#undef FLAG_w
#undef FLAG_s
#undef FLAG_b
#endif

// grep (line-buffered)(color):;(exclude-dir)*S(exclude)*M(include)*ZzEFHIab(byte-offset)h(no-filename)ino(only-matching)rRsvwc(count)L(files-without-match)l(files-with-matches)q(quiet)(silent)e*f*C#B#A#m#x[!wx][!EF] (line-buffered)(color):;(exclude-dir)*S(exclude)*M(include)*ZzEFHIab(byte-offset)h(no-filename)ino(only-matching)rRsvwc(count)L(files-without-match)l(files-with-matches)q(quiet)(silent)e*f*C#B#A#m#x[!wx][!EF]
#undef OPTSTR_grep
#define OPTSTR_grep "(line-buffered)(color):;(exclude-dir)*S(exclude)*M(include)*ZzEFHIab(byte-offset)h(no-filename)ino(only-matching)rRsvwc(count)L(files-without-match)l(files-with-matches)q(quiet)(silent)e*f*C#B#A#m#x[!wx][!EF]"
#ifdef CLEANUP_grep
#undef CLEANUP_grep
#undef FOR_grep
#undef FLAG_x
#undef FLAG_m
#undef FLAG_A
#undef FLAG_B
#undef FLAG_C
#undef FLAG_f
#undef FLAG_e
#undef FLAG_q
#undef FLAG_l
#undef FLAG_L
#undef FLAG_c
#undef FLAG_w
#undef FLAG_v
#undef FLAG_s
#undef FLAG_R
#undef FLAG_r
#undef FLAG_o
#undef FLAG_n
#undef FLAG_i
#undef FLAG_h
#undef FLAG_b
#undef FLAG_a
#undef FLAG_I
#undef FLAG_H
#undef FLAG_F
#undef FLAG_E
#undef FLAG_z
#undef FLAG_Z
#undef FLAG_M
#undef FLAG_S
#undef FLAG_exclude_dir
#undef FLAG_color
#undef FLAG_line_buffered
#endif

// gzip n(no-name)cdfkt123456789[-123456789] n(no-name)cdfkt123456789[-123456789]
#undef OPTSTR_gzip
#define OPTSTR_gzip "n(no-name)cdfkt123456789[-123456789]"
#ifdef CLEANUP_gzip
#undef CLEANUP_gzip
#undef FOR_gzip
#undef FLAG_9
#undef FLAG_8
#undef FLAG_7
#undef FLAG_6
#undef FLAG_5
#undef FLAG_4
#undef FLAG_3
#undef FLAG_2
#undef FLAG_1
#undef FLAG_t
#undef FLAG_k
#undef FLAG_f
#undef FLAG_d
#undef FLAG_c
#undef FLAG_n
#endif

// head ?n(lines)#<0=10c(bytes)#<0qv[-nc] ?n(lines)#<0=10c(bytes)#<0qv[-nc]
#undef OPTSTR_head
#define OPTSTR_head "?n(lines)#<0=10c(bytes)#<0qv[-nc]"
#ifdef CLEANUP_head
#undef CLEANUP_head
#undef FOR_head
#undef FLAG_v
#undef FLAG_q
#undef FLAG_c
#undef FLAG_n
#endif

// ln <1rt:Tvnfs <1rt:Tvnfs
#undef OPTSTR_ln
#define OPTSTR_ln "<1rt:Tvnfs"
#ifdef CLEANUP_ln
#undef CLEANUP_ln
#undef FOR_ln
#undef FLAG_s
#undef FLAG_f
#undef FLAG_n
#undef FLAG_v
#undef FLAG_T
#undef FLAG_t
#undef FLAG_r
#endif

// ls (sort):(color):;(full-time)(show-control-chars)ÿ(block-size)#=1024<1¡(group-directories-first)þZgoACFHLNRSUXabcdfhikl@mnpqrstuw#=80<0x1[-Cxm1][-Cxml][-Cxmo][-Cxmg][-cu][-ftS][-HL][-Nqb][-kÿ] (sort):(color):;(full-time)(show-control-chars)ÿ(block-size)#=1024<1¡(group-directories-first)þZgoACFHLNRSUXabcdfhikl@mnpqrstuw#=80<0x1[-Cxm1][-Cxml][-Cxmo][-Cxmg][-cu][-ftS][-HL][-Nqb][-kÿ]
#undef OPTSTR_ls
#define OPTSTR_ls "(sort):(color):;(full-time)(show-control-chars)ÿ(block-size)#=1024<1¡(group-directories-first)þZgoACFHLNRSUXabcdfhikl@mnpqrstuw#=80<0x1[-Cxm1][-Cxml][-Cxmo][-Cxmg][-cu][-ftS][-HL][-Nqb][-kÿ]"
#ifdef CLEANUP_ls
#undef CLEANUP_ls
#undef FOR_ls
#undef FLAG_1
#undef FLAG_x
#undef FLAG_w
#undef FLAG_u
#undef FLAG_t
#undef FLAG_s
#undef FLAG_r
#undef FLAG_q
#undef FLAG_p
#undef FLAG_n
#undef FLAG_m
#undef FLAG_l
#undef FLAG_k
#undef FLAG_i
#undef FLAG_h
#undef FLAG_f
#undef FLAG_d
#undef FLAG_c
#undef FLAG_b
#undef FLAG_a
#undef FLAG_X
#undef FLAG_U
#undef FLAG_S
#undef FLAG_R
#undef FLAG_N
#undef FLAG_L
#undef FLAG_H
#undef FLAG_F
#undef FLAG_C
#undef FLAG_A
#undef FLAG_o
#undef FLAG_g
#undef FLAG_Z
#undef FLAG_X7E
#undef FLAG_X21
#undef FLAG_X7F
#undef FLAG_show_control_chars
#undef FLAG_full_time
#undef FLAG_color
#undef FLAG_sort
#endif

// mkdir <1vp(parent)(parents)m: <1Z:vp(parent)(parents)m:
#undef OPTSTR_mkdir
#define OPTSTR_mkdir "<1vp(parent)(parents)m:"
#ifdef CLEANUP_mkdir
#undef CLEANUP_mkdir
#undef FOR_mkdir
#undef FLAG_m
#undef FLAG_p
#undef FLAG_v
#undef FLAG_Z
#endif

// od j#vw#<1=16N#xsodcbA:t* j#vw#<1=16N#xsodcbA:t*
#undef OPTSTR_od
#define OPTSTR_od "j#vw#<1=16N#xsodcbA:t*"
#ifdef CLEANUP_od
#undef CLEANUP_od
#undef FOR_od
#undef FLAG_t
#undef FLAG_A
#undef FLAG_b
#undef FLAG_c
#undef FLAG_d
#undef FLAG_o
#undef FLAG_s
#undef FLAG_x
#undef FLAG_N
#undef FLAG_w
#undef FLAG_v
#undef FLAG_j
#endif

// readlink <1vnf(canonicalize)emqz[-mef][-qv] <1vnf(canonicalize)emqz[-mef][-qv]
#undef OPTSTR_readlink
#define OPTSTR_readlink "<1vnf(canonicalize)emqz[-mef][-qv]"
#ifdef CLEANUP_readlink
#undef CLEANUP_readlink
#undef FOR_readlink
#undef FLAG_z
#undef FLAG_q
#undef FLAG_m
#undef FLAG_e
#undef FLAG_f
#undef FLAG_n
#undef FLAG_v
#endif

// realpath   <1(relative-base):R(relative-to):s(no-symlinks)LPemqz[-Ps][-LP][-me]
#undef OPTSTR_realpath
#define OPTSTR_realpath "<1(relative-base):R(relative-to):s(no-symlinks)LPemqz[-Ps][-LP][-me]"
#ifdef CLEANUP_realpath
#undef CLEANUP_realpath
#undef FOR_realpath
#undef FLAG_z
#undef FLAG_q
#undef FLAG_m
#undef FLAG_e
#undef FLAG_P
#undef FLAG_L
#undef FLAG_s
#undef FLAG_R
#undef FLAG_relative_base
#endif

// rm f(force)iRrv[-fi] f(force)iRrv[-fi]
#undef OPTSTR_rm
#define OPTSTR_rm "f(force)iRrv[-fi]"
#ifdef CLEANUP_rm
#undef CLEANUP_rm
#undef FOR_rm
#undef FLAG_v
#undef FLAG_r
#undef FLAG_R
#undef FLAG_i
#undef FLAG_f
#endif

// sed (help)(version)(tarxform)e*f*i:;nErz(null-data)s[+Er] (help)(version)(tarxform)e*f*i:;nErz(null-data)s[+Er]
#undef OPTSTR_sed
#define OPTSTR_sed "(help)(version)(tarxform)e*f*i:;nErz(null-data)s[+Er]"
#ifdef CLEANUP_sed
#undef CLEANUP_sed
#undef FOR_sed
#undef FLAG_s
#undef FLAG_z
#undef FLAG_r
#undef FLAG_E
#undef FLAG_n
#undef FLAG_i
#undef FLAG_f
#undef FLAG_e
#undef FLAG_tarxform
#undef FLAG_version
#undef FLAG_help
#endif

// sort S:T:mo:k*t:xVbMCcszdfirun gS:T:mo:k*t:xVbMCcszdfirun
#undef OPTSTR_sort
#define OPTSTR_sort "S:T:mo:k*t:xVbMCcszdfirun"
#ifdef CLEANUP_sort
#undef CLEANUP_sort
#undef FOR_sort
#undef FLAG_n
#undef FLAG_u
#undef FLAG_r
#undef FLAG_i
#undef FLAG_f
#undef FLAG_d
#undef FLAG_z
#undef FLAG_s
#undef FLAG_c
#undef FLAG_C
#undef FLAG_M
#undef FLAG_b
#undef FLAG_V
#undef FLAG_x
#undef FLAG_t
#undef FLAG_k
#undef FLAG_o
#undef FLAG_m
#undef FLAG_T
#undef FLAG_S
#undef FLAG_g
#endif

// tail ?fFs:c(bytes)-n(lines)-[-cn][-fF] ?fFs:c(bytes)-n(lines)-[-cn][-fF]
#undef OPTSTR_tail
#define OPTSTR_tail "?fFs:c(bytes)-n(lines)-[-cn][-fF]"
#ifdef CLEANUP_tail
#undef CLEANUP_tail
#undef FOR_tail
#undef FLAG_n
#undef FLAG_c
#undef FLAG_s
#undef FLAG_F
#undef FLAG_f
#endif

// tee ia ia
#undef OPTSTR_tee
#define OPTSTR_tee "ia"
#ifdef CLEANUP_tee
#undef CLEANUP_tee
#undef FOR_tee
#undef FLAG_a
#undef FLAG_i
#endif

// tr ^<1>2Ccstd[+cC] ^<1>2Ccstd[+cC]
#undef OPTSTR_tr
#define OPTSTR_tr "^<1>2Ccstd[+cC]"
#ifdef CLEANUP_tr
#undef CLEANUP_tr
#undef FOR_tr
#undef FLAG_d
#undef FLAG_t
#undef FLAG_s
#undef FLAG_c
#undef FLAG_C
#endif

// uname paomvrns paomvrns
#undef OPTSTR_uname
#define OPTSTR_uname "paomvrns"
#ifdef CLEANUP_uname
#undef CLEANUP_uname
#undef FOR_uname
#undef FLAG_s
#undef FLAG_n
#undef FLAG_r
#undef FLAG_v
#undef FLAG_m
#undef FLAG_o
#undef FLAG_a
#undef FLAG_p
#endif

// wc Lcmwl Lcmwl
#undef OPTSTR_wc
#define OPTSTR_wc "Lcmwl"
#ifdef CLEANUP_wc
#undef CLEANUP_wc
#undef FOR_wc
#undef FLAG_l
#undef FLAG_w
#undef FLAG_m
#undef FLAG_c
#undef FLAG_L
#endif

// xargs ^E:P#<0(null)=1optr(no-run-if-empty)n#<1(max-args)s#0[!0E] ^E:P#<0(null)=1optr(no-run-if-empty)n#<1(max-args)s#0[!0E]
#undef OPTSTR_xargs
#define OPTSTR_xargs "^E:P#<0(null)=1optr(no-run-if-empty)n#<1(max-args)s#0[!0E]"
#ifdef CLEANUP_xargs
#undef CLEANUP_xargs
#undef FOR_xargs
#undef FLAG_0
#undef FLAG_s
#undef FLAG_n
#undef FLAG_r
#undef FLAG_t
#undef FLAG_p
#undef FLAG_o
#undef FLAG_P
#undef FLAG_E
#endif

#ifdef FOR_basename
#define CLEANUP_basename
#ifndef TT
#define TT this.basename
#endif
#define FLAG_s (1LL<<0)
#define FLAG_a (1LL<<1)
#endif

#ifdef FOR_cat
#define CLEANUP_cat
#ifndef TT
#define TT this.cat
#endif
#define FLAG_e (1LL<<0)
#define FLAG_t (1LL<<1)
#define FLAG_v (1LL<<2)
#define FLAG_u (1LL<<3)
#endif

#ifdef FOR_chmod
#define CLEANUP_chmod
#ifndef TT
#define TT this.chmod
#endif
#define FLAG_R (1LL<<0)
#define FLAG_f (1LL<<1)
#define FLAG_v (1LL<<2)
#endif

#ifdef FOR_cmp
#define CLEANUP_cmp
#ifndef TT
#define TT this.cmp
#endif
#define FLAG_n (1LL<<0)
#define FLAG_s (1LL<<1)
#define FLAG_l (1LL<<2)
#endif

#ifdef FOR_echo
#define CLEANUP_echo
#ifndef TT
#define TT this.echo
#endif
#define FLAG_n (1LL<<0)
#define FLAG_e (1LL<<1)
#define FLAG_E (1LL<<2)
#endif

#ifdef FOR_fold
#define CLEANUP_fold
#ifndef TT
#define TT this.fold
#endif
#define FLAG_w (1LL<<0)
#define FLAG_s (1LL<<1)
#define FLAG_b (1LL<<2)
#endif

#ifdef FOR_grep
#define CLEANUP_grep
#ifndef TT
#define TT this.grep
#endif
#define FLAG_x (1LL<<0)
#define FLAG_m (1LL<<1)
#define FLAG_A (1LL<<2)
#define FLAG_B (1LL<<3)
#define FLAG_C (1LL<<4)
#define FLAG_f (1LL<<5)
#define FLAG_e (1LL<<6)
#define FLAG_q (1LL<<7)
#define FLAG_l (1LL<<8)
#define FLAG_L (1LL<<9)
#define FLAG_c (1LL<<10)
#define FLAG_w (1LL<<11)
#define FLAG_v (1LL<<12)
#define FLAG_s (1LL<<13)
#define FLAG_R (1LL<<14)
#define FLAG_r (1LL<<15)
#define FLAG_o (1LL<<16)
#define FLAG_n (1LL<<17)
#define FLAG_i (1LL<<18)
#define FLAG_h (1LL<<19)
#define FLAG_b (1LL<<20)
#define FLAG_a (1LL<<21)
#define FLAG_I (1LL<<22)
#define FLAG_H (1LL<<23)
#define FLAG_F (1LL<<24)
#define FLAG_E (1LL<<25)
#define FLAG_z (1LL<<26)
#define FLAG_Z (1LL<<27)
#define FLAG_M (1LL<<28)
#define FLAG_S (1LL<<29)
#define FLAG_exclude_dir (1LL<<30)
#define FLAG_color (1LL<<31)
#define FLAG_line_buffered (1LL<<32)
#endif

#ifdef FOR_gzip
#define CLEANUP_gzip
#ifndef TT
#define TT this.gzip
#endif
#define FLAG_9 (1LL<<0)
#define FLAG_8 (1LL<<1)
#define FLAG_7 (1LL<<2)
#define FLAG_6 (1LL<<3)
#define FLAG_5 (1LL<<4)
#define FLAG_4 (1LL<<5)
#define FLAG_3 (1LL<<6)
#define FLAG_2 (1LL<<7)
#define FLAG_1 (1LL<<8)
#define FLAG_t (1LL<<9)
#define FLAG_k (1LL<<10)
#define FLAG_f (1LL<<11)
#define FLAG_d (1LL<<12)
#define FLAG_c (1LL<<13)
#define FLAG_n (1LL<<14)
#endif

#ifdef FOR_head
#define CLEANUP_head
#ifndef TT
#define TT this.head
#endif
#define FLAG_v (1LL<<0)
#define FLAG_q (1LL<<1)
#define FLAG_c (1LL<<2)
#define FLAG_n (1LL<<3)
#endif

#ifdef FOR_ln
#define CLEANUP_ln
#ifndef TT
#define TT this.ln
#endif
#define FLAG_s (1LL<<0)
#define FLAG_f (1LL<<1)
#define FLAG_n (1LL<<2)
#define FLAG_v (1LL<<3)
#define FLAG_T (1LL<<4)
#define FLAG_t (1LL<<5)
#define FLAG_r (1LL<<6)
#endif

#ifdef FOR_ls
#define CLEANUP_ls
#ifndef TT
#define TT this.ls
#endif
#define FLAG_1 (1LL<<0)
#define FLAG_x (1LL<<1)
#define FLAG_w (1LL<<2)
#define FLAG_u (1LL<<3)
#define FLAG_t (1LL<<4)
#define FLAG_s (1LL<<5)
#define FLAG_r (1LL<<6)
#define FLAG_q (1LL<<7)
#define FLAG_p (1LL<<8)
#define FLAG_n (1LL<<9)
#define FLAG_m (1LL<<10)
#define FLAG_l (1LL<<11)
#define FLAG_k (1LL<<12)
#define FLAG_i (1LL<<13)
#define FLAG_h (1LL<<14)
#define FLAG_f (1LL<<15)
#define FLAG_d (1LL<<16)
#define FLAG_c (1LL<<17)
#define FLAG_b (1LL<<18)
#define FLAG_a (1LL<<19)
#define FLAG_X (1LL<<20)
#define FLAG_U (1LL<<21)
#define FLAG_S (1LL<<22)
#define FLAG_R (1LL<<23)
#define FLAG_N (1LL<<24)
#define FLAG_L (1LL<<25)
#define FLAG_H (1LL<<26)
#define FLAG_F (1LL<<27)
#define FLAG_C (1LL<<28)
#define FLAG_A (1LL<<29)
#define FLAG_o (1LL<<30)
#define FLAG_g (1LL<<31)
#define FLAG_Z (1LL<<32)
#define FLAG_X7E (1LL<<33)
#define FLAG_X21 (1LL<<34)
#define FLAG_X7F (1LL<<35)
#define FLAG_show_control_chars (1LL<<36)
#define FLAG_full_time (1LL<<37)
#define FLAG_color (1LL<<38)
#define FLAG_sort (1LL<<39)
#endif

#ifdef FOR_mkdir
#define CLEANUP_mkdir
#ifndef TT
#define TT this.mkdir
#endif
#define FLAG_m (1LL<<0)
#define FLAG_p (1LL<<1)
#define FLAG_v (1LL<<2)
#define FLAG_Z (FORCED_FLAG<<3)
#endif

#ifdef FOR_od
#define CLEANUP_od
#ifndef TT
#define TT this.od
#endif
#define FLAG_t (1LL<<0)
#define FLAG_A (1LL<<1)
#define FLAG_b (1LL<<2)
#define FLAG_c (1LL<<3)
#define FLAG_d (1LL<<4)
#define FLAG_o (1LL<<5)
#define FLAG_s (1LL<<6)
#define FLAG_x (1LL<<7)
#define FLAG_N (1LL<<8)
#define FLAG_w (1LL<<9)
#define FLAG_v (1LL<<10)
#define FLAG_j (1LL<<11)
#endif

#ifdef FOR_readlink
#define CLEANUP_readlink
#ifndef TT
#define TT this.readlink
#endif
#define FLAG_z (1LL<<0)
#define FLAG_q (1LL<<1)
#define FLAG_m (1LL<<2)
#define FLAG_e (1LL<<3)
#define FLAG_f (1LL<<4)
#define FLAG_n (1LL<<5)
#define FLAG_v (1LL<<6)
#endif

#ifdef FOR_realpath
#define CLEANUP_realpath
#ifndef TT
#define TT this.realpath
#endif
#define FLAG_z (FORCED_FLAG<<0)
#define FLAG_q (FORCED_FLAG<<1)
#define FLAG_m (FORCED_FLAG<<2)
#define FLAG_e (FORCED_FLAG<<3)
#define FLAG_P (FORCED_FLAG<<4)
#define FLAG_L (FORCED_FLAG<<5)
#define FLAG_s (FORCED_FLAG<<6)
#define FLAG_R (FORCED_FLAG<<7)
#define FLAG_relative_base (FORCED_FLAG<<8)
#endif

#ifdef FOR_rm
#define CLEANUP_rm
#ifndef TT
#define TT this.rm
#endif
#define FLAG_v (1LL<<0)
#define FLAG_r (1LL<<1)
#define FLAG_R (1LL<<2)
#define FLAG_i (1LL<<3)
#define FLAG_f (1LL<<4)
#endif

#ifdef FOR_sed
#define CLEANUP_sed
#ifndef TT
#define TT this.sed
#endif
#define FLAG_s (1LL<<0)
#define FLAG_z (1LL<<1)
#define FLAG_r (1LL<<2)
#define FLAG_E (1LL<<3)
#define FLAG_n (1LL<<4)
#define FLAG_i (1LL<<5)
#define FLAG_f (1LL<<6)
#define FLAG_e (1LL<<7)
#define FLAG_tarxform (1LL<<8)
#define FLAG_version (1LL<<9)
#define FLAG_help (1LL<<10)
#endif

#ifdef FOR_sort
#define CLEANUP_sort
#ifndef TT
#define TT this.sort
#endif
#define FLAG_n (1LL<<0)
#define FLAG_u (1LL<<1)
#define FLAG_r (1LL<<2)
#define FLAG_i (1LL<<3)
#define FLAG_f (1LL<<4)
#define FLAG_d (1LL<<5)
#define FLAG_z (1LL<<6)
#define FLAG_s (1LL<<7)
#define FLAG_c (1LL<<8)
#define FLAG_C (1LL<<9)
#define FLAG_M (1LL<<10)
#define FLAG_b (1LL<<11)
#define FLAG_V (1LL<<12)
#define FLAG_x (1LL<<13)
#define FLAG_t (1LL<<14)
#define FLAG_k (1LL<<15)
#define FLAG_o (1LL<<16)
#define FLAG_m (1LL<<17)
#define FLAG_T (1LL<<18)
#define FLAG_S (1LL<<19)
#define FLAG_g (FORCED_FLAG<<20)
#endif

#ifdef FOR_tail
#define CLEANUP_tail
#ifndef TT
#define TT this.tail
#endif
#define FLAG_n (1LL<<0)
#define FLAG_c (1LL<<1)
#define FLAG_s (1LL<<2)
#define FLAG_F (1LL<<3)
#define FLAG_f (1LL<<4)
#endif

#ifdef FOR_tee
#define CLEANUP_tee
#ifndef TT
#define TT this.tee
#endif
#define FLAG_a (1LL<<0)
#define FLAG_i (1LL<<1)
#endif

#ifdef FOR_tr
#define CLEANUP_tr
#ifndef TT
#define TT this.tr
#endif
#define FLAG_d (1LL<<0)
#define FLAG_t (1LL<<1)
#define FLAG_s (1LL<<2)
#define FLAG_c (1LL<<3)
#define FLAG_C (1LL<<4)
#endif

#ifdef FOR_uname
#define CLEANUP_uname
#ifndef TT
#define TT this.uname
#endif
#define FLAG_s (1LL<<0)
#define FLAG_n (1LL<<1)
#define FLAG_r (1LL<<2)
#define FLAG_v (1LL<<3)
#define FLAG_m (1LL<<4)
#define FLAG_o (1LL<<5)
#define FLAG_a (1LL<<6)
#define FLAG_p (1LL<<7)
#endif

#ifdef FOR_wc
#define CLEANUP_wc
#ifndef TT
#define TT this.wc
#endif
#define FLAG_l (1LL<<0)
#define FLAG_w (1LL<<1)
#define FLAG_m (1LL<<2)
#define FLAG_c (1LL<<3)
#define FLAG_L (1LL<<4)
#endif

#ifdef FOR_xargs
#define CLEANUP_xargs
#ifndef TT
#define TT this.xargs
#endif
#define FLAG_0 (1LL<<0)
#define FLAG_s (1LL<<1)
#define FLAG_n (1LL<<2)
#define FLAG_r (1LL<<3)
#define FLAG_t (1LL<<4)
#define FLAG_p (1LL<<5)
#define FLAG_o (1LL<<6)
#define FLAG_P (1LL<<7)
#define FLAG_E (1LL<<8)
#endif

#undef OPTSTR_ascii
#define OPTSTR_ascii 0
#undef OPTSTR_dirname
#define OPTSTR_dirname "<1"
#undef OPTSTR_gitcheckout
#define OPTSTR_gitcheckout "<1"
#undef OPTSTR_gitclone
#define OPTSTR_gitclone "<1"
#undef OPTSTR_gitfetch
#define OPTSTR_gitfetch 0
#undef OPTSTR_gitinit
#define OPTSTR_gitinit "<1"
#undef OPTSTR_gitremote
#define OPTSTR_gitremote "<1"
#undef OPTSTR_makedevs
#define OPTSTR_makedevs "<1>1d:"
#undef OPTSTR_toybox
#define OPTSTR_toybox 0
#undef OPTSTR_which
#define OPTSTR_which "<1a"
