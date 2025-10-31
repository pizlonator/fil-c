#include <sepol/debug.h>
#include <sepol/kernel_to_cil.h>
#include <sepol/kernel_to_conf.h>
#include <sepol/policydb/expand.h>
#include <sepol/policydb/hierarchy.h>
#include <sepol/policydb/link.h>
#include <sepol/policydb/policydb.h>

extern int policydb_validate(sepol_handle_t *handle, const policydb_t *p);

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);


// set to 1 to enable more verbose libsepol logging
#define VERBOSE 0


static int write_binary_policy(policydb_t *p, FILE *outfp)
{
	struct policy_file pf;

	policy_file_init(&pf);
	pf.type = PF_USE_STDIO;
	pf.fp = outfp;
	return policydb_write(p, &pf);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	policydb_t policydb = {}, out = {};
	sidtab_t sidtab = {};
	struct policy_file pf;
	FILE *devnull = NULL;

	sepol_debug(VERBOSE);

	policy_file_init(&pf);
	pf.type = PF_USE_MEMORY;
	pf.data = (char *) data;
	pf.len = size;

	if (policydb_init(&policydb))
		goto exit;

	if (policydb_read(&policydb, &pf, VERBOSE))
		goto exit;

	if (policydb_load_isids(&policydb, &sidtab))
		goto exit;

	if (policydb.policy_type == POLICY_KERN) {
		(void) policydb_optimize(&policydb);

		if (policydb_validate(NULL, &policydb) == -1)
			abort();
	}

	if (policydb.global->branch_list)
		(void) check_assertions(NULL, &policydb, policydb.global->branch_list->avrules);

	(void) hierarchy_check_constraints(NULL, &policydb);

	devnull = fopen("/dev/null", "we");
	if (!devnull)
		goto exit;

	if (write_binary_policy(&policydb, devnull))
		abort();

	if (policydb.policy_type == POLICY_KERN) {
		if (sepol_kernel_policydb_to_conf(devnull, &policydb))
			abort();

		if (sepol_kernel_policydb_to_cil(devnull, &policydb))
			abort();

	} else if (policydb.policy_type == POLICY_BASE) {
		if (link_modules(NULL, &policydb, NULL, 0, VERBOSE))
			goto exit;

		if (policydb_init(&out))
			goto exit;

		if (expand_module(NULL, &policydb, &out, VERBOSE, /*check_assertions=*/0))
			goto exit;

		(void) check_assertions(NULL, &out, out.global->branch_list->avrules);
		(void) hierarchy_check_constraints(NULL, &out);

		if (write_binary_policy(&out, devnull))
			abort();

		if (sepol_kernel_policydb_to_conf(devnull, &out))
			abort();

		if (sepol_kernel_policydb_to_cil(devnull, &out))
			abort();

	}

exit:
	if (devnull != NULL)
		fclose(devnull);

	policydb_destroy(&out);
	policydb_destroy(&policydb);
	sepol_sidtab_destroy(&sidtab);

	/* Non-zero return values are reserved for future use. */
	return 0;
}
