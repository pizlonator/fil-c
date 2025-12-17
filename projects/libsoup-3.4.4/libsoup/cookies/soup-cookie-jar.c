/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * soup-cookie-jar.c
 *
 * Copyright (C) 2008 Red Hat, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "soup-cookie-jar.h"
#include "soup-date-utils-private.h"
#include "soup-message-private.h"
#include "soup-message-headers-private.h"
#include "soup-misc.h"
#include "soup.h"
#include "soup-session-feature-private.h"
#include "soup-uri-utils-private.h"
#include <stdfil.h>

/**
 * SoupCookieJar:
 *
 * Automatic cookie handling for SoupSession.
 *
 * A #SoupCookieJar stores [struct@Cookie]s and arrange for them to be sent with
 * the appropriate [class@Message]s. #SoupCookieJar implements
 * [iface@SessionFeature], so you can add a cookie jar to a session with
 * [method@Session.add_feature] or [method@Session.add_feature_by_type].
 *
 * Note that the base #SoupCookieJar class does not support any form
 * of long-term cookie persistence.
 **/

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum {
	PROP_0,

	PROP_READ_ONLY,
	PROP_ACCEPT_POLICY,

	LAST_PROPERTY
};

static GParamSpec *properties[LAST_PROPERTY] = { NULL, };

typedef struct {
        GMutex mutex;
	gboolean constructed, read_only;
	GHashTable *domains, *serials;
	guint serial;
	SoupCookieJarAcceptPolicy accept_policy;
} SoupCookieJarPrivate;

static void soup_cookie_jar_session_feature_init (SoupSessionFeatureInterface *feature_interface, gpointer interface_data);

G_DEFINE_TYPE_WITH_CODE (SoupCookieJar, soup_cookie_jar, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (SoupCookieJar)
			 G_IMPLEMENT_INTERFACE (SOUP_TYPE_SESSION_FEATURE,
						soup_cookie_jar_session_feature_init))

static void
soup_cookie_jar_init (SoupCookieJar *jar)
{
	SoupCookieJarPrivate *priv = soup_cookie_jar_get_instance_private (jar);

	priv->domains = g_hash_table_new_full (soup_str_case_hash,
					       soup_str_case_equal,
					       g_free, NULL);
	priv->serials = g_hash_table_new (NULL, NULL);
	priv->accept_policy = SOUP_COOKIE_JAR_ACCEPT_ALWAYS;
        g_mutex_init (&priv->mutex);
}

static void
soup_cookie_jar_constructed (GObject *object)
{
	SoupCookieJarPrivate *priv =
		soup_cookie_jar_get_instance_private (SOUP_COOKIE_JAR (object));

	priv->constructed = TRUE;
}

static void
soup_cookie_jar_finalize (GObject *object)
{
	SoupCookieJarPrivate *priv =
		soup_cookie_jar_get_instance_private (SOUP_COOKIE_JAR (object));
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, priv->domains);
	while (g_hash_table_iter_next (&iter, &key, &value))
		soup_cookies_free (value);
	g_hash_table_destroy (priv->domains);
	g_hash_table_destroy (priv->serials);
        g_mutex_clear (&priv->mutex);

	G_OBJECT_CLASS (soup_cookie_jar_parent_class)->finalize (object);
}

static void
soup_cookie_jar_set_property (GObject *object, guint prop_id,
			      const GValue *value, GParamSpec *pspec)
{
	SoupCookieJarPrivate *priv =
		soup_cookie_jar_get_instance_private (SOUP_COOKIE_JAR (object));

	switch (prop_id) {
	case PROP_READ_ONLY:
		priv->read_only = g_value_get_boolean (value);
		break;
	case PROP_ACCEPT_POLICY:
		priv->accept_policy = g_value_get_enum (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
soup_cookie_jar_get_property (GObject *object, guint prop_id,
			      GValue *value, GParamSpec *pspec)
{
	SoupCookieJarPrivate *priv =
		soup_cookie_jar_get_instance_private (SOUP_COOKIE_JAR (object));

	switch (prop_id) {
	case PROP_READ_ONLY:
		g_value_set_boolean (value, priv->read_only);
		break;
	case PROP_ACCEPT_POLICY:
		g_value_set_enum (value, priv->accept_policy);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gboolean
soup_cookie_jar_real_is_persistent (SoupCookieJar *jar)
{
	return FALSE;
}

static void
soup_cookie_jar_class_init (SoupCookieJarClass *jar_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (jar_class);

	object_class->constructed = soup_cookie_jar_constructed;
	object_class->finalize = soup_cookie_jar_finalize;
	object_class->set_property = soup_cookie_jar_set_property;
	object_class->get_property = soup_cookie_jar_get_property;

	jar_class->is_persistent = soup_cookie_jar_real_is_persistent;

	/**
	 * SoupCookieJar::changed:
	 * @jar: the #SoupCookieJar
	 * @old_cookie: the old #SoupCookie value
	 * @new_cookie: the new #SoupCookie value
	 *
	 * Emitted when @jar changes.
	 *
	 * If a cookie has been added,
	 * @new_cookie will contain the newly-added cookie and
	 * @old_cookie will be %NULL. If a cookie has been deleted,
	 * @old_cookie will contain the to-be-deleted cookie and
	 * @new_cookie will be %NULL. If a cookie has been changed,
	 * @old_cookie will contain its old value, and @new_cookie its
	 * new value.
	 **/
	signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (SoupCookieJarClass, changed),
			      NULL, NULL,
			      NULL,
			      G_TYPE_NONE, 2, 
			      zorptr (SOUP_TYPE_COOKIE, (uintptr_t) G_SIGNAL_TYPE_STATIC_SCOPE),
			      zorptr (SOUP_TYPE_COOKIE, (uintptr_t) G_SIGNAL_TYPE_STATIC_SCOPE));

	/**
	 * SoupCookieJar:read-only:
	 *
	 * Whether or not the cookie jar is read-only.
	 */
        properties[PROP_READ_ONLY] =
		g_param_spec_boolean ("read-only",
				      "Read-only",
				      "Whether or not the cookie jar is read-only",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				      G_PARAM_STATIC_STRINGS);

	/**
	 * SoupCookieJar:accept-policy: (attributes org.gtk.Property.get=soup_cookie_jar_get_accept_policy org.gtk.Property.set=soup_cookie_jar_set_accept_policy)
	 *
	 * The policy the jar should follow to accept or reject cookies.
	 */
        properties[PROP_ACCEPT_POLICY] =
		g_param_spec_enum ("accept-policy",
				   "Accept-policy",
				   "The policy the jar should follow to accept or reject cookies",
				   SOUP_TYPE_COOKIE_JAR_ACCEPT_POLICY,
				   SOUP_COOKIE_JAR_ACCEPT_ALWAYS,
				   G_PARAM_READWRITE |
				   G_PARAM_STATIC_STRINGS);

        g_object_class_install_properties (object_class, LAST_PROPERTY, properties);
}

/**
 * soup_cookie_jar_new:
 *
 * Creates a new #SoupCookieJar.
 *
 * The base #SoupCookieJar class does not support persistent storage of cookies;
 * use a subclass for that.
 *
 * Returns: a new #SoupCookieJar
 **/
SoupCookieJar *
soup_cookie_jar_new (void) 
{
	return g_object_new (SOUP_TYPE_COOKIE_JAR, NULL);
}

static void
soup_cookie_jar_changed (SoupCookieJar *jar,
			 SoupCookie *old, SoupCookie *new)
{
	SoupCookieJarPrivate *priv = soup_cookie_jar_get_instance_private (jar);

	if (old && old != new)
		g_hash_table_remove (priv->serials, old);
	if (new) {
		priv->serial++;
		g_hash_table_insert (priv->serials, new, GUINT_TO_POINTER (priv->serial));
	}

	if (priv->read_only || !priv->constructed)
		return;

	g_signal_emit (jar, signals[CHANGED], 0, old, new);
}

static int
compare_cookies (gconstpointer a, gconstpointer b, gpointer jar)
{
	SoupCookie *ca = (SoupCookie *)a;
	SoupCookie *cb = (SoupCookie *)b;
	SoupCookieJarPrivate *priv = soup_cookie_jar_get_instance_private (jar);
	int alen, blen;
	guint aserial, bserial;

	/* "Cookies with longer path fields are listed before cookies
	 * with shorter path field."
	 */
	alen = soup_cookie_get_path (ca) ? strlen (soup_cookie_get_path (ca)) : 0;
	blen = soup_cookie_get_path (cb) ? strlen (soup_cookie_get_path (cb)) : 0;
	if (alen != blen)
		return blen - alen;

	/* "Among cookies that have equal length path fields, cookies
	 * with earlier creation dates are listed before cookies with
	 * later creation dates."
	 */
	aserial = GPOINTER_TO_UINT (g_hash_table_lookup (priv->serials, ca));
	bserial = GPOINTER_TO_UINT (g_hash_table_lookup (priv->serials, cb));
	return aserial - bserial;
}

static gboolean
cookie_is_valid_for_same_site_policy (SoupCookie *cookie,
                                      gboolean    is_safe_method,
                                      GUri       *uri,
                                      GUri       *top_level,
                                      GUri       *cookie_uri,
                                      gboolean    is_top_level_navigation,
                                      gboolean    for_http)
{
	SoupSameSitePolicy policy = soup_cookie_get_same_site_policy (cookie);

	if (policy == SOUP_SAME_SITE_POLICY_NONE)
		return TRUE;

	if (top_level == NULL)
		return TRUE;

	if (policy == SOUP_SAME_SITE_POLICY_LAX && is_top_level_navigation &&
	    (is_safe_method || for_http == FALSE))
		return TRUE;

	if (is_top_level_navigation && cookie_uri == NULL)
		return FALSE;

	return soup_host_matches_host (g_uri_get_host (cookie_uri ? cookie_uri : top_level), g_uri_get_host (uri));
}

static GSList *
get_cookies (SoupCookieJar *jar,
             GUri          *uri,
             GUri          *top_level,
             GUri          *site_for_cookies,
             gboolean       is_safe_method,
             gboolean       for_http,
             gboolean       is_top_level_navigation,
             gboolean       copy_cookies)
{
	SoupCookieJarPrivate *priv;
	GSList *cookies, *domain_cookies;
	char *domain, *cur, *next_domain;
	GSList *new_head, *cookies_to_remove = NULL, *p;
        const char *host = g_uri_get_host (uri);

	priv = soup_cookie_jar_get_instance_private (jar);

	if (!host)
		return NULL;

	/* The logic here is a little weird, but the plan is that if
	 * host is "www.foo.com", we will end up looking up
	 * cookies for ".www.foo.com", "www.foo.com", ".foo.com", and
	 * ".com", in that order. (Logic stolen from Mozilla.)
	 */
	cookies = NULL;
        if (host[0]) {
                domain = cur = g_strdup_printf (".%s", host);
                next_domain = domain + 1;
        } else {
                domain = cur = g_strdup (host);
                next_domain = NULL;
        }

        g_mutex_lock (&priv->mutex);

	do {
		new_head = domain_cookies = g_hash_table_lookup (priv->domains, cur);
		while (domain_cookies) {
			GSList *next = domain_cookies->next;
			SoupCookie *cookie = domain_cookies->data;

			if (soup_cookie_get_expires (cookie) && soup_date_time_is_past (soup_cookie_get_expires (cookie))) {
				cookies_to_remove = g_slist_append (cookies_to_remove,
								    cookie);
				new_head = g_slist_delete_link (new_head, domain_cookies);
				g_hash_table_insert (priv->domains,
						     g_strdup (cur),
						     new_head);
			} else if (soup_cookie_applies_to_uri (cookie, uri) &&
			           cookie_is_valid_for_same_site_policy (cookie, is_safe_method, uri, top_level,
				                                         site_for_cookies, is_top_level_navigation,
									 for_http) &&
				   (for_http || !soup_cookie_get_http_only (cookie)))
				cookies = g_slist_append (cookies, copy_cookies ? soup_cookie_copy (cookie) : cookie);

			domain_cookies = next;
		}
		cur = next_domain;
		if (cur)
			next_domain = strchr (cur + 1, '.');
	} while (cur);
	g_free (domain);

	for (p = cookies_to_remove; p; p = p->next) {
		SoupCookie *cookie = p->data;

		soup_cookie_jar_changed (jar, cookie, NULL);
		soup_cookie_free (cookie);
	}
	g_slist_free (cookies_to_remove);

        g_mutex_unlock (&priv->mutex);

	return g_slist_sort_with_data (cookies, compare_cookies, jar);
}

/**
 * soup_cookie_jar_get_cookies:
 * @jar: a #SoupCookieJar
 * @uri: a #GUri
 * @for_http: whether or not the return value is being passed directly
 *   to an HTTP operation
 *
 * Retrieves (in Cookie-header form) the list of cookies that would
 * be sent with a request to @uri.
 *
 * If @for_http is %TRUE, the return value will include cookies marked
 * "HttpOnly" (that is, cookies that the server wishes to keep hidden
 * from client-side scripting operations such as the JavaScript
 * document.cookies property). Since #SoupCookieJar sets the Cookie
 * header itself when making the actual HTTP request, you should
 * almost certainly be setting @for_http to %FALSE if you are calling
 * this.
 *
 * Returns: (nullable): the cookies, in string form, or %NULL if
 *   there are no cookies for @uri.
 **/
char *
soup_cookie_jar_get_cookies (SoupCookieJar *jar, GUri *uri,
			     gboolean for_http)
{
	GSList *cookies;

	g_return_val_if_fail (SOUP_IS_COOKIE_JAR (jar), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	cookies = get_cookies (jar, uri, NULL, NULL, TRUE, for_http, FALSE, FALSE);

	if (cookies) {
		char *result = soup_cookies_to_cookie_header (cookies);
		g_slist_free (cookies);

		if (!*result) {
			g_free (result);
			result = NULL;
		}
		return result;
	} else
		return NULL;
}

/**
 * soup_cookie_jar_get_cookie_list:
 * @jar: a #SoupCookieJar
 * @uri: a #GUri
 * @for_http: whether or not the return value is being passed directly
 *   to an HTTP operation
 *
 * Retrieves the list of cookies that would be sent with a request to @uri
 * as a [struct@GLib.List] of #SoupCookie objects.
 *
 * If @for_http is %TRUE, the return value will include cookies marked
 * "HttpOnly" (that is, cookies that the server wishes to keep hidden
 * from client-side scripting operations such as the JavaScript
 * document.cookies property). Since #SoupCookieJar sets the Cookie
 * header itself when making the actual HTTP request, you should
 * almost certainly be setting @for_http to %FALSE if you are calling
 * this.
 *
 * Returns: (transfer full) (element-type Soup.Cookie): a #GSList
 *   with the cookies in the @jar that would be sent with a request to @uri.
 **/
GSList *
soup_cookie_jar_get_cookie_list (SoupCookieJar *jar, GUri *uri, gboolean for_http)
{
	g_return_val_if_fail (SOUP_IS_COOKIE_JAR (jar), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	return get_cookies (jar, uri, NULL, NULL, TRUE, for_http, FALSE, TRUE);
}

/**
 * soup_cookie_jar_get_cookie_list_with_same_site_info:
 * @jar: a #SoupCookieJar
 * @uri: a #GUri
 * @top_level: (nullable): a #GUri for the top level document
 * @site_for_cookies: (nullable): a #GUri indicating the origin to get cookies for
 * @for_http: whether or not the return value is being passed directly
 *   to an HTTP operation
 * @is_safe_method: if the HTTP method is safe, as defined by RFC 7231, ignored when @for_http is %FALSE
 * @is_top_level_navigation: whether or not the HTTP request is part of
 *   top level navigation
 *
 * This is an extended version of [method@CookieJar.get_cookie_list] that
 * provides more information required to use SameSite cookies.
 *
 * See the [SameSite cookies
 * spec](https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-00) for
 * more detailed information.
 *
 * Returns: (transfer full) (element-type Soup.Cookie): a #GSList
 *   with the cookies in the @jar that would be sent with a request to @uri.
 */
GSList *
soup_cookie_jar_get_cookie_list_with_same_site_info (SoupCookieJar *jar,
                                                     GUri          *uri,
                                                     GUri          *top_level,
                                                     GUri          *site_for_cookies,
                                                     gboolean       for_http,
                                                     gboolean       is_safe_method,
                                                     gboolean       is_top_level_navigation)
{
	g_return_val_if_fail (SOUP_IS_COOKIE_JAR (jar), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	return get_cookies (jar,  uri, top_level, site_for_cookies, is_safe_method, for_http, is_top_level_navigation, TRUE);
}

static const char *
normalize_cookie_domain (const char *domain)
{
	/* Trim any leading dot if present to transform the cookie
         * domain into a valid hostname.
         */
	if (domain != NULL && domain[0] == '.')
		return domain + 1;
	return domain;
}

static gboolean
incoming_cookie_is_third_party (SoupCookieJar            *jar,
				SoupCookie               *cookie,
				GUri                     *first_party,
				SoupCookieJarAcceptPolicy policy)
{
	SoupCookieJarPrivate *priv;
	const char *normalized_cookie_domain;
	const char *cookie_base_domain;
	const char *first_party_base_domain;
        const char *first_party_host;
        gboolean retval;

	if (policy != SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY &&
	    policy != SOUP_COOKIE_JAR_ACCEPT_GRANDFATHERED_THIRD_PARTY)
		return FALSE;

	if (first_party == NULL)
                return TRUE;

        first_party_host = g_uri_get_host (first_party);
        if (first_party_host == NULL)
		return TRUE;

	normalized_cookie_domain = normalize_cookie_domain (soup_cookie_get_domain (cookie));
	cookie_base_domain = soup_tld_get_base_domain (normalized_cookie_domain, NULL);
	if (cookie_base_domain == NULL)
		cookie_base_domain = soup_cookie_get_domain (cookie);

	first_party_base_domain = soup_tld_get_base_domain (first_party_host, NULL);
	if (first_party_base_domain == NULL)
		first_party_base_domain = first_party_host;

	if (soup_host_matches_host (cookie_base_domain, first_party_base_domain))
		return FALSE;

	if (policy == SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY)
		return TRUE;

	/* Now we know the cookie's base domain and the first party's base domain
	 * are different, but for SOUP_COOKIE_JAR_ACCEPT_GRANDFATHERED_THIRD_PARTY
	 * policy we want to grandfather in any domain that's already in the jar.
	 * That is, we never want to block cookies from domains the user has
	 * previously visited directly.
	 */
	priv = soup_cookie_jar_get_instance_private (jar);
        g_mutex_lock (&priv->mutex);
	retval = !g_hash_table_lookup (priv->domains, soup_cookie_get_domain (cookie));
        g_mutex_unlock (&priv->mutex);

        return retval;
}

static gboolean
string_contains_ctrlcode (const char *s)
{
	const char *p;

	p = s;
	while (*p != '\0') {
		if (g_ascii_iscntrl (*p) && *p != 0x09)
			return TRUE;
		
		p++;
	}
	return FALSE;
}

/**
 * soup_cookie_jar_add_cookie_full:
 * @jar: a #SoupCookieJar
 * @cookie: (transfer full): a #SoupCookie
 * @uri: (nullable): the URI setting the cookie
 * @first_party: (nullable): the URI for the main document
 *
 * Adds @cookie to @jar.
 *
 * Emits the [signal@CookieJar::changed] signal if we are modifying an existing
 * cookie or adding a valid new cookie ('valid' means that the cookie's expire
 * date is not in the past).
 *
 * @first_party will be used to reject cookies coming from third party
 * resources in case such a security policy is set in the @jar.
 *
 * @uri will be used to reject setting or overwriting secure cookies
 * from insecure origins. %NULL is treated as secure.
 * 
 * @cookie will be 'stolen' by the jar, so don't free it afterwards.
 **/
void
soup_cookie_jar_add_cookie_full (SoupCookieJar *jar, SoupCookie *cookie, GUri *uri, GUri *first_party)
{
	SoupCookieJarPrivate *priv;
	GSList *old_cookies, *oc, *last = NULL;
	SoupCookie *old_cookie;

	g_return_if_fail (SOUP_IS_COOKIE_JAR (jar));
	g_return_if_fail (cookie != NULL);

	/* Never accept cookies for public domains. */
	if (!g_hostname_is_ip_address (soup_cookie_get_domain (cookie)) &&
	    soup_tld_domain_is_public_suffix (soup_cookie_get_domain (cookie))) {
		soup_cookie_free (cookie);
		return;
	}

	priv = soup_cookie_jar_get_instance_private (jar);

        if (first_party != NULL) {
                if (priv->accept_policy == SOUP_COOKIE_JAR_ACCEPT_NEVER ||
                    incoming_cookie_is_third_party (jar, cookie, first_party, priv->accept_policy)) {
                        soup_cookie_free (cookie);
                        return;
                }
        }

	/* Cannot set a secure cookie over http */
	if (uri != NULL && !soup_uri_is_https (uri) && soup_cookie_get_secure (cookie)) {
		soup_cookie_free (cookie);
		return;
	}

	/* SameSite=None cookies are rejected unless the Secure attribute is set. */
	if (soup_cookie_get_same_site_policy (cookie) == SOUP_SAME_SITE_POLICY_NONE && !soup_cookie_get_secure (cookie)) {
		soup_cookie_free (cookie);
		return;
	}

        /* See https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-cookie-prefixes-00 for handling the prefixes,
         * which has been implemented by Firefox and Chrome. */
#define MATCH_PREFIX(name, prefix) (!g_ascii_strncasecmp (name, prefix, strlen(prefix)))

	/* Cookies with a "__Secure-" prefix should have Secure attribute set and it must be for a secure host. */
	if (MATCH_PREFIX (soup_cookie_get_name (cookie), "__Secure-") && !soup_cookie_get_secure (cookie) ) {
		soup_cookie_free (cookie);
		return;
	}
        /* Path=/ and Secure attributes are required; Domain attribute must not be present.
         Note that SoupCookie always sets the domain so we ensure its not a subdomain match. */
	if (MATCH_PREFIX (soup_cookie_get_name (cookie), "__Host-")) {
		if (!soup_cookie_get_secure (cookie) ||
		    strcmp (soup_cookie_get_path (cookie), "/") != 0 ||
                    soup_cookie_get_domain (cookie)[0] == '.') {
			soup_cookie_free (cookie);
			return;
		}
	}

	/* Cookies should not take control characters %x00-1F / %x7F (defined by RFC 5234) in names or values,
	 * with the exception of %x09 (the tab character).
	 */    
	const char *name, *value;
	name = soup_cookie_get_name (cookie);
	value = soup_cookie_get_value (cookie);
	if (string_contains_ctrlcode (name) || string_contains_ctrlcode (value)) {
		soup_cookie_free (cookie);
		return;
	}
	
	if (strlen(name) > 4096 || strlen(value) > 4096) {
		soup_cookie_free (cookie);
		return;
	}
	
        g_mutex_lock (&priv->mutex);

	old_cookies = g_hash_table_lookup (priv->domains, soup_cookie_get_domain (cookie));
	for (oc = old_cookies; oc; oc = oc->next) {
		old_cookie = oc->data;
		if (!strcmp (soup_cookie_get_name (cookie), soup_cookie_get_name (old_cookie)) &&
		    !g_strcmp0 (soup_cookie_get_path (cookie), soup_cookie_get_path (old_cookie))) {
			if (soup_cookie_get_secure (oc->data) && uri != NULL && !soup_uri_is_https (uri)) {
				/* We do not allow overwriting secure cookies from an insecure origin
				 * https://tools.ietf.org/html/draft-ietf-httpbis-cookie-alone-01
				 */
				soup_cookie_free (cookie);
			} else if (soup_cookie_get_expires (cookie) && soup_date_time_is_past (soup_cookie_get_expires (cookie))) {
				/* The new cookie has an expired date,
				 * this is the way the the server has
				 * of telling us that we have to
				 * remove the cookie.
				 */
				old_cookies = g_slist_delete_link (old_cookies, oc);
				g_hash_table_insert (priv->domains,
						     g_strdup (soup_cookie_get_domain (cookie)),
						     old_cookies);
				soup_cookie_jar_changed (jar, old_cookie, NULL);
				soup_cookie_free (old_cookie);
				soup_cookie_free (cookie);
			} else {
				oc->data = cookie;
				soup_cookie_jar_changed (jar, old_cookie, cookie);
				soup_cookie_free (old_cookie);
			}

                        g_mutex_unlock (&priv->mutex);

			return;
		}
		last = oc;
	}

	/* The new cookie is... a new cookie */
	if (soup_cookie_get_expires (cookie) && soup_date_time_is_past (soup_cookie_get_expires (cookie))) {
		soup_cookie_free (cookie);
                g_mutex_unlock (&priv->mutex);
		return;
	}

	if (last)
		last->next = g_slist_append (NULL, cookie);
	else {
		old_cookies = g_slist_append (NULL, cookie);
		g_hash_table_insert (priv->domains, g_strdup (soup_cookie_get_domain (cookie)),
				     old_cookies);
	}

	soup_cookie_jar_changed (jar, NULL, cookie);

        g_mutex_unlock (&priv->mutex);
}

/**
 * soup_cookie_jar_add_cookie:
 * @jar: a #SoupCookieJar
 * @cookie: (transfer full): a #SoupCookie
 *
 * Adds @cookie to @jar.
 *
 * Emits the [signal@CookieJar::changed] signal if we are modifying
 * an existing cookie or adding a valid new cookie ('valid' means
 * that the cookie's expire date is not in the past).
 *
 * @cookie will be 'stolen' by the jar, so don't free it afterwards.
 **/
void
soup_cookie_jar_add_cookie (SoupCookieJar *jar, SoupCookie *cookie)
{
	soup_cookie_jar_add_cookie_full (jar, cookie, NULL, NULL);
}

/**
 * soup_cookie_jar_add_cookie_with_first_party:
 * @jar: a #SoupCookieJar
 * @first_party: the URI for the main document
 * @cookie: (transfer full): a #SoupCookie
 *
 * Adds @cookie to @jar.
 *
 * Emits the [signal@CookieJar::changed] signal if we are modifying
 * an existing cookie or adding a valid new cookie ('valid' means
 * that the cookie's expire date is not in the past).
 *
 * @first_party will be used to reject cookies coming from third party
 * resources in case such a security policy is set in the @jar.
 *
 * @cookie will be 'stolen' by the jar, so don't free it afterwards.
 *
 * For secure cookies to work properly you may want to use
 * [method@CookieJar.add_cookie_full].
 **/
void
soup_cookie_jar_add_cookie_with_first_party (SoupCookieJar *jar, GUri *first_party, SoupCookie *cookie)
{
	g_return_if_fail (first_party != NULL);

	soup_cookie_jar_add_cookie_full (jar, cookie, NULL, first_party);
}

/**
 * soup_cookie_jar_set_cookie:
 * @jar: a #SoupCookieJar
 * @uri: the URI setting the cookie
 * @cookie: the stringified cookie to set
 *
 * Adds @cookie to @jar, exactly as though it had appeared in a
 * Set-Cookie header returned from a request to @uri.
 *
 * Keep in mind that if the [enum@CookieJarAcceptPolicy] set is either
 * %SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY or
 * %SOUP_COOKIE_JAR_ACCEPT_GRANDFATHERED_THIRD_PARTY you'll need to use
 * [method@CookieJar.set_cookie_with_first_party], otherwise the jar
 * will have no way of knowing if the cookie is being set by a third
 * party or not.
 **/
void
soup_cookie_jar_set_cookie (SoupCookieJar *jar, GUri *uri,
			    const char *cookie)
{
	SoupCookie *soup_cookie;
	SoupCookieJarPrivate *priv;

	g_return_if_fail (SOUP_IS_COOKIE_JAR (jar));
	g_return_if_fail (uri != NULL);
	g_return_if_fail (cookie != NULL);

	if (!g_uri_get_host (uri))
		return;

	priv = soup_cookie_jar_get_instance_private (jar);
	if (priv->accept_policy == SOUP_COOKIE_JAR_ACCEPT_NEVER)
		return;

	g_return_if_fail (priv->accept_policy != SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY &&
			  priv->accept_policy != SOUP_COOKIE_JAR_ACCEPT_GRANDFATHERED_THIRD_PARTY);

	soup_cookie = soup_cookie_parse (cookie, uri);
	if (soup_cookie) {
		/* will steal or free soup_cookie */
		soup_cookie_jar_add_cookie_full (jar, soup_cookie, uri, NULL);
	}
}

/**
 * soup_cookie_jar_set_cookie_with_first_party:
 * @jar: a #SoupCookieJar
 * @uri: the URI setting the cookie
 * @first_party: the URI for the main document
 * @cookie: the stringified cookie to set
 *
 * Adds @cookie to @jar, exactly as though it had appeared in a
 * Set-Cookie header returned from a request to @uri.
 *
 * @first_party will be used to reject cookies coming from third party resources
 * in case such a security policy is set in the @jar.
 **/
void
soup_cookie_jar_set_cookie_with_first_party (SoupCookieJar *jar,
					     GUri *uri,
					     GUri *first_party,
					     const char *cookie)
{
	SoupCookie *soup_cookie;

	g_return_if_fail (SOUP_IS_COOKIE_JAR (jar));
	g_return_if_fail (uri != NULL);
	g_return_if_fail (first_party != NULL);
	g_return_if_fail (cookie != NULL);

	if (!g_uri_get_host (uri))
		return;

	soup_cookie = soup_cookie_parse (cookie, uri);
	if (soup_cookie) {
		soup_cookie_jar_add_cookie_full (jar, soup_cookie, uri, first_party);
	}
}

static void
process_set_cookie_header (SoupMessage *msg, gpointer user_data)
{
	SoupCookieJar *jar = user_data;
	SoupCookieJarPrivate *priv = soup_cookie_jar_get_instance_private (jar);
	GSList *new_cookies, *nc;
	GUri *first_party, *uri;

	if (priv->accept_policy == SOUP_COOKIE_JAR_ACCEPT_NEVER)
		return;

	new_cookies = soup_cookies_from_response (msg);
	first_party = soup_message_get_first_party (msg);
	uri = soup_message_get_uri (msg);
	for (nc = new_cookies; nc; nc = nc->next) {		
		soup_cookie_jar_add_cookie_full (jar, g_steal_pointer (&nc->data), uri, first_party);
	}
	g_slist_free (new_cookies);
}

static void
msg_starting_cb (SoupMessage *msg, gpointer feature)
{
	SoupCookieJar *jar = SOUP_COOKIE_JAR (feature);
	GSList *cookies;

	cookies = soup_cookie_jar_get_cookie_list_with_same_site_info (jar, soup_message_get_uri (msg),
	                                                               soup_message_get_first_party (msg),
							               soup_message_get_site_for_cookies (msg),
								       TRUE,
							               SOUP_METHOD_IS_SAFE (soup_message_get_method (msg)),
							               soup_message_get_is_top_level_navigation (msg));
	if (cookies != NULL) {
		char *cookie_header = soup_cookies_to_cookie_header (cookies);
		soup_message_headers_replace_common (soup_message_get_request_headers (msg), SOUP_HEADER_COOKIE, cookie_header);
		g_free (cookie_header);
		g_slist_free_full (cookies, (GDestroyNotify)soup_cookie_free);
	} else {
		soup_message_headers_remove_common (soup_message_get_request_headers (msg), SOUP_HEADER_COOKIE);
	}
}

static void
soup_cookie_jar_request_queued (SoupSessionFeature *feature,
				SoupMessage        *msg)
{
	g_signal_connect (msg, "starting",
			  G_CALLBACK (msg_starting_cb),
			  feature);

	soup_message_add_header_handler (msg, "got-headers",
					 "Set-Cookie",
					 G_CALLBACK (process_set_cookie_header),
					 feature);
        soup_message_add_status_code_handler (msg, "got-informational",
                                              SOUP_STATUS_SWITCHING_PROTOCOLS,
                                              G_CALLBACK (process_set_cookie_header),
                                              feature);
}

static void
soup_cookie_jar_request_unqueued (SoupSessionFeature *feature,
				  SoupMessage        *msg)
{
	g_signal_handlers_disconnect_by_data (msg, feature);
}

static void
soup_cookie_jar_session_feature_init (SoupSessionFeatureInterface *feature_interface,
				      gpointer interface_data)
{
	feature_interface->request_queued = soup_cookie_jar_request_queued;
	feature_interface->request_unqueued = soup_cookie_jar_request_unqueued;
}

/**
 * soup_cookie_jar_all_cookies:
 * @jar: a #SoupCookieJar
 *
 * Constructs a [struct@GLib.List] with every cookie inside the @jar.
 *
 * The cookies in the list are a copy of the original, so
 * you have to free them when you are done with them.
 *
 * Returns: (transfer full) (element-type Soup.Cookie): a #GSList
 *   with all the cookies in the @jar.
 **/
GSList *
soup_cookie_jar_all_cookies (SoupCookieJar *jar)
{
	SoupCookieJarPrivate *priv;
	GHashTableIter iter;
	GSList *l = NULL;
	gpointer key, value;

	g_return_val_if_fail (SOUP_IS_COOKIE_JAR (jar), NULL);

	priv = soup_cookie_jar_get_instance_private (jar);

        g_mutex_lock (&priv->mutex);

	g_hash_table_iter_init (&iter, priv->domains);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GSList *p, *cookies = value;
		for (p = cookies; p; p = p->next)
			l = g_slist_prepend (l, soup_cookie_copy (p->data));
	}

        g_mutex_unlock (&priv->mutex);

	return l;
}

/**
 * soup_cookie_jar_delete_cookie:
 * @jar: a #SoupCookieJar
 * @cookie: a #SoupCookie
 *
 * Deletes @cookie from @jar.
 *
 * Emits the [signal@CookieJar::changed] signal.
 **/
void
soup_cookie_jar_delete_cookie (SoupCookieJar *jar,
			       SoupCookie    *cookie)
{
	SoupCookieJarPrivate *priv;
	GSList *cookies, *p;

	g_return_if_fail (SOUP_IS_COOKIE_JAR (jar));
	g_return_if_fail (cookie != NULL);

	priv = soup_cookie_jar_get_instance_private (jar);

        g_mutex_lock (&priv->mutex);

	cookies = g_hash_table_lookup (priv->domains, soup_cookie_get_domain (cookie));
	if (cookies == NULL) {
                g_mutex_unlock (&priv->mutex);
		return;
        }

	for (p = cookies; p; p = p->next ) {
		SoupCookie *c = (SoupCookie*)p->data;
		if (soup_cookie_equal (cookie, c)) {
			cookies = g_slist_delete_link (cookies, p);
			g_hash_table_insert (priv->domains,
					     g_strdup (soup_cookie_get_domain (cookie)),
					     cookies);
			soup_cookie_jar_changed (jar, c, NULL);
			soup_cookie_free (c);
                        g_mutex_unlock (&priv->mutex);
			return;
		}
	}

        g_mutex_unlock (&priv->mutex);
}

/**
 * SoupCookieJarAcceptPolicy:
 * @SOUP_COOKIE_JAR_ACCEPT_ALWAYS: accept all cookies unconditionally.
 * @SOUP_COOKIE_JAR_ACCEPT_NEVER: reject all cookies unconditionally.
 * @SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY: accept all cookies set by the main
 *   document loaded in the application using libsoup. An example of the most
 *   common case, web browsers, would be: If http://www.example.com is the page
 *   loaded, accept all cookies set by example.com, but if a resource from
 *   http://www.third-party.com is loaded from that page reject any cookie that
 *   it could try to set. For libsoup to be able to tell apart first party
 *   cookies from the rest, the application must call
 *   [method@Message.set_first_party] on each outgoing [class@Message], setting
 *   the [struct@GLib.Uri] of the main document. If no first party is set in a
 *   message when this policy is in effect, cookies will be assumed to be third
 *   party by default.
 * @SOUP_COOKIE_JAR_ACCEPT_GRANDFATHERED_THIRD_PARTY: accept all cookies set by
 *   the main document loaded in the application using libsoup, and from domains
 *   that have previously set at least one cookie when loaded as the main
 *   document. An example of the most common case, web browsers, would be: if
 *   http://www.example.com is the page loaded, accept all cookies set by
 *   example.com, but if a resource from http://www.third-party.com is loaded
 *   from that page, reject any cookie that it could try to set unless it
 *   already has a cookie in the cookie jar. For libsoup to be able to tell
 *   apart first party cookies from the rest, the application must call
 *   [method@Message.set_first_party] on each outgoing #SoupMessage, setting the
 *   [struct@GLib.Uri] of the main document. If no first party is set in a
 *   message when this policy is in effect, cookies will be assumed to be third
 *   party by default.
 *
 * The policy for accepting or rejecting cookies returned in
 * responses.
 */

/**
 * soup_cookie_jar_get_accept_policy: (attributes org.gtk.Method.get_property=accept-policy)
 * @jar: a #SoupCookieJar
 *
 * Gets @jar's [enum@CookieJarAcceptPolicy].
 *
 * Returns: the #SoupCookieJarAcceptPolicy set in the @jar
 **/
SoupCookieJarAcceptPolicy
soup_cookie_jar_get_accept_policy (SoupCookieJar *jar)
{
	SoupCookieJarPrivate *priv;

	g_return_val_if_fail (SOUP_IS_COOKIE_JAR (jar), SOUP_COOKIE_JAR_ACCEPT_ALWAYS);

	priv = soup_cookie_jar_get_instance_private (jar);
	return priv->accept_policy;
}

/**
 * soup_cookie_jar_set_accept_policy: (attributes org.gtk.Method.set_property=accept-policy)
 * @jar: a #SoupCookieJar
 * @policy: a #SoupCookieJarAcceptPolicy
 * 
 * Sets @policy as the cookie acceptance policy for @jar.
 **/
void
soup_cookie_jar_set_accept_policy (SoupCookieJar *jar,
				   SoupCookieJarAcceptPolicy policy)
{
	SoupCookieJarPrivate *priv;

	g_return_if_fail (SOUP_IS_COOKIE_JAR (jar));

	priv = soup_cookie_jar_get_instance_private (jar);

	if (priv->accept_policy != policy) {
		priv->accept_policy = policy;
		g_object_notify_by_pspec (G_OBJECT (jar), properties[PROP_ACCEPT_POLICY]);
	}
}

/**
 * soup_cookie_jar_is_persistent:
 * @jar: a #SoupCookieJar
 *
 * Gets whether @jar stores cookies persistenly.
 *
 * Returns: %TRUE if @jar storage is persistent or %FALSE otherwise.
 **/
gboolean
soup_cookie_jar_is_persistent (SoupCookieJar *jar)
{
	g_return_val_if_fail (SOUP_IS_COOKIE_JAR (jar), FALSE);

	return SOUP_COOKIE_JAR_GET_CLASS (jar)->is_persistent (jar);
}
