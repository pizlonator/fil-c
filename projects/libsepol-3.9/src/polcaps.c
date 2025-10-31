/*
 * Policy capability support functions
 */

#include <string.h>
#include <sepol/policydb/polcaps.h>

static const char * const polcap_names[POLICYDB_CAP_MAX + 1] = {
	[POLICYDB_CAP_NETPEER]				= "network_peer_controls",
	[POLICYDB_CAP_OPENPERM]				= "open_perms",
	[POLICYDB_CAP_EXTSOCKCLASS]			= "extended_socket_class",
	[POLICYDB_CAP_ALWAYSNETWORK]			= "always_check_network",
	[POLICYDB_CAP_CGROUPSECLABEL]			= "cgroup_seclabel",
	[POLICYDB_CAP_NNP_NOSUID_TRANSITION]		= "nnp_nosuid_transition",
	[POLICYDB_CAP_GENFS_SECLABEL_SYMLINKS]		= "genfs_seclabel_symlinks",
	[POLICYDB_CAP_IOCTL_SKIP_CLOEXEC]		= "ioctl_skip_cloexec",
	[POLICYDB_CAP_USERSPACE_INITIAL_CONTEXT]	= "userspace_initial_context",
	[POLICYDB_CAP_NETLINK_XPERM]			= "netlink_xperm",
	[POLICYDB_CAP_NETIF_WILDCARD]			= "netif_wildcard",
	[POLICYDB_CAP_GENFS_SECLABEL_WILDCARD]		= "genfs_seclabel_wildcard",
};

int sepol_polcap_getnum(const char *name)
{
	int capnum;

	for (capnum = 0; capnum <= POLICYDB_CAP_MAX; capnum++) {
		if (polcap_names[capnum] == NULL)
			continue;
		if (strcasecmp(polcap_names[capnum], name) == 0)
			return capnum;
	}
	return -1;
}

const char *sepol_polcap_getname(unsigned int capnum)
{
	if (capnum > POLICYDB_CAP_MAX)
		return NULL;

	return polcap_names[capnum];
}
