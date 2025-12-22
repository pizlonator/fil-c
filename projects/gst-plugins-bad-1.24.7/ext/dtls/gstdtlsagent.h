/*
 * Copyright (c) 2014, Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifndef gstdtlsagent_h
#define gstdtlsagent_h

#include "gstdtlscertificate.h"

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_DTLS_AGENT            (gst_dtls_agent_get_type())
#define GST_DTLS_AGENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DTLS_AGENT, GstDtlsAgent))
#define GST_DTLS_AGENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DTLS_AGENT, GstDtlsAgentClass))
#define GST_IS_DTLS_AGENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DTLS_AGENT))
#define GST_IS_DTLS_AGENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DTLS_AGENT))
#define GST_DTLS_AGENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_DTLS_AGENT, GstDtlsAgentClass))

typedef gpointer GstDtlsAgentContext;

typedef struct _GstDtlsAgent        GstDtlsAgent;
typedef struct _GstDtlsAgentClass   GstDtlsAgentClass;
typedef struct _GstDtlsAgentPrivate GstDtlsAgentPrivate;

/*
 * GstDtlsAgent:
 *
 * A context for creating GstDtlsConnections with a GstDtlsCertificate.
 * GstDtlsAgent needs to be constructed with the "certificate" property set.
 */
struct _GstDtlsAgent {
    GstObject parent_instance;

    GstDtlsAgentPrivate *priv;
};

struct _GstDtlsAgentClass {
    GstObjectClass parent_class;
};

GType gst_dtls_agent_get_type(void) G_GNUC_CONST;

/*
 * Returns the certificate used by the agent.
 */
GstDtlsCertificate *gst_dtls_agent_get_certificate(GstDtlsAgent *);

/*
 * Returns the certificate used by the agent, in PEM format.
 */
gchar *gst_dtls_agent_get_certificate_pem(GstDtlsAgent *self);

/* internal */
void _gst_dtls_init_openssl(void);
const GstDtlsAgentContext _gst_dtls_agent_peek_context(GstDtlsAgent *);

G_END_DECLS

#endif /* gstdtlsagent_h */
