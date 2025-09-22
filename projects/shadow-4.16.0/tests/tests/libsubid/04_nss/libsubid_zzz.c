#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <subid.h>
#include <string.h>
#include "alloc.h"

enum subid_status shadow_subid_has_any_range(const char *owner, enum subid_type t, bool *result)
{
	if (strcmp(owner, "ubuntu") == 0) {
		*result = true;
		return SUBID_STATUS_SUCCESS;
	}
	if (strcmp(owner, "error") == 0) {
		*result = false;
		return SUBID_STATUS_ERROR;
	}
	if (strcmp(owner, "unknown") == 0) {
		*result = false;
		return SUBID_STATUS_UNKNOWN_USER;
	}
	if (strcmp(owner, "conn") == 0) {
		*result = false;
		return SUBID_STATUS_ERROR_CONN;
	}
	if (t == ID_TYPE_UID) {
		*result = strcmp(owner, "user1") == 0;
		return SUBID_STATUS_SUCCESS;
	}

	*result = strcmp(owner, "group1") == 0;
	return SUBID_STATUS_SUCCESS;
}

enum subid_status shadow_subid_has_range(const char *owner, unsigned long start, unsigned long count, enum subid_type t, bool *result)
{
	if (strcmp(owner, "ubuntu") == 0 &&
	    start >= 200000 &&
	    count <= 100000) {
		*result = true;
		return SUBID_STATUS_SUCCESS;
	}
	*result = false;
	if (strcmp(owner, "error") == 0)
		return SUBID_STATUS_ERROR;
	if (strcmp(owner, "unknown") == 0)
		return SUBID_STATUS_UNKNOWN_USER;
	if (strcmp(owner, "conn") == 0)
		return SUBID_STATUS_ERROR_CONN;

	if (t == ID_TYPE_UID && strcmp(owner, "user1") != 0)
		return SUBID_STATUS_SUCCESS;
	if (t == ID_TYPE_GID && strcmp(owner, "group1") != 0)
		return SUBID_STATUS_SUCCESS;

	if (start < 100000)
		return SUBID_STATUS_SUCCESS;
	if (count >= 65536)
		return SUBID_STATUS_SUCCESS;
	*result = true;
	return SUBID_STATUS_SUCCESS;
}

// So if 'user1' or 'ubuntu' is defined in passwd, we'll return those values,
// to ease manual testing.  For automated testing, if you return those values,
// we'll return 1000 for ubuntu and 1001 otherwise.
static uid_t getnamuid(const char *name) {
	struct passwd *pw;

	pw = getpwnam(name);
	if (pw)
		return pw->pw_uid;

	// For testing purposes
	return strcmp(name, "ubuntu") == 0 ? (uid_t)1000 : (uid_t)1001;
}

static int alloc_uid(uid_t **uids, uid_t id) {
	*uids = MALLOC(1, uid_t);
	if (!*uids)
		return -1;
	*uids[0] = id;
	return 1;
}

enum subid_status shadow_subid_find_subid_owners(unsigned long id, enum subid_type id_type, uid_t **uids, int *count)
{
	if (id >= 100000 && id < 165536) {
		*count = alloc_uid(uids, getnamuid("user1"));
		if (*count == 1)
			return SUBID_STATUS_SUCCESS;
		return SUBID_STATUS_ERROR; // out of memory
	}
	if (id >= 200000 && id < 300000) {
		*count = alloc_uid(uids, getnamuid("ubuntu"));
		if (*count == 1)
			return SUBID_STATUS_SUCCESS;
		return SUBID_STATUS_ERROR; // out of memory
	}
	*count = 0; // nothing found
	return SUBID_STATUS_SUCCESS;
}

enum subid_status shadow_subid_list_owner_ranges(const char *owner, enum subid_type id_type, struct subid_range **in_ranges, int *count)
{
	struct subid_range *ranges;

	*count = 0;
	if (strcmp(owner, "error") == 0)
		return SUBID_STATUS_ERROR;
	if (strcmp(owner, "unknown") == 0)
		return SUBID_STATUS_UNKNOWN_USER;
	if (strcmp(owner, "conn") == 0)
		return SUBID_STATUS_ERROR_CONN;

	*in_ranges = NULL;
	if (strcmp(owner, "user1") != 0 && strcmp(owner, "ubuntu") != 0 &&
	    strcmp(owner, "group1") != 0)
		return SUBID_STATUS_SUCCESS;
	if (id_type == ID_TYPE_GID && strcmp(owner, "user1") == 0)
		return SUBID_STATUS_SUCCESS;
	if (id_type == ID_TYPE_UID && strcmp(owner, "group1") == 0)
		return SUBID_STATUS_SUCCESS;
	ranges = MALLOC(1, struct subid_range);
	if (!ranges)
		return SUBID_STATUS_ERROR;
	if (strcmp(owner, "user1") == 0 || strcmp(owner, "group1") == 0) {
		ranges[0].start = 100000;
		ranges[0].count = 65536;
	} else {
		ranges[0].start = 200000;
		ranges[0].count = 100000;
	}

	*count = 1;
	*in_ranges = ranges;

	return SUBID_STATUS_SUCCESS;
}

void shadow_subid_free(void *ptr)
{
	free(ptr);
}
