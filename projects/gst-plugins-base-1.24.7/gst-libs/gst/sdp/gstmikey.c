/* GStreamer
 * Copyright (C) <2014> Wim Taymans <wim.taymans@gmail.com>
 *
 * gstmikey.h: various helper functions to manipulate mikey messages
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gstmikey
 * @title: GstMIKEYMessage
 * @short_description: Helper methods for dealing with MIKEY messages
 *
 * The GstMIKEY helper functions makes it easy to parse and create MIKEY
 * messages.
 *
 * Since: 1.4
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstmikey.h"

GST_DEFINE_MINI_OBJECT_TYPE (GstMIKEYPayload, gst_mikey_payload);
GST_DEFINE_MINI_OBJECT_TYPE (GstMIKEYMessage, gst_mikey_message);

static void
payload_destroy (GstMIKEYPayload ** payload)
{
  gst_mikey_payload_unref (*payload);
}

#define INIT_ARRAY(field, type, init_func)              \
G_STMT_START {                                          \
  if (field)                                            \
    g_array_set_size ((field), 0);                      \
  else {                                                \
    (field) = g_array_new (FALSE, TRUE, sizeof (type)); \
    g_array_set_clear_func ((field), (GDestroyNotify)init_func);        \
  }                                                     \
} G_STMT_END

#define FREE_ARRAY(field)         \
G_STMT_START {                    \
  if (field)                      \
    g_array_free ((field), TRUE); \
  (field) = NULL;                 \
} G_STMT_END

#define INIT_MEMDUP(field, data, len)            \
G_STMT_START {                                   \
  g_free ((field));                              \
  (field) = g_memdup2 (data, len);                \
} G_STMT_END
#define FREE_MEMDUP(field)                       \
G_STMT_START {                                   \
  g_free ((field));                              \
  (field) = NULL;                                \
} G_STMT_END


/* Key data transport payload (KEMAC) */
static guint
get_mac_len (GstMIKEYMacAlg mac_alg)
{
  guint len;

  switch (mac_alg) {
    case GST_MIKEY_MAC_NULL:
      len = 0;                  /* no MAC key */
      break;
    case GST_MIKEY_MAC_HMAC_SHA_1_160:
      len = 20;                 /* 160 bits key */
      break;
    default:
      len = -1;
      break;
  }
  return len;
}

/**
 * gst_mikey_payload_kemac_set:
 * @payload: a #GstMIKEYPayload
 * @enc_alg: the #GstMIKEYEncAlg
 * @mac_alg: a #GstMIKEYMacAlg
 *
 * Set the KEMAC parameters. @payload should point to a %GST_MIKEY_PT_KEMAC
 * payload.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_payload_kemac_set (GstMIKEYPayload * payload,
    GstMIKEYEncAlg enc_alg, GstMIKEYMacAlg mac_alg)
{
  GstMIKEYPayloadKEMAC *p = (GstMIKEYPayloadKEMAC *) payload;

  g_return_val_if_fail (payload != NULL, FALSE);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_KEMAC, FALSE);

  p->enc_alg = enc_alg;
  p->mac_alg = mac_alg;
  INIT_ARRAY (p->subpayloads, GstMIKEYPayload *, payload_destroy);

  return TRUE;
}

static gboolean
gst_mikey_payload_kemac_dispose (GstMIKEYPayloadKEMAC * payload)
{
  FREE_ARRAY (payload->subpayloads);

  return TRUE;
}

static GstMIKEYPayloadKEMAC *
gst_mikey_payload_kemac_copy (const GstMIKEYPayloadKEMAC * payload)
{
  guint i, len;
  GstMIKEYPayloadKEMAC *copy =
      g_memdup2 (payload, sizeof (GstMIKEYPayloadKEMAC));
  gst_mikey_payload_kemac_set (&copy->pt, payload->enc_alg, payload->mac_alg);
  len = payload->subpayloads->len;
  for (i = 0; i < len; i++) {
    GstMIKEYPayload *pay =
        g_array_index (payload->subpayloads, GstMIKEYPayload *, i);
    gst_mikey_payload_kemac_add_sub (&copy->pt, gst_mikey_payload_copy (pay));
  }
  return copy;
}

/**
 * gst_mikey_payload_kemac_get_n_sub:
 * @payload: a #GstMIKEYPayload
 *
 * Get the number of sub payloads of @payload. @payload should be of type
 * %GST_MIKEY_PT_KEMAC.
 *
 * Returns: the number of sub payloads in @payload
 *
 * Since: 1.4
 */
guint
gst_mikey_payload_kemac_get_n_sub (const GstMIKEYPayload * payload)
{
  GstMIKEYPayloadKEMAC *p = (GstMIKEYPayloadKEMAC *) payload;

  g_return_val_if_fail (payload != NULL, 0);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_KEMAC, 0);

  return p->subpayloads->len;
}

/**
 * gst_mikey_payload_kemac_get_sub:
 * @payload: a #GstMIKEYPayload
 * @idx: an index
 *
 * Get the sub payload of @payload at @idx. @payload should be of type
 * %GST_MIKEY_PT_KEMAC.
 *
 * Returns: (transfer none) (nullable): the #GstMIKEYPayload at @idx.
 *
 * Since: 1.4
 */
const GstMIKEYPayload *
gst_mikey_payload_kemac_get_sub (const GstMIKEYPayload * payload, guint idx)
{
  GstMIKEYPayloadKEMAC *p = (GstMIKEYPayloadKEMAC *) payload;

  g_return_val_if_fail (payload != NULL, 0);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_KEMAC, 0);

  if (p->subpayloads->len <= idx)
    return NULL;

  return g_array_index (p->subpayloads, GstMIKEYPayload *, idx);
}

/**
 * gst_mikey_payload_kemac_remove_sub:
 * @payload: a #GstMIKEYPayload
 * @idx: the index to remove
 *
 * Remove the sub payload at @idx in @payload.
 *
 * Returns: %TRUE on success.
 *
 * Since: 1.4
 */
gboolean
gst_mikey_payload_kemac_remove_sub (GstMIKEYPayload * payload, guint idx)
{
  GstMIKEYPayloadKEMAC *p = (GstMIKEYPayloadKEMAC *) payload;

  g_return_val_if_fail (payload != NULL, 0);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_KEMAC, 0);
  g_return_val_if_fail (p->subpayloads->len > idx, FALSE);

  g_array_remove_index (p->subpayloads, idx);

  return TRUE;
}

/**
 * gst_mikey_payload_kemac_add_sub:
 * @payload: a #GstMIKEYPayload
 * @newpay: (transfer full): a #GstMIKEYPayload to add
 *
 * Add a new sub payload to @payload.
 *
 * Returns: %TRUE on success.
 *
 * Since: 1.4
 */
gboolean
gst_mikey_payload_kemac_add_sub (GstMIKEYPayload * payload,
    GstMIKEYPayload * newpay)
{
  GstMIKEYPayloadKEMAC *p = (GstMIKEYPayloadKEMAC *) payload;

  g_return_val_if_fail (payload != NULL, 0);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_KEMAC, 0);

  g_array_append_val (p->subpayloads, newpay);

  return TRUE;
}

/* Envelope data payload (PKE) */
/**
 * gst_mikey_payload_pke_set:
 * @payload: a #GstMIKEYPayload
 * @C: envelope key cache indicator
 * @data_len: the length of @data
 * @data: (array length=data_len): the encrypted envelope key
 *
 * Set the PKE values in @payload. @payload must be of type
 * %GST_MIKEY_PT_PKE.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_payload_pke_set (GstMIKEYPayload * payload, GstMIKEYCacheType C,
    guint16 data_len, const guint8 * data)
{
  GstMIKEYPayloadPKE *p = (GstMIKEYPayloadPKE *) payload;

  g_return_val_if_fail (payload != NULL, FALSE);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_PKE, FALSE);

  p->C = C;
  p->data_len = data_len;
  INIT_MEMDUP (p->data, data, data_len);

  return TRUE;
}

static gboolean
gst_mikey_payload_pke_dispose (GstMIKEYPayloadPKE * payload)
{
  FREE_MEMDUP (payload->data);

  return TRUE;
}

static GstMIKEYPayloadPKE *
gst_mikey_payload_pke_copy (const GstMIKEYPayloadPKE * payload)
{
  GstMIKEYPayloadPKE *copy = g_memdup2 (payload, sizeof (GstMIKEYPayloadPKE));
  gst_mikey_payload_pke_set (&copy->pt, payload->C, payload->data_len,
      payload->data);
  return copy;
}

/* DH data payload (DH) */
/* Signature payload (SIGN) */

/* Timestamp payload (T) */
static guint
get_ts_len (GstMIKEYTSType type)
{
  guint len;

  switch (type) {
    case GST_MIKEY_TS_TYPE_NTP_UTC:
    case GST_MIKEY_TS_TYPE_NTP:
      len = 8;
      break;
    case GST_MIKEY_TS_TYPE_COUNTER:
      len = 4;
      break;
    default:
      len = -1;
      break;
  }
  return len;
}

/**
 * gst_mikey_payload_t_set:
 * @payload: a #GstMIKEYPayload
 * @type: the #GstMIKEYTSType
 * @ts_value: (array): the timestamp value
 *
 * Set the timestamp in a %GST_MIKEY_PT_T @payload.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_payload_t_set (GstMIKEYPayload * payload,
    GstMIKEYTSType type, const guint8 * ts_value)
{
  GstMIKEYPayloadT *p = (GstMIKEYPayloadT *) payload;
  guint ts_len;

  g_return_val_if_fail (payload != NULL, FALSE);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_T, FALSE);

  if ((ts_len = get_ts_len (type)) == -1)
    return FALSE;

  p->type = type;
  INIT_MEMDUP (p->ts_value, ts_value, ts_len);

  return TRUE;
}

static gboolean
gst_mikey_payload_t_dispose (GstMIKEYPayloadT * payload)
{
  FREE_MEMDUP (payload->ts_value);

  return TRUE;
}

static GstMIKEYPayloadT *
gst_mikey_payload_t_copy (const GstMIKEYPayloadT * payload)
{
  GstMIKEYPayloadT *copy = g_memdup2 (payload, sizeof (GstMIKEYPayloadT));
  gst_mikey_payload_t_set (&copy->pt, payload->type, payload->ts_value);
  return copy;
}

/* ID payload (ID) */
/* Certificate Payload (CERT) */
/* Cert hash payload (CHASH)*/
/* Ver msg payload (V) */
/* Security Policy payload (SP)*/
static void
param_clear (GstMIKEYPayloadSPParam * param)
{
  FREE_MEMDUP (param->val);
}

/**
 * gst_mikey_payload_sp_set:
 * @payload: a #GstMIKEYPayload
 * @policy: the policy number
 * @proto: a #GstMIKEYSecProto
 *
 * Set the Security Policy parameters for @payload.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_payload_sp_set (GstMIKEYPayload * payload,
    guint policy, GstMIKEYSecProto proto)
{
  GstMIKEYPayloadSP *p = (GstMIKEYPayloadSP *) payload;

  g_return_val_if_fail (payload != NULL, FALSE);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_SP, FALSE);

  p->policy = policy;
  p->proto = proto;
  INIT_ARRAY (p->params, GstMIKEYPayloadSPParam, param_clear);

  return TRUE;
}

static gboolean
gst_mikey_payload_sp_dispose (GstMIKEYPayloadSP * payload)
{
  FREE_ARRAY (payload->params);

  return TRUE;
}

static GstMIKEYPayloadSP *
gst_mikey_payload_sp_copy (const GstMIKEYPayloadSP * payload)
{
  guint i, len;
  GstMIKEYPayloadSP *copy = g_memdup2 (payload, sizeof (GstMIKEYPayloadSP));
  gst_mikey_payload_sp_set (&copy->pt, payload->policy, payload->proto);
  len = payload->params->len;
  for (i = 0; i < len; i++) {
    GstMIKEYPayloadSPParam *param = &g_array_index (payload->params,
        GstMIKEYPayloadSPParam, i);
    gst_mikey_payload_sp_add_param (&copy->pt, param->type, param->len,
        param->val);
  }
  return copy;
}

/**
 * gst_mikey_payload_sp_get_n_params:
 * @payload: a #GstMIKEYPayload
 *
 * Get the number of security policy parameters in a %GST_MIKEY_PT_SP
 * @payload.
 *
 * Returns: the number of parameters in @payload
 *
 * Since: 1.4
 */
guint
gst_mikey_payload_sp_get_n_params (const GstMIKEYPayload * payload)
{
  GstMIKEYPayloadSP *p = (GstMIKEYPayloadSP *) payload;

  g_return_val_if_fail (payload != NULL, 0);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_SP, 0);

  return p->params->len;

}

/**
 * gst_mikey_payload_sp_get_param:
 * @payload: a #GstMIKEYPayload
 * @idx: an index
 *
 * Get the Security Policy parameter in a %GST_MIKEY_PT_SP @payload
 * at @idx.
 *
 * Returns: (transfer none) (nullable): the #GstMIKEYPayloadSPParam at @idx in @payload
 *
 * Since: 1.4
 */
const GstMIKEYPayloadSPParam *
gst_mikey_payload_sp_get_param (const GstMIKEYPayload * payload, guint idx)
{
  GstMIKEYPayloadSP *p = (GstMIKEYPayloadSP *) payload;

  g_return_val_if_fail (payload != NULL, NULL);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_SP, NULL);

  if (p->params->len <= idx)
    return NULL;

  return &g_array_index (p->params, GstMIKEYPayloadSPParam, idx);
}

/**
 * gst_mikey_payload_sp_remove_param:
 * @payload: a #GstMIKEYPayload
 * @idx: an index
 *
 * Remove the Security Policy parameters from a %GST_MIKEY_PT_SP
 * @payload at @idx.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_payload_sp_remove_param (GstMIKEYPayload * payload, guint idx)
{
  GstMIKEYPayloadSP *p = (GstMIKEYPayloadSP *) payload;

  g_return_val_if_fail (payload != NULL, FALSE);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_SP, FALSE);
  g_return_val_if_fail (p->params->len > idx, FALSE);

  g_array_remove_index (p->params, idx);

  return TRUE;
}

/**
 * gst_mikey_payload_sp_add_param:
 * @payload: a #GstMIKEYPayload
 * @type: a type
 * @len: a length
 * @val: (array length=len): @len bytes of data
 *
 * Add a new parameter to the %GST_MIKEY_PT_SP @payload with @type, @len
 * and @val.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_payload_sp_add_param (GstMIKEYPayload * payload,
    guint8 type, guint8 len, const guint8 * val)
{
  GstMIKEYPayloadSPParam param = { 0 };
  GstMIKEYPayloadSP *p = (GstMIKEYPayloadSP *) payload;

  g_return_val_if_fail (payload != NULL, FALSE);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_SP, FALSE);

  param.type = type;
  param.len = len;
  INIT_MEMDUP (param.val, val, len);

  g_array_append_val (p->params, param);

  return TRUE;
}

/* RAND payload (RAND) */
/**
 * gst_mikey_payload_rand_set:
 * @payload: a #GstMIKEYPayload
 * @len: the length of @rand
 * @rand: (array length=len): random values
 *
 * Set the random values in a %GST_MIKEY_PT_RAND @payload.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_payload_rand_set (GstMIKEYPayload * payload, guint8 len,
    const guint8 * rand)
{
  GstMIKEYPayloadRAND *p = (GstMIKEYPayloadRAND *) payload;

  g_return_val_if_fail (payload != NULL, FALSE);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_RAND, FALSE);

  p->len = len;
  INIT_MEMDUP (p->rand, rand, len);

  return TRUE;
}

static gboolean
gst_mikey_payload_rand_dispose (GstMIKEYPayloadRAND * payload)
{
  FREE_MEMDUP (payload->rand);

  return TRUE;
}

static GstMIKEYPayloadRAND *
gst_mikey_payload_rand_copy (const GstMIKEYPayloadRAND * payload)
{
  GstMIKEYPayloadRAND *copy = g_memdup2 (payload, sizeof (GstMIKEYPayloadRAND));
  gst_mikey_payload_rand_set (&copy->pt, payload->len, payload->rand);
  return copy;
}



/* Error payload (ERR) */
/* Key data sub-payload */

/**
 * gst_mikey_payload_key_data_set_key:
 * @payload: a #GstMIKEYPayload
 * @key_type: a #GstMIKEYKeyDataType
 * @key_len: the length of @key_data
 * @key_data: (array length=key_len): the key of type @key_type
 *
 * Set @key_len bytes of @key_data of type @key_type as the key for the
 * %GST_MIKEY_PT_KEY_DATA @payload.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_payload_key_data_set_key (GstMIKEYPayload * payload,
    GstMIKEYKeyDataType key_type, guint16 key_len, const guint8 * key_data)
{
  GstMIKEYPayloadKeyData *p = (GstMIKEYPayloadKeyData *) payload;

  g_return_val_if_fail (payload != NULL, FALSE);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_KEY_DATA, FALSE);
  g_return_val_if_fail (key_len > 0 && key_data != NULL, FALSE);

  p->key_type = key_type;
  p->key_len = key_len;
  INIT_MEMDUP (p->key_data, key_data, key_len);

  return TRUE;
}

/**
 * gst_mikey_payload_key_data_set_salt:
 * @payload: a #GstMIKEYPayload
 * @salt_len: the length of @salt_data
 * @salt_data: (array length=salt_len) (allow-none): the salt
 *
 * Set the salt key data. If @salt_len is 0 and @salt_data is %NULL, the
 * salt data will be removed.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_payload_key_data_set_salt (GstMIKEYPayload * payload,
    guint16 salt_len, const guint8 * salt_data)
{
  GstMIKEYPayloadKeyData *p = (GstMIKEYPayloadKeyData *) payload;

  g_return_val_if_fail (payload != NULL, FALSE);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_KEY_DATA, FALSE);
  g_return_val_if_fail ((salt_len == 0 && salt_data == NULL) ||
      (salt_len > 0 && salt_data != NULL), FALSE);

  p->salt_len = salt_len;
  INIT_MEMDUP (p->salt_data, salt_data, salt_len);

  return TRUE;
}

/* Key validity data */

/**
 * gst_mikey_payload_key_data_set_spi:
 * @payload: a #GstMIKEYPayload
 * @spi_len: the length of @spi_data
 * @spi_data: (array length=spi_len): the SPI/MKI data
 *
 * Set the SPI/MKI validity in the %GST_MIKEY_PT_KEY_DATA @payload.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_payload_key_data_set_spi (GstMIKEYPayload * payload,
    guint8 spi_len, const guint8 * spi_data)
{
  GstMIKEYPayloadKeyData *p = (GstMIKEYPayloadKeyData *) payload;

  g_return_val_if_fail (payload != NULL, FALSE);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_KEY_DATA, FALSE);
  g_return_val_if_fail ((spi_len == 0 && spi_data == NULL) ||
      (spi_len > 0 && spi_data != NULL), FALSE);

  p->kv_type = GST_MIKEY_KV_SPI;
  p->kv_len[0] = spi_len;
  INIT_MEMDUP (p->kv_data[0], spi_data, spi_len);
  p->kv_len[1] = 0;
  FREE_MEMDUP (p->kv_data[1]);

  return TRUE;
}

/**
 * gst_mikey_payload_key_data_set_interval:
 * @payload: a #GstMIKEYPayload
 * @vf_len: the length of @vf_data
 * @vf_data: (array length=vf_len): the Valid From data
 * @vt_len: the length of @vt_data
 * @vt_data: (array length=vt_len): the Valid To data
 *
 * Set the key validity period in the %GST_MIKEY_PT_KEY_DATA @payload.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_payload_key_data_set_interval (GstMIKEYPayload * payload,
    guint8 vf_len, const guint8 * vf_data, guint8 vt_len,
    const guint8 * vt_data)
{
  GstMIKEYPayloadKeyData *p = (GstMIKEYPayloadKeyData *) payload;

  g_return_val_if_fail (payload != NULL, FALSE);
  g_return_val_if_fail (payload->type == GST_MIKEY_PT_KEY_DATA, FALSE);
  g_return_val_if_fail ((vf_len == 0 && vf_data == NULL) ||
      (vf_len > 0 && vf_data != NULL), FALSE);
  g_return_val_if_fail ((vt_len == 0 && vt_data == NULL) ||
      (vt_len > 0 && vt_data != NULL), FALSE);

  p->kv_type = GST_MIKEY_KV_INTERVAL;
  p->kv_len[0] = vf_len;
  INIT_MEMDUP (p->kv_data[0], vf_data, vf_len);
  p->kv_len[1] = vt_len;
  INIT_MEMDUP (p->kv_data[1], vt_data, vt_len);

  return TRUE;
}

static gboolean
gst_mikey_payload_key_data_dispose (GstMIKEYPayloadKeyData * payload)
{
  FREE_MEMDUP (payload->key_data);
  FREE_MEMDUP (payload->salt_data);
  FREE_MEMDUP (payload->kv_data[0]);
  FREE_MEMDUP (payload->kv_data[1]);

  return TRUE;
}

static GstMIKEYPayloadKeyData *
gst_mikey_payload_key_data_copy (const GstMIKEYPayloadKeyData * payload)
{
  GstMIKEYPayloadKeyData *copy =
      g_memdup2 (payload, sizeof (GstMIKEYPayloadKeyData));
  gst_mikey_payload_key_data_set_key (&copy->pt, payload->key_type,
      payload->key_len, payload->key_data);
  gst_mikey_payload_key_data_set_salt (&copy->pt, payload->salt_len,
      payload->salt_data);
  if (payload->kv_type == GST_MIKEY_KV_SPI)
    gst_mikey_payload_key_data_set_spi (&copy->pt, payload->kv_len[0],
        payload->kv_data[0]);
  else if (payload->kv_type == GST_MIKEY_KV_INTERVAL)
    gst_mikey_payload_key_data_set_interval (&copy->pt, payload->kv_len[0],
        payload->kv_data[0], payload->kv_len[1], payload->kv_data[1]);
  else {
    FREE_MEMDUP (copy->kv_data[0]);
    FREE_MEMDUP (copy->kv_data[1]);
  }
  return copy;
}

/* General Extension Payload */

static void
mikey_payload_free (GstMIKEYPayload * payload)
{
  g_free (payload);
}


/**
 * gst_mikey_payload_new:
 * @type: a #GstMIKEYPayloadType
 *
 * Make a new #GstMIKEYPayload with @type.
 *
 * Returns: (transfer full) (nullable): a new #GstMIKEYPayload or %NULL on failure.
 *
 * Since: 1.4
 */
GstMIKEYPayload *
gst_mikey_payload_new (GstMIKEYPayloadType type)
{
  guint len = 0;
  GstMIKEYPayload *result;
  GstMiniObjectCopyFunction copy;
  GstMiniObjectDisposeFunction clear;

  switch (type) {
    case GST_MIKEY_PT_KEMAC:
      len = sizeof (GstMIKEYPayloadKEMAC);
      clear = (GstMiniObjectDisposeFunction) gst_mikey_payload_kemac_dispose;
      copy = (GstMiniObjectCopyFunction) gst_mikey_payload_kemac_copy;
      break;
    case GST_MIKEY_PT_T:
      len = sizeof (GstMIKEYPayloadT);
      clear = (GstMiniObjectDisposeFunction) gst_mikey_payload_t_dispose;
      copy = (GstMiniObjectCopyFunction) gst_mikey_payload_t_copy;
      break;
    case GST_MIKEY_PT_PKE:
      len = sizeof (GstMIKEYPayloadPKE);
      clear = (GstMiniObjectDisposeFunction) gst_mikey_payload_pke_dispose;
      copy = (GstMiniObjectCopyFunction) gst_mikey_payload_pke_copy;
      break;
    case GST_MIKEY_PT_DH:
    case GST_MIKEY_PT_SIGN:
    case GST_MIKEY_PT_ID:
    case GST_MIKEY_PT_CERT:
    case GST_MIKEY_PT_CHASH:
    case GST_MIKEY_PT_V:
    case GST_MIKEY_PT_SP:
      len = sizeof (GstMIKEYPayloadSP);
      clear = (GstMiniObjectDisposeFunction) gst_mikey_payload_sp_dispose;
      copy = (GstMiniObjectCopyFunction) gst_mikey_payload_sp_copy;
      break;
    case GST_MIKEY_PT_RAND:
      len = sizeof (GstMIKEYPayloadRAND);
      clear = (GstMiniObjectDisposeFunction) gst_mikey_payload_rand_dispose;
      copy = (GstMiniObjectCopyFunction) gst_mikey_payload_rand_copy;
      break;
    case GST_MIKEY_PT_ERR:
      break;
    case GST_MIKEY_PT_KEY_DATA:
      len = sizeof (GstMIKEYPayloadKeyData);
      clear = (GstMiniObjectDisposeFunction) gst_mikey_payload_key_data_dispose;
      copy = (GstMiniObjectCopyFunction) gst_mikey_payload_key_data_copy;
      break;
    case GST_MIKEY_PT_GEN_EXT:
    case GST_MIKEY_PT_LAST:
      break;
  }
  if (len == 0)
    return NULL;

  result = g_malloc0 (len);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (result),
      0, GST_TYPE_MIKEY_PAYLOAD, copy, clear,
      (GstMiniObjectFreeFunction) mikey_payload_free);
  result->type = type;
  result->len = len;

  return result;
}

static GstMIKEYMessage *
mikey_message_copy (GstMIKEYMessage * msg)
{
  GstMIKEYMessage *copy;
  guint i, len;

  copy = gst_mikey_message_new ();

  gst_mikey_message_set_info (copy, msg->version, msg->type, msg->V,
      msg->prf_func, msg->CSB_id, msg->map_type);

  len = msg->map_info->len;
  for (i = 0; i < len; i++) {
    const GstMIKEYMapSRTP *srtp = gst_mikey_message_get_cs_srtp (msg, i);
    gst_mikey_message_add_cs_srtp (copy, srtp->policy, srtp->ssrc, srtp->roc);
  }

  len = msg->payloads->len;
  for (i = 0; i < len; i++) {
    const GstMIKEYPayload *pay = gst_mikey_message_get_payload (msg, i);
    gst_mikey_message_add_payload (copy, gst_mikey_payload_copy (pay));
  }
  return copy;
}

static void
mikey_message_free (GstMIKEYMessage * msg)
{
  FREE_ARRAY (msg->map_info);
  FREE_ARRAY (msg->payloads);

  g_free (msg);
}

/**
 * gst_mikey_message_new:
 *
 * Make a new MIKEY message.
 *
 * Returns: (transfer full): a new #GstMIKEYMessage on success
 *
 * Since: 1.4
 */
GstMIKEYMessage *
gst_mikey_message_new (void)
{
  GstMIKEYMessage *result;

  result = g_new0 (GstMIKEYMessage, 1);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (result),
      0, GST_TYPE_MIKEY_MESSAGE,
      (GstMiniObjectCopyFunction) mikey_message_copy, NULL,
      (GstMiniObjectFreeFunction) mikey_message_free);

  INIT_ARRAY (result->map_info, GstMIKEYMapSRTP, NULL);
  INIT_ARRAY (result->payloads, GstMIKEYPayload *, payload_destroy);

  return result;
}

/**
 * gst_mikey_message_new_from_bytes:
 * @bytes: a #GBytes
 * @info: a #GstMIKEYDecryptInfo
 * @error: a #GError
 *
 * Make a new #GstMIKEYMessage from @bytes.
 *
 * Returns: (transfer full): a new #GstMIKEYMessage
 *
 * Since: 1.4
 */
GstMIKEYMessage *
gst_mikey_message_new_from_bytes (GBytes * bytes, GstMIKEYDecryptInfo * info,
    GError ** error)
{
  gconstpointer data;
  gsize size;

  g_return_val_if_fail (bytes != NULL, NULL);

  data = g_bytes_get_data (bytes, &size);
  return gst_mikey_message_new_from_data (data, size, info, error);
}

/**
 * gst_mikey_message_set_info:
 * @msg: a #GstMIKEYMessage
 * @version: a version
 * @type: a #GstMIKEYType
 * @V: verify flag
 * @prf_func: the #GstMIKEYPRFFunc function to use
 * @CSB_id: the Crypto Session Bundle id
 * @map_type: the #GstMIKEYMapType
 *
 * Set the information in @msg.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_set_info (GstMIKEYMessage * msg, guint8 version,
    GstMIKEYType type, gboolean V, GstMIKEYPRFFunc prf_func, guint32 CSB_id,
    GstMIKEYMapType map_type)
{
  g_return_val_if_fail (msg != NULL, FALSE);

  msg->version = version;
  msg->type = type;
  msg->V = V;
  msg->prf_func = prf_func;
  msg->CSB_id = CSB_id;
  msg->map_type = map_type;

  return TRUE;
}

/**
 * gst_mikey_message_get_n_cs:
 * @msg: a #GstMIKEYMessage
 *
 * Get the number of crypto sessions in @msg.
 *
 * Returns: the number of crypto sessions
 *
 * Since: 1.4
 */
guint
gst_mikey_message_get_n_cs (const GstMIKEYMessage * msg)
{
  g_return_val_if_fail (msg != NULL, 0);

  return msg->map_info->len;
}

/**
 * gst_mikey_message_get_cs_srtp:
 * @msg: a #GstMIKEYMessage
 * @idx: an index
 *
 * Get the policy information of @msg at @idx.
 *
 * Returns: (transfer none) (nullable): a #GstMIKEYMapSRTP
 *
 * Since: 1.4
 */
const GstMIKEYMapSRTP *
gst_mikey_message_get_cs_srtp (const GstMIKEYMessage * msg, guint idx)
{
  g_return_val_if_fail (msg != NULL, NULL);
  g_return_val_if_fail (msg->map_type == GST_MIKEY_MAP_TYPE_SRTP, NULL);

  if (msg->map_info->len <= idx)
    return NULL;

  return &g_array_index (msg->map_info, GstMIKEYMapSRTP, idx);
}

/**
 * gst_mikey_message_insert_cs_srtp:
 * @msg: a #GstMIKEYMessage
 * @idx: the index to insert at
 * @map: the map info
 *
 * Insert a Crypto Session map for SRTP in @msg at @idx
 *
 * When @idx is -1, the policy will be appended.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_insert_cs_srtp (GstMIKEYMessage * msg, gint idx,
    const GstMIKEYMapSRTP * map)
{
  g_return_val_if_fail (msg != NULL, FALSE);
  g_return_val_if_fail (msg->map_type == GST_MIKEY_MAP_TYPE_SRTP, FALSE);
  g_return_val_if_fail (map != NULL, FALSE);
  g_return_val_if_fail (idx == -1 || msg->map_info->len > idx, FALSE);

  if (idx == -1)
    g_array_append_val (msg->map_info, *map);
  else
    g_array_insert_val (msg->map_info, idx, *map);

  return TRUE;
}

/**
 * gst_mikey_message_replace_cs_srtp:
 * @msg: a #GstMIKEYMessage
 * @idx: the index to insert at
 * @map: the map info
 *
 * Replace a Crypto Session map for SRTP in @msg at @idx with @map.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_replace_cs_srtp (GstMIKEYMessage * msg, gint idx,
    const GstMIKEYMapSRTP * map)
{
  g_return_val_if_fail (msg != NULL, FALSE);
  g_return_val_if_fail (msg->map_type == GST_MIKEY_MAP_TYPE_SRTP, FALSE);
  g_return_val_if_fail (map != NULL, FALSE);
  g_return_val_if_fail (msg->map_info->len > idx, FALSE);

  g_array_index (msg->map_info, GstMIKEYMapSRTP, idx) = *map;

  return TRUE;
}

/**
 * gst_mikey_message_remove_cs_srtp:
 * @msg: a #GstMIKEYMessage
 * @idx: the index to remove
 *
 * Remove the SRTP policy at @idx.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_remove_cs_srtp (GstMIKEYMessage * msg, gint idx)
{
  g_return_val_if_fail (msg != NULL, FALSE);
  g_return_val_if_fail (msg->map_type == GST_MIKEY_MAP_TYPE_SRTP, FALSE);
  g_return_val_if_fail (msg->map_info->len > idx, FALSE);

  g_array_remove_index (msg->map_info, idx);

  return TRUE;
}

/**
 * gst_mikey_message_add_cs_srtp:
 * @msg: a #GstMIKEYMessage
 * @policy: The security policy applied for the stream with @ssrc
 * @ssrc: the SSRC that must be used for the stream
 * @roc: current rollover counter
 *
 * Add a Crypto policy for SRTP to @msg.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_add_cs_srtp (GstMIKEYMessage * msg, guint8 policy,
    guint32 ssrc, guint32 roc)
{
  GstMIKEYMapSRTP val;

  g_return_val_if_fail (msg != NULL, FALSE);
  g_return_val_if_fail (msg->map_type == GST_MIKEY_MAP_TYPE_SRTP, FALSE);

  val.policy = policy;
  val.ssrc = ssrc;
  val.roc = roc;

  return gst_mikey_message_insert_cs_srtp (msg, -1, &val);
}

/* adding/retrieving payloads */
/**
 * gst_mikey_message_get_n_payloads:
 * @msg: a #GstMIKEYMessage
 *
 * Get the number of payloads in @msg.
 *
 * Returns: the number of payloads in @msg
 *
 * Since: 1.4
 */
guint
gst_mikey_message_get_n_payloads (const GstMIKEYMessage * msg)
{
  g_return_val_if_fail (msg != NULL, 0);

  return msg->payloads->len;
}

/**
 * gst_mikey_message_get_payload:
 * @msg: a #GstMIKEYMessage
 * @idx: an index
 *
 * Get the #GstMIKEYPayload at @idx in @msg
 *
 * Returns: (transfer none) (nullable): the #GstMIKEYPayload at @idx. The payload
 * remains valid for as long as it is part of @msg.
 *
 * Since: 1.4
 */
const GstMIKEYPayload *
gst_mikey_message_get_payload (const GstMIKEYMessage * msg, guint idx)
{
  g_return_val_if_fail (msg != NULL, NULL);

  if (idx >= msg->payloads->len)
    return NULL;

  return g_array_index (msg->payloads, GstMIKEYPayload *, idx);
}

/**
 * gst_mikey_message_find_payload:
 * @msg: a #GstMIKEYMessage
 * @type: a #GstMIKEYPayloadType
 * @nth: payload to find
 *
 * Find the @nth occurrence of the payload with @type in @msg.
 *
 * Returns: (transfer none) (nullable): the @nth #GstMIKEYPayload of @type.
 *
 * Since: 1.4
 */
const GstMIKEYPayload *
gst_mikey_message_find_payload (const GstMIKEYMessage * msg,
    GstMIKEYPayloadType type, guint idx)
{
  guint i, len, count;

  count = 0;
  len = msg->payloads->len;
  for (i = 0; i < len; i++) {
    GstMIKEYPayload *payload =
        g_array_index (msg->payloads, GstMIKEYPayload *, i);

    if (payload->type == type) {
      if (count == idx)
        return payload;

      count++;
    }
  }
  return NULL;
}

/**
 * gst_mikey_message_remove_payload:
 * @msg: a #GstMIKEYMessage
 * @idx: an index
 *
 * Remove the payload in @msg at @idx
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_remove_payload (GstMIKEYMessage * msg, guint idx)
{
  g_return_val_if_fail (msg != NULL, FALSE);
  g_return_val_if_fail (msg->payloads->len > idx, FALSE);

  g_array_remove_index (msg->payloads, idx);

  return TRUE;
}

/**
 * gst_mikey_message_insert_payload:
 * @msg: a #GstMIKEYMessage
 * @idx: an index
 * @payload: (transfer full): a #GstMIKEYPayload
 *
 * Insert the @payload at index @idx in @msg. If @idx is -1, the payload
 * will be appended to @msg.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_insert_payload (GstMIKEYMessage * msg, guint idx,
    GstMIKEYPayload * payload)
{
  g_return_val_if_fail (msg != NULL, FALSE);
  g_return_val_if_fail (payload != NULL, FALSE);
  g_return_val_if_fail (idx == -1 || msg->payloads->len > idx, FALSE);

  if (idx == -1)
    g_array_append_val (msg->payloads, payload);
  else
    g_array_insert_val (msg->payloads, idx, payload);

  return TRUE;
}

/**
 * gst_mikey_message_add_payload:
 * @msg: a #GstMIKEYMessage
 * @payload: (transfer full): a #GstMIKEYPayload
 *
 * Add a new payload to @msg.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_add_payload (GstMIKEYMessage * msg, GstMIKEYPayload * payload)
{
  return gst_mikey_message_insert_payload (msg, -1, payload);
}

/**
 * gst_mikey_message_replace_payload:
 * @msg: a #GstMIKEYMessage
 * @idx: an index
 * @payload: (transfer full): a #GstMIKEYPayload
 *
 * Replace the payload at @idx in @msg with @payload.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_replace_payload (GstMIKEYMessage * msg, guint idx,
    GstMIKEYPayload * payload)
{
  GstMIKEYPayload *p;

  g_return_val_if_fail (msg != NULL, FALSE);
  g_return_val_if_fail (payload != NULL, FALSE);
  g_return_val_if_fail (msg->payloads->len > idx, FALSE);

  p = g_array_index (msg->payloads, GstMIKEYPayload *, idx);
  gst_mikey_payload_unref (p);
  g_array_index (msg->payloads, GstMIKEYPayload *, idx) = payload;

  return TRUE;
}

/**
 * gst_mikey_message_add_pke:
 * @msg: a #GstMIKEYMessage
 * @C: envelope key cache indicator
 * @data_len: the length of @data
 * @data: (array length=data_len): the encrypted envelope key
 *
 * Add a new PKE payload to @msg with the given parameters.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_add_pke (GstMIKEYMessage * msg, GstMIKEYCacheType C,
    guint16 data_len, const guint8 * data)
{
  GstMIKEYPayload *p;

  g_return_val_if_fail (msg != NULL, FALSE);

  p = gst_mikey_payload_new (GST_MIKEY_PT_PKE);
  if (!gst_mikey_payload_pke_set (p, C, data_len, data)) {
    gst_mikey_payload_unref (p);
    return FALSE;
  }

  return gst_mikey_message_insert_payload (msg, -1, p);
}

/**
 * gst_mikey_message_add_t:
 * @msg: a #GstMIKEYMessage
 * @type: specifies the timestamp type used
 * @ts_value: (array): The timestamp value of the specified @type
 *
 * Add a new T payload to @msg with the given parameters.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_add_t (GstMIKEYMessage * msg, GstMIKEYTSType type,
    const guint8 * ts_value)
{
  GstMIKEYPayload *p;

  g_return_val_if_fail (msg != NULL, FALSE);

  p = gst_mikey_payload_new (GST_MIKEY_PT_T);
  if (!gst_mikey_payload_t_set (p, type, ts_value)) {
    gst_mikey_payload_unref (p);
    return FALSE;
  }

  return gst_mikey_message_insert_payload (msg, -1, p);
}

/**
 * gst_mikey_message_add_t_now_ntp_utc:
 * @msg: a #GstMIKEYMessage
 *
 * Add a new T payload to @msg that contains the current time
 * in NTP-UTC format.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_add_t_now_ntp_utc (GstMIKEYMessage * msg)
{
  gint64 now;
  guint64 ntptime;
  guint8 bytes[8];

  now = g_get_real_time ();

  /* convert clock time to NTP time. upper 32 bits should contain the seconds
   * and the lower 32 bits, the fractions of a second. */
  ntptime = gst_util_uint64_scale (now, (G_GINT64_CONSTANT (1) << 32), 1000000);
  /* conversion from UNIX timestamp (seconds since 1970) to NTP (seconds
   * since 1900). */
  ntptime += (G_GUINT64_CONSTANT (2208988800) << 32);
  GST_WRITE_UINT64_BE (bytes, ntptime);

  return gst_mikey_message_add_t (msg, GST_MIKEY_TS_TYPE_NTP_UTC, bytes);
}

/**
 * gst_mikey_message_add_rand:
 * @msg: a #GstMIKEYMessage
 * @len: the length of @rand
 * @rand: (array length=len): random data
 *
 * Add a new RAND payload to @msg with the given parameters.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_add_rand (GstMIKEYMessage * msg, guint8 len,
    const guint8 * rand)
{
  GstMIKEYPayload *p;

  g_return_val_if_fail (msg != NULL, FALSE);
  g_return_val_if_fail (len != 0 && rand != NULL, FALSE);

  p = gst_mikey_payload_new (GST_MIKEY_PT_RAND);
  if (!gst_mikey_payload_rand_set (p, len, rand)) {
    gst_mikey_payload_unref (p);
    return FALSE;
  }

  return gst_mikey_message_insert_payload (msg, -1, p);
}

/**
 * gst_mikey_message_add_rand_len:
 * @msg: a #GstMIKEYMessage
 * @len: length
 *
 * Add a new RAND payload to @msg with @len random bytes.
 *
 * Returns: %TRUE on success
 *
 * Since: 1.4
 */
gboolean
gst_mikey_message_add_rand_len (GstMIKEYMessage * msg, guint8 len)
{
  GstMIKEYPayloadRAND *p;
  guint i;

  p = (GstMIKEYPayloadRAND *) gst_mikey_payload_new (GST_MIKEY_PT_RAND);
  p->len = len;
  p->rand = g_malloc (len);
  for (i = 0; i < len; i++)
    p->rand[i] = g_random_int_range (0, 256);

  return gst_mikey_message_add_payload (msg, &p->pt);
}

#define ENSURE_SIZE(n)                          \
G_STMT_START {                                  \
  guint offset = data - arr->data;              \
  g_byte_array_set_size (arr, offset + n);      \
  data = arr->data + offset;                    \
} G_STMT_END
static guint
payloads_to_bytes (GArray * payloads, GByteArray * arr, guint8 ** ptr,
    guint offset, GstMIKEYEncryptInfo * info, GError ** error)
{
  guint i, n_payloads, len, start, size;
  guint8 *data;
  GstMIKEYPayload *next_payload;

  len = arr->len;
  start = *ptr - arr->data;
  data = *ptr + offset;

  n_payloads = payloads->len;

  for (i = 0; i < n_payloads; i++) {
    GstMIKEYPayload *payload = g_array_index (payloads, GstMIKEYPayload *, i);

    if (i + 1 < n_payloads)
      next_payload = g_array_index (payloads, GstMIKEYPayload *, i + 1);
    else
      next_payload = NULL;

    switch (payload->type) {
      case GST_MIKEY_PT_KEMAC:
      {
        GstMIKEYPayloadKEMAC *p = (GstMIKEYPayloadKEMAC *) payload;
        guint enc_len;
        guint mac_len;

        if ((mac_len = get_mac_len (p->mac_alg)) == -1)
          break;

        /*                  1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Next payload  ! Encr alg      ! Encr data len                 !
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * !                        Encr data                              ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Mac alg       !        MAC                                    ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        ENSURE_SIZE (4);
        data[0] = next_payload ? next_payload->type : GST_MIKEY_PT_LAST;
        data[1] = p->enc_alg;
        enc_len =
            payloads_to_bytes (p->subpayloads, arr, &data, 4, info, error);
        /* FIXME, encrypt data here */
        GST_WRITE_UINT16_BE (&data[2], enc_len);
        data += enc_len;
        ENSURE_SIZE (5 + mac_len);
        data[4] = p->mac_alg;
        /* FIXME, do mac here */
        data += 5 + mac_len;
        break;
      }
      case GST_MIKEY_PT_T:
      {
        GstMIKEYPayloadT *p = (GstMIKEYPayloadT *) payload;
        guint ts_len;

        if ((ts_len = get_ts_len (p->type)) == -1)
          break;

        /*                      1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Next Payload  !   TS type     ! TS value                      ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        ENSURE_SIZE (2 + ts_len);
        data[0] = next_payload ? next_payload->type : GST_MIKEY_PT_LAST;
        data[1] = p->type;
        memcpy (&data[2], p->ts_value, ts_len);
        data += 2 + ts_len;
        break;
      }
      case GST_MIKEY_PT_PKE:
      {
        guint16 clen;
        GstMIKEYPayloadPKE *p = (GstMIKEYPayloadPKE *) payload;
        /*                      1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Next Payload  ! C ! Data len                  ! Data          ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        ENSURE_SIZE (3 + p->data_len);
        data[0] = next_payload ? next_payload->type : GST_MIKEY_PT_LAST;
        clen = (p->C << 14) || (p->data_len & 0x3fff);
        GST_WRITE_UINT16_BE (&data[1], clen);
        memcpy (&data[3], p->data, p->data_len);
        data += 3 + p->data_len;
        break;
      }
      case GST_MIKEY_PT_DH:
      case GST_MIKEY_PT_SIGN:
      case GST_MIKEY_PT_ID:
      case GST_MIKEY_PT_CERT:
      case GST_MIKEY_PT_CHASH:
      case GST_MIKEY_PT_V:
        break;
      case GST_MIKEY_PT_SP:
      {
        GstMIKEYPayloadSP *p = (GstMIKEYPayloadSP *) payload;
        guint len, plen, i;

        plen = 0;
        len = p->params->len;
        for (i = 0; i < len; i++) {
          GstMIKEYPayloadSPParam *param = &g_array_index (p->params,
              GstMIKEYPayloadSPParam, i);
          plen += 2 + param->len;
        }
        /*                      1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Next payload  ! Policy no     ! Prot type     ! Policy param  ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ~ length (cont) ! Policy param                                  ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        ENSURE_SIZE (5 + plen);
        data[0] = next_payload ? next_payload->type : GST_MIKEY_PT_LAST;
        data[1] = p->policy;
        data[2] = p->proto;
        GST_WRITE_UINT16_BE (&data[3], plen);
        data += 5;
        for (i = 0; i < len; i++) {
          GstMIKEYPayloadSPParam *param = &g_array_index (p->params,
              GstMIKEYPayloadSPParam, i);
          /*                     1                   2                   3
           * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           * ! Type          ! Length        ! Value                         ~
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           */
          data[0] = param->type;
          data[1] = param->len;
          memcpy (&data[2], param->val, param->len);
          data += 2 + param->len;
        }
        break;
      }
      case GST_MIKEY_PT_RAND:
      {
        GstMIKEYPayloadRAND *p = (GstMIKEYPayloadRAND *) payload;
        /*                      1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Next payload  ! RAND len      ! RAND                          ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        ENSURE_SIZE (2 + p->len);
        data[0] = next_payload ? next_payload->type : GST_MIKEY_PT_LAST;
        data[1] = p->len;
        memcpy (&data[2], p->rand, p->len);
        data += 2 + p->len;
        break;
      }
      case GST_MIKEY_PT_ERR:
        break;
      case GST_MIKEY_PT_KEY_DATA:
      {
        GstMIKEYPayloadKeyData *p = (GstMIKEYPayloadKeyData *) payload;
        /*                        1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * !  Next Payload ! Type  ! KV    ! Key data len                  !
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * !                         Key data                              ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Salt len (optional)           ! Salt data (optional)          ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * !                        KV data (optional)                     ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        ENSURE_SIZE (4 + p->key_len);
        data[0] = next_payload ? next_payload->type : GST_MIKEY_PT_LAST;
        data[1] =
            ((p->key_type | (p->salt_len ? 1 : 0)) << 4) | (p->kv_type & 0xf);
        GST_WRITE_UINT16_BE (&data[2], p->key_len);
        memcpy (&data[4], p->key_data, p->key_len);
        data += 4 + p->key_len;

        if (p->salt_len > 0) {
          ENSURE_SIZE (2 + p->salt_len);
          GST_WRITE_UINT16_BE (&data[0], p->salt_len);
          memcpy (&data[2], p->salt_data, p->salt_len);
          data += 2 + p->salt_len;
        }
        if (p->kv_type == GST_MIKEY_KV_SPI) {
          /*
           *                      1                   2                   3
           *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           * ! SPI Length    ! SPI                                           ~
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           */
          ENSURE_SIZE (1 + p->kv_len[0]);
          data[0] = p->kv_len[0];
          memcpy (&data[1], p->kv_data[0], p->kv_len[0]);
          data += 1 + p->kv_len[0];
        } else if (p->kv_type == GST_MIKEY_KV_INTERVAL) {
          /*
           *                      1                   2                   3
           *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           * ! VF Length     ! Valid From                                    ~
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           * ! VT Length     ! Valid To (expires)                            ~
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           */
          ENSURE_SIZE (1 + p->kv_len[0]);
          data[0] = p->kv_len[0];
          memcpy (&data[1], p->kv_data[0], p->kv_len[0]);
          data += 1 + p->kv_len[0];
          ENSURE_SIZE (1 + p->kv_len[1]);
          data[0] = p->kv_len[1];
          memcpy (&data[1], p->kv_data[1], p->kv_len[1]);
          data += 1 + p->kv_len[1];
        }
        break;
      }
      case GST_MIKEY_PT_GEN_EXT:
      case GST_MIKEY_PT_LAST:
        break;
    }
  }
  *ptr = arr->data + start;
  size = arr->len - len;

  return size;
}

/**
 * gst_mikey_message_to_bytes:
 * @msg: a #GstMIKEYMessage
 * @info: a #GstMIKEYEncryptInfo
 * @error: a #GError
 *
 * Convert @msg to a #GBytes.
 *
 * Returns: (transfer full): a new #GBytes for @msg.
 *
 * Since: 1.4
 */
GBytes *
gst_mikey_message_to_bytes (GstMIKEYMessage * msg, GstMIKEYEncryptInfo * info,
    GError ** error)
{
  GByteArray *arr = NULL;
  guint8 *data;
  GstMIKEYPayload *next_payload;
  guint i, n_cs;
  arr = g_byte_array_new ();
  data = arr->data;

  if (msg->payloads->len == 0)
    next_payload = NULL;
  else
    next_payload = g_array_index (msg->payloads, GstMIKEYPayload *, 0);

  n_cs = msg->map_info->len;
  /*                      1                   2                   3
   *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   * !  version      !  data type    ! next payload  !V! PRF func    !
   * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   * !                         CSB ID                                !
   * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   * ! #CS           ! CS ID map type! CS ID map info                ~
   * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   */
  ENSURE_SIZE (10 + 9 * n_cs);
  data[0] = msg->version;
  data[1] = msg->type;
  data[2] = next_payload ? next_payload->type : GST_MIKEY_PT_LAST;
  data[3] = (msg->V ? 0x80 : 0x00) | (msg->prf_func & 0x7f);
  GST_WRITE_UINT32_BE (&data[4], msg->CSB_id);
  data[8] = n_cs;
  data[9] = msg->map_type;
  data += 10;

  for (i = 0; i < n_cs; i++) {
    GstMIKEYMapSRTP *info = &g_array_index (msg->map_info, GstMIKEYMapSRTP, i);
    /*                      1                   2                   3
     *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * ! Policy_no_1   ! SSRC_1                                        !
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * ! SSRC_1 (cont) ! ROC_1                                         !
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * ! ROC_1 (cont)  ! Policy_no_2   ! SSRC_2                        !
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * ! SSRC_2 (cont)                 ! ROC_2                         !
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * ! ROC_2 (cont)                  !                               :
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ ...
     */
    data[0] = info->policy;
    GST_WRITE_UINT32_BE (&data[1], info->ssrc);
    GST_WRITE_UINT32_BE (&data[5], info->roc);
    data += 9;
  }

  payloads_to_bytes (msg->payloads, arr, &data, 0, info, error);

  return g_byte_array_free_to_bytes (arr);
}

#undef ENSURE_SIZE

typedef enum
{
  STATE_PSK,
  STATE_PK,
  STATE_KEMAC,
  STATE_OTHER
} ParseState;

#define CHECK_SIZE(n) if (size < (n)) goto short_data;
#define ADVANCE(n) (d += (n), size -= (n));
static gboolean
payloads_from_bytes (ParseState state, GArray * payloads, const guint8 * d,
    gsize size, guint8 next_payload, GstMIKEYDecryptInfo * info,
    GError ** error)
{
  GstMIKEYPayload *p;

  while (next_payload != GST_MIKEY_PT_LAST) {
    p = NULL;
    switch (next_payload) {
      case GST_MIKEY_PT_KEMAC:
      {
        guint mac_len;
        GstMIKEYEncAlg enc_alg;
        guint16 enc_len;
        const guint8 *enc_data;
        GstMIKEYMacAlg mac_alg;
        guint8 np;
        /*                  1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Next payload  ! Encr alg      ! Encr data len                 !
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * !                        Encr data                              ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Mac alg       !        MAC                                    ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        CHECK_SIZE (5);
        next_payload = d[0];
        enc_alg = d[1];
        enc_len = GST_READ_UINT16_BE (&d[2]);
        CHECK_SIZE (5 + enc_len);
        enc_data = &d[4];
        /* FIXME, decrypt data */
        ADVANCE (enc_len);
        mac_alg = d[4];
        if ((mac_len = get_mac_len (mac_alg)) == -1)
          goto invalid_data;
        CHECK_SIZE (5 + mac_len);
        /* FIXME, check MAC */
        ADVANCE (5 + mac_len);

        p = gst_mikey_payload_new (GST_MIKEY_PT_KEMAC);
        gst_mikey_payload_kemac_set (p, enc_alg, mac_alg);

        if (state == STATE_PSK)
          /* we expect Key data for Preshared key */
          np = GST_MIKEY_PT_KEY_DATA;
        else if (state == STATE_PK)
          /* we expect ID for Public key */
          np = GST_MIKEY_PT_ID;
        else
          goto invalid_data;

        payloads_from_bytes (STATE_KEMAC,
            ((GstMIKEYPayloadKEMAC *) p)->subpayloads, enc_data, enc_len, np,
            info, error);
        g_array_append_val (payloads, p);
        break;
      }
      case GST_MIKEY_PT_T:
      {
        GstMIKEYTSType type;
        guint ts_len;
        const guint8 *ts_value;
        /*                      1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Next Payload  !   TS type     ! TS value                      ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        CHECK_SIZE (2);
        next_payload = d[0];
        type = d[1];
        if ((ts_len = get_ts_len (type)) == -1)
          goto invalid_data;
        CHECK_SIZE (2 + ts_len);
        ts_value = &d[2];
        ADVANCE (2 + ts_len);

        p = gst_mikey_payload_new (GST_MIKEY_PT_T);
        gst_mikey_payload_t_set (p, type, ts_value);
        g_array_append_val (payloads, p);
        break;
      }
      case GST_MIKEY_PT_PKE:
      {
        guint8 C;
        guint16 clen, data_len;
        const guint8 *data;
        /*                      1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Next Payload  ! C ! Data len                  ! Data          ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        CHECK_SIZE (3);
        next_payload = d[0];
        clen = GST_READ_UINT16_BE (&d[1]);
        C = clen >> 14;
        data_len = clen & 0x3fff;
        CHECK_SIZE (3 + data_len);
        data = &d[3];
        ADVANCE (3 + data_len);

        p = gst_mikey_payload_new (GST_MIKEY_PT_PKE);
        gst_mikey_payload_pke_set (p, C, data_len, data);
        g_array_append_val (payloads, p);
        break;
      }
      case GST_MIKEY_PT_DH:
      case GST_MIKEY_PT_SIGN:
      case GST_MIKEY_PT_ID:
      case GST_MIKEY_PT_CERT:
      case GST_MIKEY_PT_CHASH:
      case GST_MIKEY_PT_V:
        break;
      case GST_MIKEY_PT_SP:
      {
        guint8 policy;
        GstMIKEYSecProto proto;
        guint16 plen;
        /*                      1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Next payload  ! Policy no     ! Prot type     ! Policy param  ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ~ length (cont) ! Policy param                                  ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        CHECK_SIZE (5);
        next_payload = d[0];
        policy = d[1];
        proto = d[2];
        plen = GST_READ_UINT16_BE (&d[3]);
        ADVANCE (5);

        p = gst_mikey_payload_new (GST_MIKEY_PT_SP);
        gst_mikey_payload_sp_set (p, policy, proto);

        CHECK_SIZE (plen);
        while (plen) {
          guint8 type, len;

          CHECK_SIZE (2);
          /*                     1                   2                   3
           * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           * ! Type          ! Length        ! Value                         ~
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           */
          type = d[0];
          len = d[1];
          CHECK_SIZE (2 + len);
          gst_mikey_payload_sp_add_param (p, type, len, &d[2]);
          ADVANCE (2 + len);
          plen -= 2 + len;
        }
        g_array_append_val (payloads, p);
        break;
      }
      case GST_MIKEY_PT_RAND:
      {
        guint8 len;
        const guint8 *rand;
        /*                      1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Next payload  ! RAND len      ! RAND                          ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        CHECK_SIZE (2);
        next_payload = d[0];
        len = d[1];
        CHECK_SIZE (2 + len);
        rand = &d[2];
        ADVANCE (2 + len);

        p = gst_mikey_payload_new (GST_MIKEY_PT_RAND);
        gst_mikey_payload_rand_set (p, len, rand);
        g_array_append_val (payloads, p);
        break;
      }
      case GST_MIKEY_PT_ERR:
        break;
      case GST_MIKEY_PT_KEY_DATA:
      {
        GstMIKEYKeyDataType key_type;
        GstMIKEYKVType kv_type;
        guint16 key_len, salt_len = 0;
        const guint8 *key_data, *salt_data;
        /*                        1                   2                   3
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * !  Next Payload ! Type  ! KV    ! Key data len                  !
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * !                         Key data                              ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * ! Salt len (optional)           ! Salt data (optional)          ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * !                        KV data (optional)                     ~
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        CHECK_SIZE (4);
        next_payload = d[0];
        key_type = d[1] >> 4;
        kv_type = d[1] & 0xf;
        key_len = GST_READ_UINT16_BE (&d[2]);
        CHECK_SIZE (4 + key_len);
        key_data = &d[4];
        ADVANCE (4 + key_len);
        if (key_type & 1) {
          CHECK_SIZE (2);
          salt_len = GST_READ_UINT16_BE (&d[0]);
          CHECK_SIZE (2 + salt_len);
          salt_data = &d[2];
          ADVANCE (2 + salt_len);
        }
        p = gst_mikey_payload_new (GST_MIKEY_PT_KEY_DATA);
        gst_mikey_payload_key_data_set_key (p, key_type & 2, key_len, key_data);
        if (salt_len > 0)
          gst_mikey_payload_key_data_set_salt (p, salt_len, salt_data);

        if (kv_type == GST_MIKEY_KV_SPI) {
          guint8 spi_len;
          const guint8 *spi_data;
          /*
           *                      1                   2                   3
           *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           * ! SPI Length    ! SPI                                           ~
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           */
          CHECK_SIZE (1);
          spi_len = d[0];
          CHECK_SIZE (1 + spi_len);
          spi_data = &d[1];
          ADVANCE (1 + spi_len);

          gst_mikey_payload_key_data_set_spi (p, spi_len, spi_data);
        } else if (kv_type == GST_MIKEY_KV_INTERVAL) {
          guint8 vf_len, vt_len;
          const guint8 *vf_data, *vt_data;
          /*
           *                      1                   2                   3
           *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           * ! VF Length     ! Valid From                                    ~
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           * ! VT Length     ! Valid To (expires)                            ~
           * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           */
          CHECK_SIZE (1);
          vf_len = d[0];
          CHECK_SIZE (1 + vf_len);
          vf_data = &d[1];
          ADVANCE (1 + vf_len);
          CHECK_SIZE (1);
          vt_len = d[0];
          CHECK_SIZE (1 + vt_len);
          vt_data = &d[1];
          ADVANCE (1 + vt_len);

          gst_mikey_payload_key_data_set_interval (p, vf_len, vf_data, vt_len,
              vt_data);
        } else if (kv_type != GST_MIKEY_KV_NULL)
          goto invalid_data;

        g_array_append_val (payloads, p);
        break;
      }
      case GST_MIKEY_PT_GEN_EXT:
      case GST_MIKEY_PT_LAST:
        break;
    }
  }
  return TRUE;

  /* ERRORS */
short_data:
  {
    GST_DEBUG ("not enough data");
    if (p)
      gst_mikey_payload_unref (p);
    return FALSE;
  }
invalid_data:
  {
    GST_DEBUG ("invalid data");
    if (p)
      gst_mikey_payload_unref (p);
    return FALSE;
  }
}

/**
 * gst_mikey_message_new_from_data:
 * @data: (array length=size) (element-type guint8): bytes to read
 * @size: length of @data
 * @info: #GstMIKEYDecryptInfo
 * @error: a #GError
 *
 * Parse @size bytes from @data into a #GstMIKEYMessage. @info contains the
 * parameters to decrypt and verify the data.
 *
 * Returns: (transfer full): a #GstMIKEYMessage on success or %NULL when parsing failed and
 * @error will be set.
 *
 * Since: 1.4
 */
GstMIKEYMessage *
gst_mikey_message_new_from_data (gconstpointer data, gsize size,
    GstMIKEYDecryptInfo * info, GError ** error)
{
  GstMIKEYMessage *msg;
  guint n_cs, i;
  const guint8 *d = data;
  guint8 next_payload;
  ParseState state;

  g_return_val_if_fail (data != NULL, NULL);

  msg = gst_mikey_message_new ();
  /*                      1                   2                   3
   *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   * !  version      !  data type    ! next payload  !V! PRF func    !
   * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   * !                         CSB ID                                !
   * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   * ! #CS           ! CS ID map type! CS ID map info                ~
   * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   */
  CHECK_SIZE (10);
  msg->version = d[0];
  if (msg->version != GST_MIKEY_VERSION)
    goto unknown_version;

  msg->type = d[1];
  next_payload = d[2];
  msg->V = d[3] & 0x80 ? TRUE : FALSE;
  msg->prf_func = d[3] & 0x7f;
  msg->CSB_id = GST_READ_UINT32_BE (&d[4]);
  n_cs = d[8];
  msg->map_type = d[9];
  ADVANCE (10);

  CHECK_SIZE (n_cs * 9);
  for (i = 0; i < n_cs; i++) {
    GstMIKEYMapSRTP map;

    /*                      1                   2                   3
     *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * ! Policy_no_1   ! SSRC_1                                        !
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * ! SSRC_1 (cont) ! ROC_1                                         !
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * ! ROC_1 (cont)  ! Policy_no_2   ! SSRC_2                        ~
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     */
    map.policy = d[0];
    map.ssrc = GST_READ_UINT32_BE (&d[1]);
    map.roc = GST_READ_UINT32_BE (&d[5]);
    gst_mikey_message_insert_cs_srtp (msg, -1, &map);
    ADVANCE (9);
  }
  if (msg->type == GST_MIKEY_TYPE_PSK_INIT)
    state = STATE_PSK;
  else if (msg->type == GST_MIKEY_TYPE_PK_INIT)
    state = STATE_PK;
  else
    state = STATE_OTHER;

  if (!payloads_from_bytes (state, msg->payloads, d, size, next_payload, info,
          error))
    goto parse_error;

  return msg;

  /* ERRORS */
short_data:
  {
    GST_DEBUG ("not enough data");
    gst_mikey_message_unref (msg);
    return NULL;
  }
unknown_version:
  {
    GST_DEBUG ("unknown version");
    gst_mikey_message_unref (msg);
    return NULL;
  }
parse_error:
  {
    GST_DEBUG ("failed to parse");
    gst_mikey_message_unref (msg);
    return NULL;
  }
}

#define AES_128_KEY_LEN 16
#define AES_256_KEY_LEN 32
#define HMAC_32_KEY_LEN 4
#define HMAC_80_KEY_LEN 10

static gboolean
auth_alg_from_cipher_name (const gchar * cipher, guint8 * auth_alg)
{
  if (g_strcmp0 (cipher, "aes-128-icm") == 0)
    *auth_alg = GST_MIKEY_MAC_HMAC_SHA_1_160;
  else if (g_strcmp0 (cipher, "aes-256-icm") == 0)
    *auth_alg = GST_MIKEY_MAC_HMAC_SHA_1_160;
  else if (g_strcmp0 (cipher, "aes-128-gcm") == 0)
    *auth_alg = GST_MIKEY_MAC_NULL;
  else if (g_strcmp0 (cipher, "aes-256-gcm") == 0)
    *auth_alg = GST_MIKEY_MAC_NULL;
  else {
    GST_ERROR ("encryption algorithm '%s' not supported", cipher);
    return FALSE;
  }
  return TRUE;
}

static gboolean
enc_alg_from_cipher_name (const gchar * cipher, guint8 * enc_alg)
{
  if (g_strcmp0 (cipher, "aes-128-icm") == 0)
    *enc_alg = GST_MIKEY_ENC_AES_CM_128;
  else if (g_strcmp0 (cipher, "aes-256-icm") == 0)
    *enc_alg = GST_MIKEY_ENC_AES_CM_128;
  else if (g_strcmp0 (cipher, "aes-128-gcm") == 0)
    *enc_alg = GST_MIKEY_ENC_AES_GCM_128;
  else if (g_strcmp0 (cipher, "aes-256-gcm") == 0)
    *enc_alg = GST_MIKEY_ENC_AES_GCM_128;
  else {
    GST_ERROR ("encryption algorithm '%s' not supported", cipher);
    return FALSE;
  }
  return TRUE;
}


static gboolean
enc_key_length_from_cipher_name (const gchar * cipher, guint8 * enc_key_length)
{
  if (g_strcmp0 (cipher, "aes-128-icm") == 0)
    *enc_key_length = AES_128_KEY_LEN;
  else if (g_strcmp0 (cipher, "aes-256-icm") == 0)
    *enc_key_length = AES_256_KEY_LEN;
  else if (g_strcmp0 (cipher, "aes-128-gcm") == 0)
    *enc_key_length = AES_128_KEY_LEN;
  else if (g_strcmp0 (cipher, "aes-256-gcm") == 0)
    *enc_key_length = AES_256_KEY_LEN;
  else {
    GST_ERROR ("encryption algorithm '%s' not supported", cipher);
    return FALSE;
  }
  return TRUE;
}

static gboolean
auth_key_length_from_auth_cipher_name (const gchar * auth, const gchar * cipher,
    guint8 * length)
{
  if (g_strcmp0 (cipher, "aes-128-gcm") == 0
      || g_strcmp0 (cipher, "aes-256-gcm") == 0) {
    *length = 0;
  } else {
    if (g_strcmp0 (auth, "hmac-sha1-32") == 0) {
      *length = HMAC_32_KEY_LEN;
    } else if (g_strcmp0 (auth, "hmac-sha1-80") == 0) {
      *length = HMAC_80_KEY_LEN;
    } else {
      GST_ERROR ("authentication algorithm '%s' not supported", auth);
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * gst_mikey_message_new_from_caps:
 * @caps: a #GstCaps, including SRTP parameters (srtp/srtcp cipher, authorization, key data)
 *
 * Makes mikey message including:
 *  - Security Policy Payload
 *  - Key Data Transport Payload
 *  - Key Data Sub-Payload
 *
 * Returns: (transfer full) (nullable): a #GstMIKEYMessage,
 * or %NULL if there is no srtp information in the caps.
 *
 * Since: 1.8
 */
GstMIKEYMessage *
gst_mikey_message_new_from_caps (GstCaps * caps)
{
  GstMIKEYMessage *msg;
  GstMIKEYPayload *payload, *pkd;
  guint8 byte;
  guint8 enc_alg;
  guint8 auth_alg;
  guint8 enc_key_length;
  guint8 auth_key_length;
  GstStructure *s;
  GstMapInfo info;
  GstBuffer *srtpkey;
  const GValue *val;
  const gchar *cipher, *auth;
  const gchar *srtpcipher, *srtpauth, *srtcpcipher, *srtcpauth;

  g_return_val_if_fail (caps != NULL && GST_IS_CAPS (caps), NULL);

  s = gst_caps_get_structure (caps, 0);
  g_return_val_if_fail (s != NULL, NULL);

  val = gst_structure_get_value (s, "srtp-key");
  if (!val)
    goto no_key;

  srtpkey = gst_value_get_buffer (val);
  if (!srtpkey || !GST_IS_BUFFER (srtpkey))
    goto no_key;

  srtpcipher = gst_structure_get_string (s, "srtp-cipher");
  srtpauth = gst_structure_get_string (s, "srtp-auth");
  srtcpcipher = gst_structure_get_string (s, "srtcp-cipher");
  srtcpauth = gst_structure_get_string (s, "srtcp-auth");

  /* we need srtp cipher/auth or srtcp cipher/auth */
  if ((srtpcipher == NULL || srtpauth == NULL)
      && (srtcpcipher == NULL || srtcpauth == NULL)) {
    GST_WARNING ("could not find the right SRTP parameters in caps");
    return NULL;
  }

  /* prefer srtp cipher over srtcp */
  cipher = srtpcipher;
  if (cipher == NULL)
    cipher = srtcpcipher;

  /* prefer srtp auth over srtcp */
  auth = srtpauth;
  if (auth == NULL)
    auth = srtcpauth;

  /* get cipher and auth values */
  if (!enc_alg_from_cipher_name (cipher, &enc_alg) ||
      !auth_alg_from_cipher_name (cipher, &auth_alg) ||
      !enc_key_length_from_cipher_name (cipher, &enc_key_length) ||
      !auth_key_length_from_auth_cipher_name (auth, cipher, &auth_key_length)) {
    return NULL;
  }

  msg = gst_mikey_message_new ();
  /* unencrypted MIKEY message, we send this over TLS so this is allowed */
  gst_mikey_message_set_info (msg, GST_MIKEY_VERSION, GST_MIKEY_TYPE_PSK_INIT,
      FALSE, GST_MIKEY_PRF_MIKEY_1, g_random_int (), GST_MIKEY_MAP_TYPE_SRTP);

  /* timestamp is now */
  gst_mikey_message_add_t_now_ntp_utc (msg);
  /* add some random data */
  gst_mikey_message_add_rand_len (msg, 16);

  /* the policy '0' is SRTP */
  payload = gst_mikey_payload_new (GST_MIKEY_PT_SP);
  gst_mikey_payload_sp_set (payload, 0, GST_MIKEY_SEC_PROTO_SRTP);

  /* AES-CM or AES-GCM is supported */
  gst_mikey_payload_sp_add_param (payload, GST_MIKEY_SP_SRTP_ENC_ALG, 1,
      &enc_alg);
  /* encryption key length */
  gst_mikey_payload_sp_add_param (payload, GST_MIKEY_SP_SRTP_ENC_KEY_LEN, 1,
      &enc_key_length);
  /* HMAC-SHA1 or NULL in case of GCM */
  gst_mikey_payload_sp_add_param (payload, GST_MIKEY_SP_SRTP_AUTH_ALG, 1,
      &auth_alg);
  /* authentication key length */
  gst_mikey_payload_sp_add_param (payload, GST_MIKEY_SP_SRTP_AUTH_KEY_LEN, 1,
      &auth_key_length);
  /* we enable encryption on RTP and RTCP */
  byte = 1;
  gst_mikey_payload_sp_add_param (payload, GST_MIKEY_SP_SRTP_SRTP_ENC, 1,
      &byte);
  gst_mikey_payload_sp_add_param (payload, GST_MIKEY_SP_SRTP_SRTCP_ENC, 1,
      &byte);
  /* we enable authentication on RTP and RTCP */
  gst_mikey_payload_sp_add_param (payload, GST_MIKEY_SP_SRTP_SRTP_AUTH, 1,
      &byte);
  gst_mikey_message_add_payload (msg, payload);

  /* make unencrypted KEMAC */
  payload = gst_mikey_payload_new (GST_MIKEY_PT_KEMAC);
  gst_mikey_payload_kemac_set (payload, GST_MIKEY_ENC_NULL, GST_MIKEY_MAC_NULL);
  /* add the key in KEMAC */
  pkd = gst_mikey_payload_new (GST_MIKEY_PT_KEY_DATA);
  gst_buffer_map (srtpkey, &info, GST_MAP_READ);
  gst_mikey_payload_key_data_set_key (pkd, GST_MIKEY_KD_TEK, info.size,
      info.data);
  gst_buffer_unmap (srtpkey, &info);
  gst_mikey_payload_kemac_add_sub (payload, pkd);
  gst_mikey_message_add_payload (msg, payload);

  return msg;

no_key:
  GST_INFO ("No srtp key");
  return NULL;
}

#define AES_128_KEY_LEN 16
#define AES_256_KEY_LEN 32
#define HMAC_32_KEY_LEN 4
#define HMAC_80_KEY_LEN 10

/**
 * gst_mikey_message_to_caps:
 * @msg: a #GstMIKEYMessage
 * @caps: a #GstCaps to be filled with SRTP parameters (srtp/srtcp cipher, authorization, key data)
 *
 * Returns: %TRUE on success
 *
 * Since: 1.8.1
 */
gboolean
gst_mikey_message_to_caps (const GstMIKEYMessage * msg, GstCaps * caps)
{
  gboolean res = FALSE;
  const GstMIKEYMapSRTP *srtp;
  const GstMIKEYPayload *payload;
  const gchar *srtp_cipher;
  const gchar *srtp_auth;

  srtp_cipher = "aes-128-icm";
  srtp_auth = "hmac-sha1-80";

  /* Look for first crypto session */
  if (!(srtp = gst_mikey_message_get_cs_srtp (msg, 0))) {
    GST_ERROR ("No crypto session found at index 0");
    goto done;
  }

  /* Look for crypto policy corresponding to first crypto session */
  if ((payload =
          gst_mikey_message_find_payload (msg, GST_MIKEY_PT_SP,
              (unsigned int) srtp->policy))) {
    GstMIKEYPayloadSP *p = (GstMIKEYPayloadSP *) payload;
    guint len, i;
    guint enc_alg = GST_MIKEY_ENC_NULL;

    if (p->proto != GST_MIKEY_SEC_PROTO_SRTP)
      goto done;

    len = gst_mikey_payload_sp_get_n_params (payload);
    for (i = 0; i < len; i++) {
      const GstMIKEYPayloadSPParam *param =
          gst_mikey_payload_sp_get_param (payload, i);

      switch (param->type) {
        case GST_MIKEY_SP_SRTP_ENC_ALG:
          enc_alg = param->val[0];
          switch (param->val[0]) {
            case GST_MIKEY_ENC_NULL:
              srtp_cipher = "null";
              break;
            case GST_MIKEY_ENC_AES_CM_128:
            case GST_MIKEY_ENC_AES_KW_128:
              srtp_cipher = "aes-128-icm";
              break;
            case GST_MIKEY_ENC_AES_GCM_128:
              srtp_cipher = "aes-128-gcm";
              break;
            default:
              break;
          }
          break;
        case GST_MIKEY_SP_SRTP_ENC_KEY_LEN:
          switch (param->val[0]) {
            case AES_128_KEY_LEN:
              if (enc_alg == GST_MIKEY_ENC_AES_CM_128 ||
                  enc_alg == GST_MIKEY_ENC_AES_KW_128) {
                srtp_cipher = "aes-128-icm";
              } else if (enc_alg == GST_MIKEY_ENC_AES_GCM_128) {
                srtp_cipher = "aes-128-gcm";
              }
              break;
            case AES_256_KEY_LEN:
              if (enc_alg == GST_MIKEY_ENC_AES_CM_128 ||
                  enc_alg == GST_MIKEY_ENC_AES_KW_128) {
                srtp_cipher = "aes-256-icm";
              } else if (enc_alg == GST_MIKEY_ENC_AES_GCM_128) {
                srtp_cipher = "aes-256-gcm";
              }
              break;
            default:
              break;
          }
          break;
        case GST_MIKEY_SP_SRTP_AUTH_ALG:
          switch (param->val[0]) {
            case GST_MIKEY_MAC_NULL:
              srtp_auth = "null";
              break;
            case GST_MIKEY_MAC_HMAC_SHA_1_160:
              srtp_auth = "hmac-sha1-80";
              break;
            default:
              break;
          }
          break;
        case GST_MIKEY_SP_SRTP_AUTH_KEY_LEN:
          switch (param->val[0]) {
            case HMAC_32_KEY_LEN:
              srtp_auth = "hmac-sha1-32";
              break;
            case HMAC_80_KEY_LEN:
              srtp_auth = "hmac-sha1-80";
              break;
            default:
              break;
          }
          break;
        case GST_MIKEY_SP_SRTP_SRTP_ENC:
          break;
        case GST_MIKEY_SP_SRTP_SRTCP_ENC:
          break;
        default:
          break;
      }
    }
  }

  if (!(payload = gst_mikey_message_find_payload (msg, GST_MIKEY_PT_KEMAC, 0)))
    goto done;
  else {
    GstMIKEYPayloadKEMAC *p = (GstMIKEYPayloadKEMAC *) payload;
    const GstMIKEYPayload *sub;
    GstMIKEYPayloadKeyData *pkd;
    GstBuffer *buf, *saltbuf;

    if (p->enc_alg != GST_MIKEY_ENC_NULL || p->mac_alg != GST_MIKEY_MAC_NULL)
      goto done;

    if (!(sub = gst_mikey_payload_kemac_get_sub (payload, 0)))
      goto done;

    if (sub->type != GST_MIKEY_PT_KEY_DATA)
      goto done;

    pkd = (GstMIKEYPayloadKeyData *) sub;
    buf = gst_buffer_new_memdup (pkd->key_data, pkd->key_len);
    if (pkd->salt_len) {
      saltbuf = gst_buffer_new_memdup (pkd->salt_data, pkd->salt_len);
      gst_buffer_append (buf, saltbuf);
      gst_buffer_unref (saltbuf);
    }
    gst_caps_set_simple (caps, "srtp-key", GST_TYPE_BUFFER, buf, NULL);
    gst_buffer_unref (buf);

    gst_caps_set_simple (caps, "roc", G_TYPE_UINT, srtp->roc, NULL);
  }

  gst_caps_set_simple (caps,
      "srtp-cipher", G_TYPE_STRING, srtp_cipher,
      "srtp-auth", G_TYPE_STRING, srtp_auth,
      "srtcp-cipher", G_TYPE_STRING, srtp_cipher,
      "srtcp-auth", G_TYPE_STRING, srtp_auth, NULL);

  res = TRUE;

done:
  return res;
}

/**
 * gst_mikey_message_base64_encode:
 * @msg: a #GstMIKEYMessage
 *
 * Returns: (transfer full): a #gchar, base64-encoded data
 *
 * Since: 1.8
 */
gchar *
gst_mikey_message_base64_encode (GstMIKEYMessage * msg)
{
  GBytes *bytes;
  gchar *base64;
  const guint8 *data;
  gsize size;

  g_return_val_if_fail (msg != NULL, NULL);

  /* serialize mikey message to bytes */
  bytes = gst_mikey_message_to_bytes (msg, NULL, NULL);

  /* and make it into base64 */
  data = g_bytes_get_data (bytes, &size);
  base64 = g_base64_encode (data, size);
  g_bytes_unref (bytes);

  return base64;
}
