#define _GNU_SOURCE  /* vasprintf(3) */

#include "test-neverallow.h"

#include "helpers.h"
#include "test-common.h"

#include <sepol/debug.h>
#include <sepol/policydb/link.h>
#include <sepol/policydb/expand.h>

#include <stdio.h>
#include <stdarg.h>

extern int mls;

int neverallow_test_init(void)
{
	return 0;
}

int neverallow_test_cleanup(void)
{
	return 0;
}

static struct msg_list {
	char *msg;
	struct msg_list *next;
} *messages;

static void messages_clean(void)
{
	while (messages) {
		struct msg_list *n = messages->next;
		free(messages->msg);
		free(messages);
		messages = n;
	}
}

static void messages_check(unsigned count, const char *const expected[count])
{
	unsigned i;
	const struct msg_list *m = messages;

	for (i = 0; i < count; i++, m = m->next) {
		if (!m) {
			CU_FAIL("less messages than expected");
			fprintf(stderr, "\n<expected %u, got %u>\n", count, i);
			return;
		}

		if (strcmp(expected[i], m->msg) != 0) {
			CU_FAIL("messages differ from expected");
			fprintf(stderr, "\n<expected: '''%s''', got: '''%s'''>\n", expected[i], m->msg);
		}
	}

	if (m) {
		CU_FAIL("more messages than expected");
		fprintf(stderr, "\n<expected %u; next message: '''%s'''>\n", count, m->msg);
	}
}

__attribute__ ((format(printf, 3, 4)))
static void msg_handler(void *varg __attribute__ ((unused)),
			sepol_handle_t * handle __attribute__ ((unused)),
			const char *fmt, ...)
{
	char *msg;
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vasprintf(&msg, fmt, ap);
	if (r < 0)
		CU_FAIL_FATAL("oom");
	va_end(ap);

	struct msg_list *new = malloc(sizeof(*new));
	if (!new)
		CU_FAIL_FATAL("oom");
	new->msg = msg;
	new->next = messages;
	messages = new;
}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*a))

static void test_neverallow_basic(void)
{
	policydb_t basemod, base_expanded;
	sepol_handle_t *handle;
	static const char *const expected_messages[] = {
		"30 neverallow failures occurred",
		"neverallow on line 53 of policies/test-neverallow/policy.conf.std (or line 53 of policies/test-neverallow/policy.conf.std) violated by allow test1_t test1_t:file { read };",
		"neverallow on line 60 of policies/test-neverallow/policy.conf.std (or line 60 of policies/test-neverallow/policy.conf.std) violated by allow test2_t test2_t:file { read write };",
		"neverallow on line 67 of policies/test-neverallow/policy.conf.std (or line 67 of policies/test-neverallow/policy.conf.std) violated by allow test3_t test3_t:file { read };",
		"neverallow on line 74 of policies/test-neverallow/policy.conf.std (or line 74 of policies/test-neverallow/policy.conf.std) violated by allow test4_t test4_t:file { read };",
		"neverallow on line 81 of policies/test-neverallow/policy.conf.std (or line 81 of policies/test-neverallow/policy.conf.std) violated by allow test5_t test5_t:file { read };",
		"neverallow on line 89 of policies/test-neverallow/policy.conf.std (or line 89 of policies/test-neverallow/policy.conf.std) violated by allow test6_1_t test6_1_t:file { read };",
		"neverallow on line 97 of policies/test-neverallow/policy.conf.std (or line 97 of policies/test-neverallow/policy.conf.std) violated by allow test7_1_t test7_1_t:file { read };",
		"neverallow on line 106 of policies/test-neverallow/policy.conf.std (or line 106 of policies/test-neverallow/policy.conf.std) violated by allow test8_t test8_t:file { read };",
		"neverallow on line 106 of policies/test-neverallow/policy.conf.std (or line 106 of policies/test-neverallow/policy.conf.std) violated by allow test8_t test8_t:file { write };",
		"neverallow on line 115 of policies/test-neverallow/policy.conf.std (or line 115 of policies/test-neverallow/policy.conf.std) violated by allow test9_t test9_t:file { write };",
		"neverallow on line 115 of policies/test-neverallow/policy.conf.std (or line 115 of policies/test-neverallow/policy.conf.std) violated by allow test9_t test9_t:file { read };",
		"neverallow on line 124 of policies/test-neverallow/policy.conf.std (or line 124 of policies/test-neverallow/policy.conf.std) violated by allow test10_1_t test10_1_t:file { read };",
		"neverallow on line 131 of policies/test-neverallow/policy.conf.std (or line 131 of policies/test-neverallow/policy.conf.std) violated by allow test11_t test11_t:process { dyntransition transition };",
		"neverallow on line 143 of policies/test-neverallow/policy.conf.std (or line 143 of policies/test-neverallow/policy.conf.std) violated by allow test12_3_t test12_1_t:file { getattr };",
		"neverallow on line 143 of policies/test-neverallow/policy.conf.std (or line 143 of policies/test-neverallow/policy.conf.std) violated by allow test12_3_t test12_2_t:file { getattr };",
		"neverallow on line 144 of policies/test-neverallow/policy.conf.std (or line 144 of policies/test-neverallow/policy.conf.std) violated by allow test12_3_t test12_1_t:file { open };",
		"neverallow on line 144 of policies/test-neverallow/policy.conf.std (or line 144 of policies/test-neverallow/policy.conf.std) violated by allow test12_2_t test12_1_t:file { open };",
		"neverallow on line 156 of policies/test-neverallow/policy.conf.std (or line 156 of policies/test-neverallow/policy.conf.std) violated by allow test13_1_t test13_1_t:file { read };",
		"neverallowxperm on line 174 of policies/test-neverallow/policy.conf.std (or line 174 of policies/test-neverallow/policy.conf.std) violated by\n  allow test15_t test15_t:file { ioctl };",
		"neverallowxperm on line 182 of policies/test-neverallow/policy.conf.std (or line 182 of policies/test-neverallow/policy.conf.std) violated by\n  allowxperm test16_t test16_t:file ioctl { 0x1111 };",
		"neverallowxperm on line 198 of policies/test-neverallow/policy.conf.std (or line 198 of policies/test-neverallow/policy.conf.std) violated by\n  allowxperm test18_t test18_t:file ioctl { 0x1111 };",
		"neverallowxperm on line 206 of policies/test-neverallow/policy.conf.std (or line 206 of policies/test-neverallow/policy.conf.std) violated by\n  allowxperm test19_t test19_t:file ioctl { 0x1111 };",
		"neverallowxperm on line 216 of policies/test-neverallow/policy.conf.std (or line 216 of policies/test-neverallow/policy.conf.std) violated by\n  allowxperm test20_a test20_a:file ioctl { 0x1111 };",
		"neverallowxperm on line 227 of policies/test-neverallow/policy.conf.std (or line 227 of policies/test-neverallow/policy.conf.std) violated by\n  allowxperm test21_1_a test21_2_a:file ioctl { 0x1111 };",
		"neverallowxperm on line 237 of policies/test-neverallow/policy.conf.std (or line 237 of policies/test-neverallow/policy.conf.std) violated by\n  allowxperm test22_t test22_t:file ioctl { 0x1111 };",
		"neverallowxperm on line 247 of policies/test-neverallow/policy.conf.std (or line 247 of policies/test-neverallow/policy.conf.std) violated by\n  allowxperm test23_t test23_t:file ioctl { 0x1111 };",
		"neverallowxperm on line 257 of policies/test-neverallow/policy.conf.std (or line 257 of policies/test-neverallow/policy.conf.std) violated by\n  allowxperm test24_t test24_a:file ioctl { 0x1111 };",
		"neverallowxperm on line 267 of policies/test-neverallow/policy.conf.std (or line 267 of policies/test-neverallow/policy.conf.std) violated by\n  allowxperm test25_t test25_t:file ioctl { 0x1111 };",
		"neverallowxperm on line 277 of policies/test-neverallow/policy.conf.std (or line 277 of policies/test-neverallow/policy.conf.std) violated by\n  allowxperm test26_a test26_a:file ioctl { 0x1111 };",
		"neverallowxperm on line 277 of policies/test-neverallow/policy.conf.std (or line 277 of policies/test-neverallow/policy.conf.std) violated by\n  allowxperm test26_a test26_a:file ioctl { 0x1111 };",
	};

	if (policydb_init(&base_expanded))
		CU_FAIL_FATAL("Failed to initialize policy");

	if (test_load_policy(&basemod, POLICY_BASE, mls, "test-neverallow", "policy.conf"))
		CU_FAIL_FATAL("Failed to load policy");

	if (link_modules(NULL, &basemod, NULL, 0, 0))
		CU_FAIL_FATAL("Failed to link base module");

	if (expand_module(NULL, &basemod, &base_expanded, 0, 0))
		CU_FAIL_FATAL("Failed to expand policy");

	if ((handle = sepol_handle_create()) == NULL)
		CU_FAIL_FATAL("Failed to initialize handle");

	sepol_msg_set_callback(handle, msg_handler, NULL);

	if (check_assertions(handle, &base_expanded, base_expanded.global->branch_list->avrules) != -1)
		CU_FAIL("Assertions did not trigger");

	messages_check(ARRAY_SIZE(expected_messages), expected_messages);

	sepol_handle_destroy(handle);
	messages_clean();
	policydb_destroy(&basemod);
	policydb_destroy(&base_expanded);
}

static void test_neverallow_minus_self(void)
{
	policydb_t basemod, base_expanded;
	sepol_handle_t *handle;
	static const char *const expected_messages[] = {
		"33 neverallow failures occurred",
		"neverallow on line 77 of policies/test-neverallow/policy_minus_self.conf.std (or line 77 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test3_1_t test3_2_t:file { read };",
		"neverallow on line 85 of policies/test-neverallow/policy_minus_self.conf.std (or line 85 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test4_1_t test4_2_t:file { read };",
		"neverallow on line 93 of policies/test-neverallow/policy_minus_self.conf.std (or line 93 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test5_2_t test5_1_t:class5 { perm };",
		"neverallow on line 93 of policies/test-neverallow/policy_minus_self.conf.std (or line 93 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test5_1_t test5_2_t:class5 { perm };",
		"neverallow on line 101 of policies/test-neverallow/policy_minus_self.conf.std (or line 101 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test6_1_t test6_2_t:class6 { perm };",
		"neverallow on line 118 of policies/test-neverallow/policy_minus_self.conf.std (or line 118 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test8_1_t test8_2_t:file { read };",
		"neverallow on line 127 of policies/test-neverallow/policy_minus_self.conf.std (or line 127 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test9_1_t test9_2_t:file { read };",
		"neverallow on line 137 of policies/test-neverallow/policy_minus_self.conf.std (or line 137 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test10_1_t test10_2_t:file { read };",
		"neverallow on line 157 of policies/test-neverallow/policy_minus_self.conf.std (or line 157 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test12_1_t test12_2_t:file { read };",
		"neverallow on line 166 of policies/test-neverallow/policy_minus_self.conf.std (or line 166 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test13_1_t test13_2_t:file { read };",
		"neverallow on line 175 of policies/test-neverallow/policy_minus_self.conf.std (or line 175 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test14_2_t test14_1_t:file { read };",
		"neverallow on line 175 of policies/test-neverallow/policy_minus_self.conf.std (or line 175 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test14_1_t test14_2_t:file { read };",
		"neverallow on line 193 of policies/test-neverallow/policy_minus_self.conf.std (or line 193 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test16_2_t test16_1_t:file { read };",
		"neverallow on line 193 of policies/test-neverallow/policy_minus_self.conf.std (or line 193 of policies/test-neverallow/policy_minus_self.conf.std) violated by allow test16_1_t test16_2_t:file { read };",
		"neverallowxperm on line 201 of policies/test-neverallow/policy_minus_self.conf.std (or line 201 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allow test17_1_t test17_2_t:class17 { ioctl };",
		"neverallowxperm on line 219 of policies/test-neverallow/policy_minus_self.conf.std (or line 219 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test19_2_t test19_1_t:file ioctl { 0x101-0x102 };",
		"neverallowxperm on line 231 of policies/test-neverallow/policy_minus_self.conf.std (or line 231 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test20_2_t test20_1_t:file ioctl { 0x103 };",
		"neverallowxperm on line 231 of policies/test-neverallow/policy_minus_self.conf.std (or line 231 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test20_1_t test20_2_t:file ioctl { 0x102 };",
		"neverallowxperm on line 261 of policies/test-neverallow/policy_minus_self.conf.std (or line 261 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test23_1_t test23_2_t:file ioctl { 0x9511 };",
		"neverallowxperm on line 272 of policies/test-neverallow/policy_minus_self.conf.std (or line 272 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test24_1_t test24_a:file ioctl { 0x9511 };",
		"neverallowxperm on line 283 of policies/test-neverallow/policy_minus_self.conf.std (or line 283 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test25_a test25_a:file ioctl { 0x9511 };",
		"neverallowxperm on line 295 of policies/test-neverallow/policy_minus_self.conf.std (or line 295 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test26_1_a test26_2_a:file ioctl { 0x9511 };",
		"neverallowxperm on line 295 of policies/test-neverallow/policy_minus_self.conf.std (or line 295 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test26_1_a test26_2_a:file ioctl { 0x9511 };",
		"neverallowxperm on line 295 of policies/test-neverallow/policy_minus_self.conf.std (or line 295 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test26_1_a test26_2_a:file ioctl { 0x9511 };",
		"neverallowxperm on line 295 of policies/test-neverallow/policy_minus_self.conf.std (or line 295 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test26_1_a test26_2_a:file ioctl { 0x9511 };",
		"neverallowxperm on line 317 of policies/test-neverallow/policy_minus_self.conf.std (or line 317 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allow test28_2_t test28_1_t:file { ioctl };",
		"neverallowxperm on line 317 of policies/test-neverallow/policy_minus_self.conf.std (or line 317 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test28_1_t test28_2_t:file ioctl { 0x9521 };",
		"neverallowxperm on line 327 of policies/test-neverallow/policy_minus_self.conf.std (or line 327 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allow test29_2_t test29_1_t:file { ioctl };",
		"neverallowxperm on line 327 of policies/test-neverallow/policy_minus_self.conf.std (or line 327 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test29_1_t test29_a:file ioctl { 0x9521 };",
		"neverallowxperm on line 337 of policies/test-neverallow/policy_minus_self.conf.std (or line 337 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test30_a test30_a:file ioctl { 0x9521 };",
		"neverallowxperm on line 337 of policies/test-neverallow/policy_minus_self.conf.std (or line 337 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test30_a test30_a:file ioctl { 0x9521 };",
		"neverallowxperm on line 348 of policies/test-neverallow/policy_minus_self.conf.std (or line 348 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test31_1_a test31_2_a:file ioctl { 0x9521 };",
		"neverallowxperm on line 348 of policies/test-neverallow/policy_minus_self.conf.std (or line 348 of policies/test-neverallow/policy_minus_self.conf.std) violated by\n  allowxperm test31_1_a test31_2_a:file ioctl { 0x9521 };",
	};

	if (policydb_init(&base_expanded))
		CU_FAIL_FATAL("Failed to initialize policy");

	if (test_load_policy(&basemod, POLICY_BASE, mls, "test-neverallow", "policy_minus_self.conf"))
		CU_FAIL_FATAL("Failed to load policy");

	if (link_modules(NULL, &basemod, NULL, 0, 0))
		CU_FAIL_FATAL("Failed to link base module");

	if (expand_module(NULL, &basemod, &base_expanded, 0, 0))
		CU_FAIL_FATAL("Failed to expand policy");

	if ((handle = sepol_handle_create()) == NULL)
		CU_FAIL_FATAL("Failed to initialize handle");

	sepol_msg_set_callback(handle, msg_handler, NULL);

	if (check_assertions(handle, &base_expanded, base_expanded.global->branch_list->avrules) != -1)
		CU_FAIL("Assertions did not trigger");

	messages_check(ARRAY_SIZE(expected_messages), expected_messages);

	sepol_handle_destroy(handle);
	messages_clean();
	policydb_destroy(&basemod);
	policydb_destroy(&base_expanded);
}

static void test_neverallow_not_self(void)
{
	policydb_t basemod, base_expanded;
	sepol_handle_t *handle;
	static const char *const expected_messages[] = {
		"34 neverallow failures occurred",
		"neverallow on line 78 of policies/test-neverallow/policy_not_self.conf.std (or line 78 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test3_1_t test3_2_t:file { read };",
		"neverallow on line 86 of policies/test-neverallow/policy_not_self.conf.std (or line 86 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test4_1_t test4_2_t:file { read };",
		"neverallow on line 94 of policies/test-neverallow/policy_not_self.conf.std (or line 94 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test5_2_t test5_1_t:class5 { perm };",
		"neverallow on line 94 of policies/test-neverallow/policy_not_self.conf.std (or line 94 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test5_1_t test5_2_t:class5 { perm };",
		"neverallow on line 102 of policies/test-neverallow/policy_not_self.conf.std (or line 102 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test6_1_t test6_2_t:class6 { perm };",
		"neverallow on line 119 of policies/test-neverallow/policy_not_self.conf.std (or line 119 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test8_1_t test8_2_t:file { read };",
		"neverallow on line 128 of policies/test-neverallow/policy_not_self.conf.std (or line 128 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test9_1_t test9_2_t:file { read };",
		"neverallow on line 138 of policies/test-neverallow/policy_not_self.conf.std (or line 138 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test10_1_t test10_2_t:file { read };",
		"neverallow on line 158 of policies/test-neverallow/policy_not_self.conf.std (or line 158 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test12_1_t test12_2_t:file { read };",
		"neverallow on line 167 of policies/test-neverallow/policy_not_self.conf.std (or line 167 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test13_1_t test13_2_t:file { read };",
		"neverallow on line 176 of policies/test-neverallow/policy_not_self.conf.std (or line 176 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test14_2_t test14_1_t:file { read };",
		"neverallow on line 176 of policies/test-neverallow/policy_not_self.conf.std (or line 176 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test14_1_t test14_2_t:file { read };",
		"neverallow on line 185 of policies/test-neverallow/policy_not_self.conf.std (or line 185 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test13_1_t test13_2_t:file { read };",
		"neverallow on line 194 of policies/test-neverallow/policy_not_self.conf.std (or line 194 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test16_2_t test16_1_t:file { read };",
		"neverallow on line 194 of policies/test-neverallow/policy_not_self.conf.std (or line 194 of policies/test-neverallow/policy_not_self.conf.std) violated by allow test16_1_t test16_2_t:file { read };",
		"neverallowxperm on line 202 of policies/test-neverallow/policy_not_self.conf.std (or line 202 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allow test17_1_t test17_2_t:class17 { ioctl };",
		"neverallowxperm on line 220 of policies/test-neverallow/policy_not_self.conf.std (or line 220 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test19_2_t test19_1_t:file ioctl { 0x101-0x102 };",
		"neverallowxperm on line 232 of policies/test-neverallow/policy_not_self.conf.std (or line 232 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test20_2_t test20_1_t:file ioctl { 0x103 };",
		"neverallowxperm on line 232 of policies/test-neverallow/policy_not_self.conf.std (or line 232 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test20_1_t test20_2_t:file ioctl { 0x102 };",
		"neverallowxperm on line 262 of policies/test-neverallow/policy_not_self.conf.std (or line 262 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test23_1_t test23_2_t:file ioctl { 0x9511 };",
		"neverallowxperm on line 273 of policies/test-neverallow/policy_not_self.conf.std (or line 273 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test24_1_t test24_a:file ioctl { 0x9511 };",
		"neverallowxperm on line 284 of policies/test-neverallow/policy_not_self.conf.std (or line 284 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test25_a test25_a:file ioctl { 0x9511 };",
		"neverallowxperm on line 296 of policies/test-neverallow/policy_not_self.conf.std (or line 296 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test26_1_a test26_2_a:file ioctl { 0x9511 };",
		"neverallowxperm on line 296 of policies/test-neverallow/policy_not_self.conf.std (or line 296 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test26_1_a test26_2_a:file ioctl { 0x9511 };",
		"neverallowxperm on line 296 of policies/test-neverallow/policy_not_self.conf.std (or line 296 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test26_1_a test26_2_a:file ioctl { 0x9511 };",
		"neverallowxperm on line 296 of policies/test-neverallow/policy_not_self.conf.std (or line 296 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test26_1_a test26_2_a:file ioctl { 0x9511 };",
		"neverallowxperm on line 318 of policies/test-neverallow/policy_not_self.conf.std (or line 318 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allow test28_2_t test28_1_t:file { ioctl };",
		"neverallowxperm on line 318 of policies/test-neverallow/policy_not_self.conf.std (or line 318 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test28_1_t test28_2_t:file ioctl { 0x9521 };",
		"neverallowxperm on line 328 of policies/test-neverallow/policy_not_self.conf.std (or line 328 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allow test29_2_t test29_1_t:file { ioctl };",
		"neverallowxperm on line 328 of policies/test-neverallow/policy_not_self.conf.std (or line 328 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test29_1_t test29_a:file ioctl { 0x9521 };",
		"neverallowxperm on line 338 of policies/test-neverallow/policy_not_self.conf.std (or line 338 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test30_a test30_a:file ioctl { 0x9521 };",
		"neverallowxperm on line 338 of policies/test-neverallow/policy_not_self.conf.std (or line 338 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test30_a test30_a:file ioctl { 0x9521 };",
		"neverallowxperm on line 349 of policies/test-neverallow/policy_not_self.conf.std (or line 349 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test31_1_a test31_2_a:file ioctl { 0x9521 };",
		"neverallowxperm on line 349 of policies/test-neverallow/policy_not_self.conf.std (or line 349 of policies/test-neverallow/policy_not_self.conf.std) violated by\n  allowxperm test31_1_a test31_2_a:file ioctl { 0x9521 };",
	};

	if (policydb_init(&base_expanded))
		CU_FAIL_FATAL("Failed to initialize policy");

	if (test_load_policy(&basemod, POLICY_BASE, mls, "test-neverallow", "policy_not_self.conf"))
		CU_FAIL_FATAL("Failed to load policy");

	if (link_modules(NULL, &basemod, NULL, 0, 0))
		CU_FAIL_FATAL("Failed to link base module");

	if (expand_module(NULL, &basemod, &base_expanded, 0, 0))
		CU_FAIL_FATAL("Failed to expand policy");

	if ((handle = sepol_handle_create()) == NULL)
		CU_FAIL_FATAL("Failed to initialize handle");

	sepol_msg_set_callback(handle, msg_handler, NULL);

	if (check_assertions(handle, &base_expanded, base_expanded.global->branch_list->avrules) != -1)
		CU_FAIL("Assertions did not trigger");

	messages_check(ARRAY_SIZE(expected_messages), expected_messages);

	sepol_handle_destroy(handle);
	messages_clean();
	policydb_destroy(&basemod);
	policydb_destroy(&base_expanded);
}

static void test_neverallow_cond(void)
{
	policydb_t basemod, base_expanded;
	sepol_handle_t *handle;
	static const char *const expected_messages[] = {
		"16 neverallow failures occurred",
		"neverallow on line 58 of policies/test-neverallow/policy_cond.conf.std (or line 58 of policies/test-neverallow/policy_cond.conf.std) violated by allow test1_t test1_t:file { read };",
		"neverallow on line 70 of policies/test-neverallow/policy_cond.conf.std (or line 70 of policies/test-neverallow/policy_cond.conf.std) violated by allow test2_1_t test2_1_t:file { write };",
		"neverallowxperm on line 81 of policies/test-neverallow/policy_cond.conf.std (or line 81 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allow test3_t test3_t:file { ioctl };",
		"neverallowxperm on line 93 of policies/test-neverallow/policy_cond.conf.std (or line 93 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allow test4_t test4_t:file { ioctl };",
		"neverallowxperm on line 117 of policies/test-neverallow/policy_cond.conf.std (or line 117 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allowxperm test6_t test6_t:file ioctl { 0x1 };",
		"neverallowxperm on line 130 of policies/test-neverallow/policy_cond.conf.std (or line 130 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allowxperm test7_t test7_t:file ioctl { 0x2 };",
		"neverallowxperm on line 130 of policies/test-neverallow/policy_cond.conf.std (or line 130 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allowxperm test7_t test7_t:file ioctl { 0x1 };",
		"neverallowxperm on line 130 of policies/test-neverallow/policy_cond.conf.std (or line 130 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allowxperm test7_t test7_t:file ioctl { 0x2 };",
		"neverallowxperm on line 130 of policies/test-neverallow/policy_cond.conf.std (or line 130 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allowxperm test7_t test7_t:file ioctl { 0x1 };",
		"neverallowxperm on line 155 of policies/test-neverallow/policy_cond.conf.std (or line 155 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allow test9_t test9_t:file { ioctl };",
		"neverallowxperm on line 191 of policies/test-neverallow/policy_cond.conf.std (or line 191 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allow test12_t test12_t:file { ioctl };",
		"neverallowxperm on line 204 of policies/test-neverallow/policy_cond.conf.std (or line 204 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allowxperm test13_t test13_t:file ioctl { 0x1 };",
		"neverallowxperm on line 204 of policies/test-neverallow/policy_cond.conf.std (or line 204 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allow test13_t test13_t:file { ioctl };",
		"neverallowxperm on line 204 of policies/test-neverallow/policy_cond.conf.std (or line 204 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allowxperm test13_t test13_t:file ioctl { 0x1 };",
		"neverallowxperm on line 217 of policies/test-neverallow/policy_cond.conf.std (or line 217 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allow test14_t test14_t:file { ioctl };",
		"neverallowxperm on line 230 of policies/test-neverallow/policy_cond.conf.std (or line 230 of policies/test-neverallow/policy_cond.conf.std) violated by\n  allow test15_t test15_t:file { ioctl };",
	};

	if (policydb_init(&base_expanded))
		CU_FAIL_FATAL("Failed to initialize policy");

	if (test_load_policy(&basemod, POLICY_BASE, mls, "test-neverallow", "policy_cond.conf"))
		CU_FAIL_FATAL("Failed to load policy");

	if (link_modules(NULL, &basemod, NULL, 0, 0))
		CU_FAIL_FATAL("Failed to link base module");

	if (expand_module(NULL, &basemod, &base_expanded, 0, 0))
		CU_FAIL_FATAL("Failed to expand policy");

	if ((handle = sepol_handle_create()) == NULL)
		CU_FAIL_FATAL("Failed to initialize handle");

	sepol_msg_set_callback(handle, msg_handler, NULL);

	if (check_assertions(handle, &base_expanded, base_expanded.global->branch_list->avrules) != -1)
		CU_FAIL("Assertions did not trigger");

	messages_check(ARRAY_SIZE(expected_messages), expected_messages);

	sepol_handle_destroy(handle);
	messages_clean();
	policydb_destroy(&basemod);
	policydb_destroy(&base_expanded);
}

int neverallow_add_tests(CU_pSuite suite)
{
	/*
	 * neverallow rules operate only on types and are unaffected by MLS
	 * (avoid adjusting the messages for std and mls)
	 */
	if (mls)
		return 0;

	if (NULL == CU_add_test(suite, "neverallow_basic", test_neverallow_basic)) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	if (NULL == CU_add_test(suite, "neverallow_not_self", test_neverallow_not_self)) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	if (NULL == CU_add_test(suite, "neverallow_minus_self", test_neverallow_minus_self)) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	if (NULL == CU_add_test(suite, "neverallow_cond", test_neverallow_cond)) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	return 0;
}
