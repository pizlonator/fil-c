/*
 * This file was automatically generated from tss2_esys.h,
 * which is covered by the following license:
 * SPDX-License-Identifier: BSD-2-Clause
 */
FUNC(TSS2_RC, Esys_Initialize, (ESYS_CONTEXT **esys_context, TSS2_TCTI_CONTEXT *tcti, TSS2_ABI_VERSION *abiVersion), (esys_context, tcti, abiVersion))
VOID_FUNC(void, Esys_Finalize, (ESYS_CONTEXT **context), (context))
FUNC(TSS2_RC, Esys_TR_FromTPMPublic, (ESYS_CONTEXT *esysContext, TPM2_HANDLE tpm_handle, ESYS_TR optionalSession1, ESYS_TR optionalSession2, ESYS_TR optionalSession3, ESYS_TR *object), (esysContext, tpm_handle, optionalSession1, optionalSession2, optionalSession3, object))
FUNC(TSS2_RC, Esys_TR_SetAuth, (ESYS_CONTEXT *esysContext, ESYS_TR handle, const TPM2B_AUTH *authValue), (esysContext, handle, authValue))
FUNC(TSS2_RC, Esys_Startup, (ESYS_CONTEXT *esysContext, TPM2_SU startupType), (esysContext, startupType))
FUNC(TSS2_RC, Esys_Load, (ESYS_CONTEXT *esysContext, ESYS_TR parentHandle, ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3, const TPM2B_PRIVATE *inPrivate, const TPM2B_PUBLIC *inPublic, ESYS_TR *objectHandle), (esysContext, parentHandle, shandle1, shandle2, shandle3, inPrivate, inPublic, objectHandle))
FUNC(TSS2_RC, Esys_ReadPublic, (ESYS_CONTEXT *esysContext, ESYS_TR objectHandle, ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3, TPM2B_PUBLIC **outPublic, TPM2B_NAME **name, TPM2B_NAME **qualifiedName), (esysContext, objectHandle, shandle1, shandle2, shandle3, outPublic, name, qualifiedName))
FUNC(TSS2_RC, Esys_RSA_Decrypt, (ESYS_CONTEXT *esysContext, ESYS_TR keyHandle, ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3, const TPM2B_PUBLIC_KEY_RSA *cipherText, const TPMT_RSA_DECRYPT *inScheme, const TPM2B_DATA *label, TPM2B_PUBLIC_KEY_RSA **message), (esysContext, keyHandle, shandle1, shandle2, shandle3, cipherText, inScheme, label, message))
FUNC(TSS2_RC, Esys_Sign, (ESYS_CONTEXT *esysContext, ESYS_TR keyHandle, ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3, const TPM2B_DIGEST *digest, const TPMT_SIG_SCHEME *inScheme, const TPMT_TK_HASHCHECK *validation, TPMT_SIGNATURE **signature), (esysContext, keyHandle, shandle1, shandle2, shandle3, digest, inScheme, validation, signature))
FUNC(TSS2_RC, Esys_CreatePrimary, (ESYS_CONTEXT *esysContext, ESYS_TR primaryHandle, ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3, const TPM2B_SENSITIVE_CREATE *inSensitive, const TPM2B_PUBLIC *inPublic, const TPM2B_DATA *outsideInfo, const TPML_PCR_SELECTION *creationPCR, ESYS_TR *objectHandle, TPM2B_PUBLIC **outPublic, TPM2B_CREATION_DATA **creationData, TPM2B_DIGEST **creationHash, TPMT_TK_CREATION **creationTicket), (esysContext, primaryHandle, shandle1, shandle2, shandle3, inSensitive, inPublic, outsideInfo, creationPCR, objectHandle, outPublic, creationData, creationHash, creationTicket))
FUNC(TSS2_RC, Esys_FlushContext, (ESYS_CONTEXT *esysContext, ESYS_TR flushHandle), (esysContext, flushHandle))
FUNC(TSS2_RC, Esys_GetCapability, (ESYS_CONTEXT *esysContext, ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3, TPM2_CAP capability, UINT32 property, UINT32 propertyCount, TPMI_YES_NO *moreData, TPMS_CAPABILITY_DATA **capabilityData), (esysContext, shandle1, shandle2, shandle3, capability, property, propertyCount, moreData, capabilityData))
VOID_FUNC(void, Esys_Free, (void *__ptr), (__ptr))
