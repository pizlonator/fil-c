/*
 * $Id$
 *
 * Copyright (c) 1999 Andrew G. Morgan <morgan@ftp.kernel.org>
 *
 * pamc_load
 */

#include "libpamc.h"
#include "pam_inline.h"

static int __pamc_exec_agent(pamc_handle_t pch, pamc_agent_t *agent)
{
    char *full_path = NULL;
    int found_agent, length, to_agent[2], from_agent[2];
    int return_code = PAM_BPC_FAIL;

    if (agent->id[agent->id_length] != '\0') {
	PAM_BP_ASSERT("libpamc: internal error agent_id not terminated");
    }

    for (length=0; (length < agent->id_length); ++length) {
	switch (agent->id[length]) {
	case '/':
	    D(("ill formed agent id"));
	    return PAM_BPC_FAIL;
	}
    }

    found_agent = 0;
    for (length=0; pch->agent_paths[length]; ++length) {
	struct stat buf;

	D(("path: [%s]", pch->agent_paths[length]));
	D(("agent id: [%s]", agent->id));

	if ((full_path = pam_asprintf("%s/%s", pch->agent_paths[length], agent->id)) == NULL) {
	    D(("no memory for agent path"));
	    return PAM_BPC_FAIL;
	}

	D(("looking for agent here: [%s]\n", full_path));
	if (stat(full_path, &buf) == 0) {
	    D(("file existis"));
	    found_agent = 1;
	    break;
	}

	free(full_path);
	full_path = NULL;
    }

    if (! found_agent) {
	D(("no agent was found"));
	goto free_and_return;
    }

    if (pipe(to_agent)) {
	D(("failed to open pipe to agent"));
	goto free_and_return;
    }

    if (pipe(from_agent)) {
	D(("failed to open pipe from agent"));
	goto close_the_agent;
    }

    agent->pid = fork();
    if (agent->pid == -1) {

	D(("failed to fork for agent"));
	goto close_both_pipes;

    } else if (agent->pid == 0) {

	int i;

	dup2(from_agent[1], STDOUT_FILENO);
	dup2(to_agent[0], STDIN_FILENO);

	/* we close all of the files that have filedescriptors lower
	   and equal to twice the highest we have seen, The idea is
	   that we don't want to leak filedescriptors to agents from a
	   privileged client application.

	   XXX - this is a heuristic at this point. There is a growing
	    need for an extra 'set param' libpamc function, that could
	    be used to supply info like the highest fd to close etc..
	*/

	if (from_agent[1] > pch->highest_fd_to_close) {
	    pch->highest_fd_to_close = 2*from_agent[1];
	}

	for (i=0; i <= pch->highest_fd_to_close; ++i) {
	    switch (i) {
	    case STDOUT_FILENO:
	    case STDERR_FILENO:
	    case STDIN_FILENO:
		/* only these three remain open */
		break;
	    default:
		(void) close(i); /* don't care if its not open */
	    }
	}

	/* we make no attempt to drop other privileges - this library
	   has no idea how that would be done in the general case. It
	   is up to the client application (when calling
	   pamc_converse) to make sure no privilege will leak into an
	   (untrusted) agent. */

	/* we propagate no environment - future versions of this
           library may have the ability to audit all agent
           transactions. */

	D(("exec'ing agent %s", full_path));
	execle(full_path, "pam-agent", NULL, NULL);

	D(("exec failed"));
	_exit(1);

    }

    close(to_agent[0]);
    close(from_agent[1]);

    agent->writer = to_agent[1];
    agent->reader = from_agent[0];

    return_code = PAM_BPC_TRUE;
    goto free_and_return;

close_both_pipes:
    close(from_agent[0]);
    close(from_agent[1]);

close_the_agent:
    close(to_agent[0]);
    close(to_agent[1]);

free_and_return:
    free(full_path);

    D(("returning %d", return_code));

    return return_code;
}

/*
 * has the named agent been loaded?
 */

static int __pamc_agent_is_enabled(pamc_handle_t pch, const char *agent_id)
{
    pamc_agent_t *agent;

    for (agent = pch->chain; agent; agent = agent->next) {
	if (!strcmp(agent->id, agent_id)) {
	    D(("agent already loaded"));
	    return PAM_BPC_TRUE;
	}
    }

    D(("agent is not loaded"));
    return PAM_BPC_FALSE;
}

/*
 * has the named agent been disabled?
 */

static int __pamc_agent_is_disabled(pamc_handle_t pch, const char *agent_id)
{
    pamc_blocked_t *blocked;

    for (blocked=pch->blocked_agents; blocked; blocked = blocked->next) {
	if (!strcmp(agent_id, blocked->id)) {
	    D(("agent is disabled"));
	    return PAM_BPC_TRUE;
	}
    }

    D(("agent is not disabled"));
    return PAM_BPC_FALSE;
}

/*
 * disable an agent
 */

int pamc_disable(pamc_handle_t pch, const char *agent_id)
{
    pamc_blocked_t *block;

    if (pch == NULL) {
	D(("pch is NULL"));
	return PAM_BPC_FALSE;
    }

    if (agent_id == NULL) {
	D(("agent_id is NULL"));
	return PAM_BPC_FALSE;
    }

    if (__pamc_agent_is_enabled(pch, agent_id) != PAM_BPC_FALSE) {
	D(("agent is already loaded"));
	return PAM_BPC_FALSE;
    }

    if (__pamc_agent_is_disabled(pch, agent_id) != PAM_BPC_FALSE) {
	D(("agent is already disabled"));
	return PAM_BPC_TRUE;
    }

    block = calloc(1, sizeof(pamc_blocked_t));
    if (block == NULL) {
	D(("no memory for new blocking structure"));
	return PAM_BPC_FALSE;
    }

    block->id =	strdup(agent_id);
    if (block->id == NULL) {
	D(("no memory for agent id"));
	free(block);
	return PAM_BPC_FALSE;
    }

    block->next = pch->blocked_agents;
    pch->blocked_agents = block;

    return PAM_BPC_TRUE;
}

/*
 * force the loading of a particular agent
 */

int pamc_load(pamc_handle_t pch, const char *agent_id)
{
    pamc_agent_t *agent;
    size_t length;

    /* sanity checking */

    if (pch == NULL) {
	D(("pch is NULL"));
	return PAM_BPC_FALSE;
    }

    if (agent_id == NULL) {
	D(("agent_id is NULL"));
	return PAM_BPC_FALSE;
    }

    if (__pamc_agent_is_disabled(pch, agent_id) != PAM_BPC_FALSE) {
	D(("sorry agent is disabled"));
	return PAM_BPC_FALSE;
    }

    length = strlen(agent_id);

    /* scan list to see if agent is loaded */

    if (__pamc_agent_is_enabled(pch, agent_id) == PAM_BPC_TRUE) {
	D(("no need to load an already loaded agent (%s)", agent_id));
	return PAM_BPC_TRUE;
    }

    /* not in the list, so we need to load it and add it to the head
       of the chain */

    agent = calloc(1, sizeof(pamc_agent_t));
    if (agent == NULL) {
	D(("no memory for new agent"));
	return PAM_BPC_FALSE;
    }
    agent->id = calloc(1, 1+length);
    if (agent->id == NULL) {
	D(("no memory for new agent's id"));
	goto fail_free_agent;
    }
    memcpy(agent->id, agent_id, length);
    agent->id[length] = '\0';
    agent->id_length = length;

    if (__pamc_exec_agent(pch, agent) != PAM_BPC_TRUE) {
	D(("unable to exec agent"));
	goto fail_free_agent_id;
    }

    agent->next = pch->chain;
    pch->chain = agent;

    return PAM_BPC_TRUE;

fail_free_agent_id:

    pam_overwrite_n(agent->id, agent->id_length);
    free(agent->id);

    pam_overwrite_object(agent);

fail_free_agent:

    free(agent);
    return PAM_BPC_FALSE;
}

/*
 * what's a valid agent name?
 */

int __pamc_valid_agent_id(int id_length, const char *id)
{
    int post, i;

    for (i=post=0 ; i < id_length; ++i) {
	int ch = id[i++];

	if (isalpha((unsigned char)ch) || isdigit((unsigned char)ch) || (ch == '_')) {
	    continue;
	} else if (post && (ch == '.')) {
	    continue;
	} else if ((i > 1) && (!post) && (ch == '@')) {
	    post = 1;
	} else {
	    D(("id=%s contains '%c' which is illegal", id, ch));
	    return 0;
	}
    }

    if (!i) {
	D(("length of id is 0"));
	return 0;
    } else {
	return 1;                       /* id is valid */
    }
}

/*
 * building a tree of available agent names
 */

static pamc_id_node_t *__pamc_add_node(pamc_id_node_t *root, const char *id,
				       int *counter)
{
    if (root) {

	int cmp;

	if ((cmp = strcmp(id, root->agent_id))) {
	    if (cmp > 0) {
		root->right = __pamc_add_node(root->right, id,
					      &(root->child_count));
	    } else {
		root->left = __pamc_add_node(root->left, id,
					     &(root->child_count));
	    }
	}

	return root;

    } else {

	pamc_id_node_t *node = calloc(1, sizeof(pamc_id_node_t));

	if (node) {
	    node->agent_id = strdup(id);
	    if (node->agent_id == NULL) {
		free(node);
		node = NULL;
	    }
	}

	(*counter)++;
	return node;
    }
}

/*
 * drop all of the tree and any remaining ids
 */

static pamc_id_node_t *__pamc_liberate_nodes(pamc_id_node_t *tree)
{
    if (tree) {
	free(tree->agent_id);
	tree->agent_id = NULL;

	tree->left = __pamc_liberate_nodes(tree->left);
	tree->right = __pamc_liberate_nodes(tree->right);

	tree->child_count = 0;
	free(tree);
    }

    return NULL;
}

/*
 * fill a list with the contents of the tree (in ascii order)
 */

static void __pamc_fill_list_from_tree(pamc_id_node_t *tree, char **agent_list,
				       int *counter)
{
    if (tree) {
	__pamc_fill_list_from_tree(tree->left, agent_list, counter);
	agent_list[(*counter)++] = tree->agent_id;
	tree->agent_id = NULL;
	__pamc_fill_list_from_tree(tree->right, agent_list, counter);
    }
}

/*
 * get a list of the available agents
 */

char **pamc_list_agents(pamc_handle_t pch)
{
    int i, total_agent_count=0;
    pamc_id_node_t *tree = NULL;
    char **agent_list;

    /* loop over agent paths */

    for (i=0; pch->agent_paths[i]; ++i) {
	DIR *dir;

	dir = opendir(pch->agent_paths[i]);
	if (dir) {
	    struct dirent *item;

	    while ((item = readdir(dir))) {

		/* this is a cheat on recognizing agent_ids */
		if (!__pamc_valid_agent_id(strlen(item->d_name),
					   item->d_name)) {
		    continue;
		}

		tree = __pamc_add_node(tree, item->d_name, &total_agent_count);
	    }

	    closedir(dir);
	}
    }

    /* now, we build a list of ids */
    D(("total of %d available agents\n", total_agent_count));

    agent_list = calloc(total_agent_count+1, sizeof(char *));
    if (agent_list) {
	int counter=0;

	__pamc_fill_list_from_tree(tree, agent_list, &counter);
	if (counter != total_agent_count) {
	    PAM_BP_ASSERT("libpamc: internal error transcribing tree");
	}
    } else {
	D(("no memory for agent list"));
    }

    __pamc_liberate_nodes(tree);

    return agent_list;
}
