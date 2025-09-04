#ifndef _REGEX_H
#include <posix/regex.h>

#ifndef _ISOMAC
/* Document internal interfaces.  */
extern reg_syntax_t __re_set_syntax (reg_syntax_t syntax);

extern const char *__re_compile_pattern (const char *pattern, size_t length,
					 struct re_pattern_buffer *buffer);

extern int __re_compile_fastmap (struct re_pattern_buffer *buffer)
     attribute_hidden;

extern regoff_t __re_search (struct re_pattern_buffer *buffer, const char *string,
			regoff_t length, regoff_t start, regoff_t range,
			struct re_registers *regs);

extern regoff_t __re_search_2
  (struct re_pattern_buffer *buffer, const char *string1,
   regoff_t length1, const char *string2, regoff_t length2,
   regoff_t start, regoff_t range, struct re_registers *regs, regoff_t stop);

extern regoff_t __re_match
  (struct re_pattern_buffer *buffer, const char *string,
   regoff_t length, regoff_t start, struct re_registers *regs);

extern regoff_t __re_match_2
  (struct re_pattern_buffer *buffer, const char *string1,
   regoff_t length1, const char *string2, regoff_t length2,
   regoff_t start, struct re_registers *regs, regoff_t stop);

extern void __re_set_registers
  (struct re_pattern_buffer *buffer, struct re_registers *regs,
   __re_size_t num_regs, regoff_t *starts, regoff_t *ends);

extern int __regcomp (regex_t *__preg, const char *__pattern, int __cflags);
libc_hidden_proto (__regcomp)

extern int __regexec (const regex_t *__preg, const char *__string,
		      size_t __nmatch, regmatch_t __pmatch[__nmatch],
		      int __eflags);
libc_hidden_proto (__regexec)

extern size_t __regerror (int __errcode, const regex_t *__preg,
			  char *__errbuf, size_t __errbuf_size);

extern void __regfree (regex_t *__preg);
libc_hidden_proto (__regfree)
#endif
#endif
