/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * soup-hsts-enforcer.c: HTTP Strict Transport Security enforcer session feature
 *
 * Copyright (C) 2016, 2017, 2018 Igalia S.L.
 * Copyright (C) 2017, 2018 Metrological Group B.V.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "soup-hsts-enforcer.h"
#include "soup-misc.h"
#include "soup.h"
#include "soup-session-private.h"
#include "soup-session-feature-private.h"
#include "soup-message-private.h"
#include "soup-uri-utils-private.h"
#include <stdfil.h>

/**
 * SoupHSTSEnforcer:
 *
 * Automatic HTTP Strict Transport Security enforcing for [class@Session].
 *
 * A #SoupHSTSEnforcer stores HSTS policies and enforces them when
 * required. #SoupHSTSEnforcer implements [iface@SessionFeature], so you
 * can add an HSTS enforcer to a session with
 * [method@Session.add_feature] or [method@Session.add_feature_by_type].
 *
 * #SoupHSTSEnforcer keeps track of all the HTTPS destinations that,
 * when connected to, return the Strict-Transport-Security header with
 * valid values. #SoupHSTSEnforcer will forget those destinations
 * upon expiry or when the server requests it.
 *
 * When the [class@Session] the #SoupHSTSEnforcer is attached to queues or
 * restarts a message, the #SoupHSTSEnforcer will rewrite the URI to HTTPS if
 * the destination is a known HSTS host and is contacted over an insecure
 * transport protocol (HTTP). Users of #SoupHSTSEnforcer are advised to listen
 * to changes in the [property@Message:uri] property in order to be aware of
 * changes in the message URI.
 *
 * Note that #SoupHSTSEnforcer does not support any form of long-term
 * HSTS policy persistence. See [class@HSTSEnforcerDB] for a persistent
 * enforcer.
 **/

static void soup_hsts_enforcer_session_feature_init (SoupSessionFeatureInterface *feature_interface, gpointer interface_data);

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
	SoupSession *session;
        GMutex mutex;
	GHashTable *host_policies;
	GHashTable *session_policies;
} SoupHSTSEnforcerPrivate;

G_DEFINE_TYPE_WITH_CODE (SoupHSTSEnforcer, soup_hsts_enforcer, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (SOUP_TYPE_SESSION_FEATURE,
						soup_hsts_enforcer_session_feature_init)
			 G_ADD_PRIVATE(SoupHSTSEnforcer))

static void
soup_hsts_enforcer_init (SoupHSTSEnforcer *hsts_enforcer)
{
	SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private (hsts_enforcer);

	priv->host_policies = g_hash_table_new_full (soup_str_case_hash,
								    soup_str_case_equal,
								    g_free, NULL);

	priv->session_policies = g_hash_table_new_full (soup_str_case_hash,
								       soup_str_case_equal,
								       g_free, NULL);
        g_mutex_init (&priv->mutex);
}

static void
soup_hsts_enforcer_finalize (GObject *object)
{
	SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private ((SoupHSTSEnforcer*)object);
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, priv->host_policies);
	while (g_hash_table_iter_next (&iter, &key, &value))
		soup_hsts_policy_free (value);
	g_hash_table_destroy (priv->host_policies);

	g_hash_table_iter_init (&iter, priv->session_policies);
	while (g_hash_table_iter_next (&iter, &key, &value))
		soup_hsts_policy_free (value);
	g_hash_table_destroy (priv->session_policies);

        g_mutex_clear (&priv->mutex);

	G_OBJECT_CLASS (soup_hsts_enforcer_parent_class)->finalize (object);
}

static gboolean
soup_hsts_enforcer_real_is_persistent (SoupHSTSEnforcer *hsts_enforcer)
{
	return FALSE;
}

static SoupHSTSPolicy *
soup_hsts_enforcer_get_host_policy (SoupHSTSEnforcer *hsts_enforcer,
				    const char *domain)
{
        SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private (hsts_enforcer);
	return g_hash_table_lookup (priv->host_policies, domain);
}

static SoupHSTSPolicy *
soup_hsts_enforcer_get_session_policy (SoupHSTSEnforcer *hsts_enforcer,
				       const char *domain)
{
        SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private (hsts_enforcer);
	return g_hash_table_lookup (priv->session_policies, domain);
}

static gboolean
soup_hsts_enforcer_real_has_valid_policy (SoupHSTSEnforcer *hsts_enforcer,
					  const char *domain)
{
	SoupHSTSPolicy *policy;

	if (soup_hsts_enforcer_get_session_policy (hsts_enforcer, domain))
		return TRUE;

	policy = soup_hsts_enforcer_get_host_policy (hsts_enforcer, domain);
	if (policy)
		return !soup_hsts_policy_is_expired (policy);

	return FALSE;
}

static void
soup_hsts_enforcer_class_init (SoupHSTSEnforcerClass *hsts_enforcer_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (hsts_enforcer_class);

	object_class->finalize = soup_hsts_enforcer_finalize;

	hsts_enforcer_class->is_persistent = soup_hsts_enforcer_real_is_persistent;
	hsts_enforcer_class->has_valid_policy = soup_hsts_enforcer_real_has_valid_policy;

	/**
	 * SoupHSTSEnforcer::changed:
	 * @hsts_enforcer: the #SoupHSTSEnforcer
	 * @old_policy: the old #SoupHSTSPolicy value
	 * @new_policy: the new #SoupHSTSPolicy value
	 *
	 * Emitted when @hsts_enforcer changes.
	 *
	 * If a policy has been added,
	 * @new_policy will contain the newly-added policy and
	 * @old_policy will be %NULL. If a policy has been deleted,
	 * @old_policy will contain the to-be-deleted policy and
	 * @new_policy will be %NULL. If a policy has been changed,
	 * @old_policy will contain its old value, and @new_policy its
	 * new value.
	 *
	 * Note that you shouldn't modify the policies from a callback to
	 * this signal.
	 **/
	signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (SoupHSTSEnforcerClass, changed),
			      NULL, NULL,
			      NULL,
			      G_TYPE_NONE, 2,
			      zorptr (SOUP_TYPE_HSTS_POLICY, (uintptr_t) G_SIGNAL_TYPE_STATIC_SCOPE),
			      zorptr (SOUP_TYPE_HSTS_POLICY, (uintptr_t) G_SIGNAL_TYPE_STATIC_SCOPE));
}

/**
 * soup_hsts_enforcer_new:
 *
 * Creates a new #SoupHSTSEnforcer.
 *
 * The base #SoupHSTSEnforcer class does not support persistent storage of HSTS
 * policies, see [class@HSTSEnforcerDB] for that.
 *
 * Returns: a new #SoupHSTSEnforcer
 **/
SoupHSTSEnforcer *
soup_hsts_enforcer_new (void)
{
	return g_object_new (SOUP_TYPE_HSTS_ENFORCER, NULL);
}

static void
soup_hsts_enforcer_changed (SoupHSTSEnforcer *hsts_enforcer,
			    SoupHSTSPolicy *old, SoupHSTSPolicy *new)
{
	g_assert (old || new);

	g_signal_emit (hsts_enforcer, signals[CHANGED], 0, old, new);
}

static gboolean
should_remove_expired_host_policy (G_GNUC_UNUSED gpointer key,
				   SoupHSTSPolicy *policy,
				   SoupHSTSEnforcer *enforcer)
{
	if (soup_hsts_policy_is_expired (policy)) {
		/* This will emit the ::changed signal before the
		   policy is actually removed from the policies hash
		   table, which could be problematic, or not.
		*/
		soup_hsts_enforcer_changed (enforcer, policy, NULL);
		soup_hsts_policy_free (policy);

		return TRUE;
	}

	return FALSE;
}

static void
remove_expired_host_policies (SoupHSTSEnforcer *hsts_enforcer)
{
        SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private (hsts_enforcer);
	g_hash_table_foreach_remove (priv->host_policies,
				     (GHRFunc)should_remove_expired_host_policy,
				     hsts_enforcer);
}

static void
soup_hsts_enforcer_remove_host_policy (SoupHSTSEnforcer *hsts_enforcer,
				       const char *domain)
{
        SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private (hsts_enforcer);
	SoupHSTSPolicy *policy;

	policy = g_hash_table_lookup (priv->host_policies, domain);

	if (!policy)
		return;

	g_hash_table_remove (priv->host_policies, domain);
	soup_hsts_enforcer_changed (hsts_enforcer, policy, NULL);
	soup_hsts_policy_free (policy);

	remove_expired_host_policies (hsts_enforcer);
}

static void
soup_hsts_enforcer_replace_policy (SoupHSTSEnforcer *hsts_enforcer,
				   SoupHSTSPolicy *new_policy)
{
        SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private (hsts_enforcer);
	GHashTable *policies;
	SoupHSTSPolicy *old_policy;
	const char *domain;
	gboolean is_session_policy;

	g_assert (!soup_hsts_policy_is_expired (new_policy));

	domain = soup_hsts_policy_get_domain (new_policy);
	is_session_policy = soup_hsts_policy_is_session_policy (new_policy);

	policies = is_session_policy ? priv->session_policies :
		                       priv->host_policies;

	old_policy = g_hash_table_lookup (policies, domain);
	g_assert (old_policy);

	g_hash_table_replace (policies, g_strdup (domain), soup_hsts_policy_copy (new_policy));
	if (!soup_hsts_policy_equal (old_policy, new_policy))
		soup_hsts_enforcer_changed (hsts_enforcer, old_policy, new_policy);
	soup_hsts_policy_free (old_policy);

	remove_expired_host_policies (hsts_enforcer);
}

static void
soup_hsts_enforcer_insert_policy (SoupHSTSEnforcer *hsts_enforcer,
				  SoupHSTSPolicy *policy)
{
        SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private (hsts_enforcer);
	GHashTable *policies;
	const char *domain;
	gboolean is_session_policy;

	g_return_if_fail (SOUP_IS_HSTS_ENFORCER (hsts_enforcer));
	g_return_if_fail (policy != NULL);

	g_assert (!soup_hsts_policy_is_expired (policy));

	domain = soup_hsts_policy_get_domain (policy);
	is_session_policy = soup_hsts_policy_is_session_policy (policy);

	g_return_if_fail (domain != NULL);

	policies = is_session_policy ? priv->session_policies :
				  priv->host_policies;

	g_assert (!g_hash_table_contains (policies, domain));

	g_hash_table_insert (policies, g_strdup (domain), soup_hsts_policy_copy (policy));
	soup_hsts_enforcer_changed (hsts_enforcer, NULL, policy);
}

/**
 * soup_hsts_enforcer_set_policy:
 * @hsts_enforcer: a #SoupHSTSEnforcer
 * @policy: (transfer none): the policy of the HSTS host
 *
 * Sets @policy to @hsts_enforcer.
 *
 * If @policy is expired, any existing HSTS policy for its host will be removed
 * instead. If a policy existed for this host, it will be replaced. Otherwise,
 * the new policy will be inserted. If the policy is a session policy, that is,
 * one created with [ctor@HSTSPolicy.new_session_policy], the policy will not
 * expire and will be enforced during the lifetime of @hsts_enforcer's
 * [class@Session].
 **/
void
soup_hsts_enforcer_set_policy (SoupHSTSEnforcer *hsts_enforcer,
			       SoupHSTSPolicy *policy)
{
        SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private (hsts_enforcer);
	GHashTable *policies;
	const char *domain;
	gboolean is_session_policy;
	SoupHSTSPolicy *current_policy;

	g_return_if_fail (SOUP_IS_HSTS_ENFORCER (hsts_enforcer));
	g_return_if_fail (policy != NULL);

	domain = soup_hsts_policy_get_domain (policy);
	g_return_if_fail (domain != NULL);

        g_mutex_lock (&priv->mutex);

	is_session_policy = soup_hsts_policy_is_session_policy (policy);
	if (!is_session_policy && soup_hsts_policy_is_expired (policy)) {
		soup_hsts_enforcer_remove_host_policy (hsts_enforcer, domain);
                g_mutex_unlock (&priv->mutex);
		return;
	}

        policies = is_session_policy ? priv->session_policies : priv->host_policies;
	current_policy = g_hash_table_lookup (policies, domain);

	if (current_policy)
		soup_hsts_enforcer_replace_policy (hsts_enforcer, policy);
	else
		soup_hsts_enforcer_insert_policy (hsts_enforcer, policy);

        g_mutex_unlock (&priv->mutex);
}

/**
 * soup_hsts_enforcer_set_session_policy:
 * @hsts_enforcer: a #SoupHSTSEnforcer
 * @domain: policy domain or hostname
 * @include_subdomains: %TRUE if the policy applies on sub domains
 *
 * Sets a session policy for @domain.
 *
 * A session policy is a policy that is permanent to the lifetime of
 * @hsts_enforcer's [class@Session] and doesn't expire.
 **/
void
soup_hsts_enforcer_set_session_policy (SoupHSTSEnforcer *hsts_enforcer,
				       const char *domain,
				       gboolean include_subdomains)
{
	SoupHSTSPolicy *policy;

	g_return_if_fail (SOUP_IS_HSTS_ENFORCER (hsts_enforcer));
	g_return_if_fail (domain != NULL);

	policy = soup_hsts_policy_new_session_policy (domain, include_subdomains);
	soup_hsts_enforcer_set_policy (hsts_enforcer, policy);
	soup_hsts_policy_free (policy);
}

static gboolean
soup_hsts_enforcer_host_includes_subdomains (SoupHSTSEnforcer *hsts_enforcer,
					     const char *domain)
{
	SoupHSTSPolicy *policy;
	gboolean include_subdomains = FALSE;

	g_return_val_if_fail (SOUP_IS_HSTS_ENFORCER (hsts_enforcer), FALSE);
	g_return_val_if_fail (domain != NULL, FALSE);

	policy = soup_hsts_enforcer_get_session_policy (hsts_enforcer, domain);
	if (policy)
		include_subdomains |= soup_hsts_policy_includes_subdomains (policy);

	policy = soup_hsts_enforcer_get_host_policy (hsts_enforcer, domain);
	if (policy)
		include_subdomains |= soup_hsts_policy_includes_subdomains (policy);

	return include_subdomains;
}

static inline const char *
super_domain_of (const char *domain)
{
	const char *iter = domain;

	g_assert (domain);

	for (; *iter != '\0' && *iter != '.' ; iter++);
	for (; *iter == '.' ; iter++);

	if (*iter == '\0')
		return NULL;

	return iter;
}

static gboolean
soup_hsts_enforcer_must_enforce_secure_transport (SoupHSTSEnforcer *hsts_enforcer,
						  const char *domain)
{
        SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private (hsts_enforcer);
	const char *super_domain = domain;

	g_return_val_if_fail (domain != NULL, FALSE);

        g_mutex_lock (&priv->mutex);

	if (soup_hsts_enforcer_has_valid_policy (hsts_enforcer, domain)) {
                g_mutex_unlock (&priv->mutex);
		return TRUE;
        }

	while ((super_domain = super_domain_of (super_domain)) != NULL) {
		if (soup_hsts_enforcer_host_includes_subdomains (hsts_enforcer, super_domain) &&
		    soup_hsts_enforcer_has_valid_policy (hsts_enforcer, super_domain)) {
                        g_mutex_unlock (&priv->mutex);
			return TRUE;
                }
	}

        g_mutex_unlock (&priv->mutex);

	return FALSE;
}

static void
soup_hsts_enforcer_process_sts_header (SoupHSTSEnforcer *hsts_enforcer,
				       SoupMessage *msg)
{
	SoupHSTSPolicy *policy;
	GUri *uri;

	uri = soup_message_get_uri (msg);

	g_return_if_fail (uri != NULL);

	policy = soup_hsts_policy_new_from_response (msg);
	if (policy) {
		soup_hsts_enforcer_set_policy (hsts_enforcer, policy);
		soup_hsts_policy_free (policy);
	}
}

static void
got_sts_header_cb (SoupMessage *msg, gpointer user_data)
{
	SoupHSTSEnforcer *hsts_enforcer = SOUP_HSTS_ENFORCER (user_data);

	soup_hsts_enforcer_process_sts_header (hsts_enforcer, msg);
}

static void
rewrite_message_uri_to_https (SoupMessage *msg)
{
	GUri *uri, *new_uri;
	int port;

	uri = soup_message_get_uri (msg);
	port = g_uri_get_port (uri);
	/* From the RFC: "If the URI contains an explicit port component that
	   is not equal to "80", the port component value MUST be preserved;" */
	if (port == 80)
                port = 443;

        new_uri = soup_uri_copy (uri, SOUP_URI_SCHEME, "https", SOUP_URI_PORT, port, SOUP_URI_NONE);
	soup_message_set_uri (msg, new_uri);
	g_uri_unref (new_uri);
}

static void
on_sts_known_host_message_starting (SoupMessage *msg, SoupHSTSEnforcer *hsts_enforcer)
{
        SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private (hsts_enforcer);
	GTlsCertificateFlags errors;

	/* THE UA MUST terminate the connection if there are
	   any errors with the underlying secure transport for STS
	   known hosts. */

	errors = soup_message_get_tls_peer_certificate_errors (msg);
	if (errors)
		soup_session_cancel_message (priv->session, msg);
}

static void
preprocess_request (SoupHSTSEnforcer *enforcer, SoupMessage *msg)
{
	GUri *uri;
	const char *host;
	char *canonicalized = NULL;

	uri = soup_message_get_uri (msg);
	host = g_uri_get_host (uri);

	if (g_hostname_is_ip_address (host))
		return;

	if (soup_uri_is_http (uri)) {
		if (g_hostname_is_ascii_encoded (host)) {
			canonicalized = g_hostname_to_unicode (host);
			if (!canonicalized)
				return;
		}
		if (soup_hsts_enforcer_must_enforce_secure_transport (enforcer, canonicalized? canonicalized : host)) {
			rewrite_message_uri_to_https (msg);
			g_signal_connect (msg, "starting",
					  G_CALLBACK (on_sts_known_host_message_starting),
					  enforcer);
			soup_message_hsts_enforced (msg);
		}
		g_free (canonicalized);
	} else if (soup_uri_is_https (uri)) {
		soup_message_add_header_handler (msg, "got-headers",
						 "Strict-Transport-Security",
						 G_CALLBACK (got_sts_header_cb),
						 enforcer);
	}
}

static void
message_restarted_cb (SoupMessage *msg, gpointer user_data)
{
	preprocess_request (SOUP_HSTS_ENFORCER (user_data), msg);

}

static void
soup_hsts_enforcer_attach (SoupSessionFeature *feature, SoupSession *session)
{
        SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private (SOUP_HSTS_ENFORCER (feature));
	priv->session = session;
}

static void
soup_hsts_enforcer_request_queued (SoupSessionFeature *feature,
				   SoupMessage        *msg)
{
	g_signal_connect (msg, "restarted", G_CALLBACK (message_restarted_cb), feature);
	preprocess_request (SOUP_HSTS_ENFORCER (feature), msg);
}

static void
soup_hsts_enforcer_request_unqueued (SoupSessionFeature *feature,
				     SoupMessage        *msg)
{
	g_signal_handlers_disconnect_by_data (msg, feature);
}

static void
soup_hsts_enforcer_session_feature_init (SoupSessionFeatureInterface *feature_interface,
					 gpointer interface_data)
{
	feature_interface->attach = soup_hsts_enforcer_attach;
	feature_interface->request_queued = soup_hsts_enforcer_request_queued;
	feature_interface->request_unqueued = soup_hsts_enforcer_request_unqueued;
}

/**
 * soup_hsts_enforcer_is_persistent:
 * @hsts_enforcer: a #SoupHSTSEnforcer
 *
 * Gets whether @hsts_enforcer stores policies persistenly.
 *
 * Returns: %TRUE if @hsts_enforcer storage is persistent or %FALSE otherwise.
 **/
gboolean
soup_hsts_enforcer_is_persistent (SoupHSTSEnforcer *hsts_enforcer)
{
	g_return_val_if_fail (SOUP_IS_HSTS_ENFORCER (hsts_enforcer), FALSE);

	return SOUP_HSTS_ENFORCER_GET_CLASS (hsts_enforcer)->is_persistent (hsts_enforcer);
}

/**
 * soup_hsts_enforcer_has_valid_policy:
 * @hsts_enforcer: a #SoupHSTSEnforcer
 * @domain: a domain.
 *
 * Gets whether @hsts_enforcer has a currently valid policy for @domain.
 *
 * Returns: %TRUE if access to @domain should happen over HTTPS, false
 *   otherwise.
 **/
gboolean
soup_hsts_enforcer_has_valid_policy (SoupHSTSEnforcer *hsts_enforcer,
				     const char *domain)
{
	char *canonicalized = NULL;
	gboolean retval;

	g_return_val_if_fail (SOUP_IS_HSTS_ENFORCER (hsts_enforcer), FALSE);
	g_return_val_if_fail (domain != NULL, FALSE);

	if (g_hostname_is_ascii_encoded (domain)) {
		canonicalized = g_hostname_to_unicode (domain);
		g_return_val_if_fail (canonicalized, FALSE);
	}

	retval = SOUP_HSTS_ENFORCER_GET_CLASS (hsts_enforcer)->has_valid_policy (hsts_enforcer,
										 canonicalized ? canonicalized : domain);

	g_free (canonicalized);

	return retval;
}

static void
add_domain_to_list (gpointer key,
		    gpointer value,
		    gpointer data)
{
	GList **domains = (GList **) data;
	*domains = g_list_prepend (*domains, g_strdup ((char*)key));
}

/**
 * soup_hsts_enforcer_get_domains:
 * @hsts_enforcer: a #SoupHSTSEnforcer
 * @session_policies: whether to include session policies
 *
 * Gets a list of domains for which there are policies in @enforcer.
 *
 * Returns: (element-type utf8) (transfer full): a newly allocated
 *   list of domains. Use [func@GLib.List.free_full] and [func@GLib.free] to free the
 *   list.
 **/
GList*
soup_hsts_enforcer_get_domains (SoupHSTSEnforcer *hsts_enforcer,
				gboolean          session_policies)
{
        SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private (hsts_enforcer);
	GList *domains = NULL;

	g_return_val_if_fail (SOUP_IS_HSTS_ENFORCER (hsts_enforcer), NULL);

	g_hash_table_foreach (priv->host_policies, add_domain_to_list, &domains);
	if (session_policies)
		g_hash_table_foreach (priv->session_policies, add_domain_to_list, &domains);

	return domains;
}

static void
add_policy_to_list (gpointer key,
		    gpointer value,
		    gpointer data)
{
	GList **policies = (GList **) data;
	*policies = g_list_prepend (*policies, soup_hsts_policy_copy ((SoupHSTSPolicy*)value));
}

/**
 * soup_hsts_enforcer_get_policies:
 * @hsts_enforcer: a #SoupHSTSEnforcer
 * @session_policies: whether to include session policies
 *
 * Gets a list with the policies in @enforcer.
 *
 * Returns: (element-type SoupHSTSPolicy) (transfer full): a newly
 *   allocated list of policies. Use [func@GLib.List.free_full] and
 *   [method@HSTSPolicy.free] to free the list.
 **/
GList*
soup_hsts_enforcer_get_policies (SoupHSTSEnforcer *hsts_enforcer,
				 gboolean          session_policies)
{
        SoupHSTSEnforcerPrivate *priv = soup_hsts_enforcer_get_instance_private (hsts_enforcer);
	GList *policies = NULL;

	g_return_val_if_fail (SOUP_IS_HSTS_ENFORCER (hsts_enforcer), NULL);

	g_hash_table_foreach (priv->host_policies, add_policy_to_list, &policies);
	if (session_policies)
		g_hash_table_foreach (priv->session_policies, add_policy_to_list, &policies);

	return policies;
}
