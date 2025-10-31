#include <stdint.h>

#include <sepol/handle.h>
#include <sepol/policydb/policydb.h>

int value_isvalid(uint32_t value, uint32_t nprim);
int policydb_validate(sepol_handle_t *handle, const policydb_t *p);
