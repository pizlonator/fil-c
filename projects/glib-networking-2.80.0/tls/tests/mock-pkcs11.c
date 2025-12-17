/*
 *  Copyright 2011-2016 The Pkcs11Interop Project
 *  Copyright 2019 Igalia S.L.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 *  Originally written for the Pkcs11Interop project by: Jaroslav IMRICH <jimrich@jimrich.sk>
 */

/*
 * This file implements a PKCS #11 module to be loaded that returns a mock slot and objects.
 *
 * It is based on this project originally: https://github.com/Pkcs11Interop/pkcs11-mock
 *
 * Quite a few things have been changed since then though:
 * - The CK defines just below use glib macros just out of convenience.
 * - Logging was added just for ease of debugging.
 * - Instead of hardcoded defines for objects this now has an array
 *   of mock_objects that is easier to read and extend. The search behavior
 *   of C_FindObjects was also updated to actually search through this.
 * - The certificates/keys are real certificates/keys backed by gnutls
 *   loading them in C_Initialize from glib-networkings normal test data.
 *   This changes the behavior of many functions most notably including C_GetAttributeValue
 *   and C_Sign to use them. Any function not used in a TLS handshake was largely
 *   ignored and won't work.
 */

/* LCOV_EXCL_START */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "MockPKCS11"

#include <gio/gio.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>

/* See http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-os.html */
#define CK_PTR *
#define CK_DEFINE_FUNCTION(returnType, name) returnType G_MODULE_EXPORT name
#define CK_DECLARE_FUNCTION(returnType, name) returnType G_MODULE_EXPORT name
#define CK_DECLARE_FUNCTION_POINTER(returnType, name) returnType (* name)
#define CK_CALLBACK_FUNCTION(returnType, name) returnType (* name)
#define NULL_PTR NULL

#include "pkcs11/pkcs11.h"


#define IGNORE(P) (void)(P)

#define MOCK_MANUFACTURER_ID "GLib-Networking"
#define MOCK_MODEL "mock"
#define PKCS11_MOCK_CK_TOKEN_INFO_MAX_PIN_LEN 256
#define PKCS11_MOCK_CK_TOKEN_INFO_MIN_PIN_LEN 4

static CK_INFO mock_info = {
        .cryptokiVersion = { 2, 40 },
        .manufacturerID = MOCK_MANUFACTURER_ID,
        .libraryDescription = "Mock Module",
};

typedef struct {
        CK_OBJECT_CLASS object_class;
        CK_TOKEN_INFO info;
        union {
                gnutls_x509_crt_t cert;
                gnutls_privkey_t key;
        };
} MockObject;

static MockObject mock_objects[] = {
        {
                .object_class = CKO_CERTIFICATE,
                .info = {
                        .model = MOCK_MODEL,
                        .label = "Mock Certificate",
                        .serialNumber = "1",
                        .manufacturerID = MOCK_MANUFACTURER_ID,
                        .flags = CKF_TOKEN_INITIALIZED | CKF_WRITE_PROTECTED | CKF_LOGIN_REQUIRED | CKF_USER_PIN_INITIALIZED,
                        .ulMaxSessionCount = 1,
                },
        },
        {
                .object_class = CKO_PRIVATE_KEY,
                .info = {
                        .model = MOCK_MODEL,
                        .label = "Mock Private Key",
                        .serialNumber = "2",
                        .manufacturerID = MOCK_MANUFACTURER_ID,
                        .flags = CKF_TOKEN_INITIALIZED | CKF_WRITE_PROTECTED | CKF_LOGIN_REQUIRED | CKF_USER_PIN_INITIALIZED,
                        .ulMaxSessionCount = 1,
                        .ulMaxPinLen = PKCS11_MOCK_CK_TOKEN_INFO_MAX_PIN_LEN,
                        .ulMinPinLen = PKCS11_MOCK_CK_TOKEN_INFO_MIN_PIN_LEN,
                },
        },
        {
                .object_class = CKO_PRIVATE_KEY,
                .info = {
                        .model = MOCK_MODEL,
                        .label = "Mock Private Key 2",
                        .serialNumber = "3",
                        .manufacturerID = MOCK_MANUFACTURER_ID,
                        .flags = CKF_TOKEN_INITIALIZED | CKF_WRITE_PROTECTED | CKF_LOGIN_REQUIRED | CKF_USER_PIN_INITIALIZED,
                        .ulMaxSessionCount = 1,
                        .ulMaxPinLen = PKCS11_MOCK_CK_TOKEN_INFO_MAX_PIN_LEN,
                        .ulMinPinLen = PKCS11_MOCK_CK_TOKEN_INFO_MIN_PIN_LEN,
                },
        },
        {
                .object_class = CKO_CERTIFICATE,
                .info = {
                        .model = MOCK_MODEL,
                        .label = "Mock Certificate 2",
                        .serialNumber = "4",
                        .manufacturerID = MOCK_MANUFACTURER_ID,
                        .flags = CKF_TOKEN_INITIALIZED | CKF_WRITE_PROTECTED,
                        .ulMaxSessionCount = 1,
                },
        },
};

typedef struct {
        CK_SLOT_INFO info;
        //CK_TOKEN_INFO_PTR tokens[2];
} MockSlot;

static const MockSlot mock_slots[] = {
        {
                .info = {
                        .slotDescription = "Mock Slot",
                        .manufacturerID = MOCK_MANUFACTURER_ID,
                        .flags = CKF_TOKEN_PRESENT,
                },
        }
};


// FIXME: These are left overs that are unused
#define PKCS11_MOCK_CK_OBJECT_HANDLE_DATA 1
#define PKCS11_MOCK_CK_OBJECT_HANDLE_SECRET_KEY 2
#define PKCS11_MOCK_CK_OBJECT_HANDLE_PUBLIC_KEY 3
#define PKCS11_MOCK_CK_OBJECT_HANDLE_PRIVATE_KEY 4
#define PKCS11_MOCK_CK_SLOT_ID 0

#define PKCS11_MOCK_CK_SESSION_ID 1

typedef enum
{
        PKCS11_MOCK_CK_OPERATION_NONE,
        PKCS11_MOCK_CK_OPERATION_FIND,
        PKCS11_MOCK_CK_OPERATION_ENCRYPT,
        PKCS11_MOCK_CK_OPERATION_DECRYPT,
        PKCS11_MOCK_CK_OPERATION_DIGEST,
        PKCS11_MOCK_CK_OPERATION_SIGN,
        PKCS11_MOCK_CK_OPERATION_SIGN_RECOVER,
        PKCS11_MOCK_CK_OPERATION_VERIFY,
        PKCS11_MOCK_CK_OPERATION_VERIFY_RECOVER,
        PKCS11_MOCK_CK_OPERATION_DIGEST_ENCRYPT,
        PKCS11_MOCK_CK_OPERATION_DECRYPT_DIGEST,
        PKCS11_MOCK_CK_OPERATION_SIGN_ENCRYPT,
        PKCS11_MOCK_CK_OPERATION_DECRYPT_VERIFY
} PKCS11_MOCK_CK_OPERATION;

#define PKCS11_MOCK_CKO_ANYTHING -1 // We'll use -1 as a magic match all

static CK_BBOOL pkcs11_mock_initialized = CK_FALSE;
static CK_BBOOL pkcs11_mock_session_opened = CK_FALSE;
static CK_ULONG pkcs11_mock_session_state = CKS_RO_PUBLIC_SESSION;
static PKCS11_MOCK_CK_OPERATION pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;
static CK_ULONG pkcs11_mock_sign_key = 0;
static CK_LONG mock_search_template_class = PKCS11_MOCK_CKO_ANYTHING;
static char *mock_search_template_label;
static CK_ULONG mock_search_iterator = 0;
static gboolean mock_logged_in_state = FALSE;
static size_t mock_login_attempts = 0;
static CK_ULONG mock_sign_algo = 0;

static CK_FUNCTION_LIST pkcs11_mock_functions = 
{
        {2, 20},
        &C_Initialize,
        &C_Finalize,
        &C_GetInfo,
        &C_GetFunctionList,
        &C_GetSlotList,
        &C_GetSlotInfo,
        &C_GetTokenInfo,
        &C_GetMechanismList,
        &C_GetMechanismInfo,
        &C_InitToken,
        &C_InitPIN,
        &C_SetPIN,
        &C_OpenSession,
        &C_CloseSession,
        &C_CloseAllSessions,
        &C_GetSessionInfo,
        &C_GetOperationState,
        &C_SetOperationState,
        &C_Login,
        &C_Logout,
        &C_CreateObject,
        &C_CopyObject,
        &C_DestroyObject,
        &C_GetObjectSize,
        &C_GetAttributeValue,
        &C_SetAttributeValue,
        &C_FindObjectsInit,
        &C_FindObjects,
        &C_FindObjectsFinal,
        &C_EncryptInit,
        &C_Encrypt,
        &C_EncryptUpdate,
        &C_EncryptFinal,
        &C_DecryptInit,
        &C_Decrypt,
        &C_DecryptUpdate,
        &C_DecryptFinal,
        &C_DigestInit,
        &C_Digest,
        &C_DigestUpdate,
        &C_DigestKey,
        &C_DigestFinal,
        &C_SignInit,
        &C_Sign,
        &C_SignUpdate,
        &C_SignFinal,
        &C_SignRecoverInit,
        &C_SignRecover,
        &C_VerifyInit,
        &C_Verify,
        &C_VerifyUpdate,
        &C_VerifyFinal,
        &C_VerifyRecoverInit,
        &C_VerifyRecover,
        &C_DigestEncryptUpdate,
        &C_DecryptDigestUpdate,
        &C_SignEncryptUpdate,
        &C_DecryptVerifyUpdate,
        &C_GenerateKey,
        &C_GenerateKeyPair,
        &C_WrapKey,
        &C_UnwrapKey,
        &C_DeriveKey,
        &C_SeedRandom,
        &C_GenerateRandom,
        &C_GetFunctionStatus,
        &C_CancelFunction,
        &C_WaitForSlotEvent
};


/* Copy a string into a buffer without NUL termination and padded with ' ' */
static void
copy_padded_string(CK_UTF8CHAR_PTR dest, const CK_UTF8CHAR_PTR src, size_t dest_size)
{
        const size_t len = strlen((char*)src);

        g_assert (len < dest_size);

        memset(dest, ' ', dest_size);
        memcpy(dest, src, len);
}

CK_DEFINE_FUNCTION(CK_RV, C_Initialize)(CK_VOID_PTR pInitArgs)
{
        int status;
        gnutls_datum_t data;
        char *path;

        if (CK_TRUE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_ALREADY_INITIALIZED;

        IGNORE(pInitArgs);

        // client.pem
        path = g_test_build_filename(G_TEST_DIST, "files", "client.pem", NULL);
        status = gnutls_load_file(path, &data);
        g_debug("Loading %s - %s", path, gnutls_strerror(status));
        g_assert(status == GNUTLS_E_SUCCESS);

        status = gnutls_x509_crt_init(&mock_objects[0].cert);
        g_assert(status == GNUTLS_E_SUCCESS);

        status = gnutls_x509_crt_import(mock_objects[0].cert, &data, GNUTLS_X509_FMT_PEM);
        g_assert(status == GNUTLS_E_SUCCESS);

        gnutls_free(data.data);
        g_free(path);

        // client-key.pem
        path = g_test_build_filename(G_TEST_DIST, "files", "client-key.pem", NULL);
        status = gnutls_load_file(path, &data);
        g_debug("Loading %s - %s", path, gnutls_strerror(status));
        g_assert(status == GNUTLS_E_SUCCESS);

        status = gnutls_privkey_init(&mock_objects[1].key);
        g_assert(status == GNUTLS_E_SUCCESS);

        status = gnutls_privkey_import_x509_raw(mock_objects[1].key, &data, GNUTLS_X509_FMT_PEM, NULL, 0);
        g_assert(status == GNUTLS_E_SUCCESS);

        gnutls_free(data.data);
        g_free(path);

        // client2-key.pem
        path = g_test_build_filename(G_TEST_DIST, "files", "client2-key.pem", NULL);
        status = gnutls_load_file(path, &data);
        g_debug("Loading %s - %s", path, gnutls_strerror(status));
        g_assert(status == GNUTLS_E_SUCCESS);

        status = gnutls_privkey_init(&mock_objects[2].key);
        g_assert(status == GNUTLS_E_SUCCESS);

        status = gnutls_privkey_import_x509_raw(mock_objects[2].key, &data, GNUTLS_X509_FMT_PEM, NULL, 0);
        g_assert(status == GNUTLS_E_SUCCESS);

        gnutls_free(data.data);
        g_free(path);

        // client2.pem
        path = g_test_build_filename(G_TEST_DIST, "files", "client2.pem", NULL);
        status = gnutls_load_file(path, &data);
        g_debug("Loading %s - %s", path, gnutls_strerror(status));
        g_assert(status == GNUTLS_E_SUCCESS);

        status = gnutls_x509_crt_init(&mock_objects[3].cert);
        g_assert(status == GNUTLS_E_SUCCESS);

        status = gnutls_x509_crt_import(mock_objects[3].cert, &data, GNUTLS_X509_FMT_PEM);
        g_assert(status == GNUTLS_E_SUCCESS);

        gnutls_free(data.data);
        g_free(path);

        pkcs11_mock_initialized = CK_TRUE;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_Finalize)(CK_VOID_PTR pReserved)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        IGNORE(pReserved);

        gnutls_x509_crt_deinit(mock_objects[0].cert);
        gnutls_privkey_deinit(mock_objects[1].key);

        pkcs11_mock_initialized = CK_FALSE;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GetInfo)(CK_INFO_PTR pInfo)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (NULL == pInfo)
                return CKR_ARGUMENTS_BAD;

        pInfo->cryptokiVersion = mock_info.cryptokiVersion;
        copy_padded_string(pInfo->manufacturerID, mock_info.manufacturerID, sizeof(pInfo->manufacturerID));
        pInfo->flags = 0;
        copy_padded_string(pInfo->libraryDescription, mock_info.libraryDescription, sizeof(pInfo->libraryDescription));
        pInfo->libraryVersion = mock_info.libraryVersion;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GetFunctionList)(CK_FUNCTION_LIST_PTR_PTR ppFunctionList)
{
        if (NULL == ppFunctionList)
                return CKR_ARGUMENTS_BAD;

        *ppFunctionList = &pkcs11_mock_functions;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GetSlotList)(CK_BBOOL tokenPresent, CK_SLOT_ID_PTR pSlotList, CK_ULONG_PTR pulCount)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        IGNORE(tokenPresent);

        if (NULL == pulCount)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pSlotList)
        {
                *pulCount = 1;
        }
        else
        {
                if (0 == *pulCount)
                        return CKR_BUFFER_TOO_SMALL;

                pSlotList[0] = PKCS11_MOCK_CK_SLOT_ID;
                *pulCount = 1;
        }

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GetSlotInfo)(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo)
{
        MockSlot mock_slot;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (slotID >= G_N_ELEMENTS (mock_slots))
                return CKR_SLOT_ID_INVALID;

        if (NULL == pInfo)
                return CKR_ARGUMENTS_BAD;

        mock_slot = mock_slots[slotID];

        copy_padded_string(pInfo->slotDescription, mock_slot.info.slotDescription, sizeof(pInfo->slotDescription));
        copy_padded_string(pInfo->manufacturerID, mock_slot.info.manufacturerID, sizeof(pInfo->manufacturerID));
        pInfo->flags = mock_slot.info.flags;
        pInfo->hardwareVersion = mock_slot.info.hardwareVersion;
        pInfo->firmwareVersion = mock_slot.info.firmwareVersion;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GetTokenInfo)(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo)
{
        CK_TOKEN_INFO token;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (slotID > G_N_ELEMENTS (mock_slots))
                return CKR_SLOT_ID_INVALID;

        if (NULL == pInfo)
                return CKR_ARGUMENTS_BAD;

        token = mock_objects[slotID].info;

        copy_padded_string(pInfo->label, token.label, sizeof(pInfo->label));
        copy_padded_string(pInfo->manufacturerID, token.manufacturerID, sizeof(pInfo->manufacturerID));
        copy_padded_string(pInfo->serialNumber, token.serialNumber, sizeof(pInfo->serialNumber));
        copy_padded_string(pInfo->model, token.model, sizeof(pInfo->model));
        pInfo->flags = token.flags;
        pInfo->ulMaxSessionCount = token.ulMaxSessionCount;
        pInfo->ulSessionCount = (CK_TRUE == pkcs11_mock_session_opened) ? 1 : 0;
        pInfo->ulMaxRwSessionCount = token.ulMaxRwSessionCount;
        pInfo->ulRwSessionCount = (CK_TRUE == pkcs11_mock_session_opened) ? 1 : 0;
        pInfo->ulMaxPinLen = token.ulMaxPinLen;
        pInfo->ulMinPinLen = token.ulMinPinLen;
        pInfo->ulTotalPublicMemory = token.ulTotalPublicMemory;
        pInfo->ulFreePublicMemory = token.ulFreePublicMemory;
        pInfo->ulTotalPrivateMemory = token.ulTotalPrivateMemory;
        pInfo->ulFreePrivateMemory = token.ulFreePrivateMemory;
        pInfo->hardwareVersion = token.hardwareVersion;
        pInfo->firmwareVersion = token.firmwareVersion;
        memset(pInfo->utcTime, ' ', sizeof(pInfo->utcTime));

        // FIXME: Not picked up by gnutls
        if (mock_login_attempts > 2)
        {
                pInfo->flags |= CKF_USER_PIN_COUNT_LOW;
        }

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GetMechanismList)(CK_SLOT_ID slotID, CK_MECHANISM_TYPE_PTR pMechanismList, CK_ULONG_PTR pulCount)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (slotID > G_N_ELEMENTS(mock_slots))
                return CKR_SLOT_ID_INVALID;

        if (NULL == pulCount)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pMechanismList)
        {
                *pulCount = 9;
        }
        else
        {
                if (9 > *pulCount)
                        return CKR_BUFFER_TOO_SMALL;

                pMechanismList[0] = CKM_RSA_PKCS_KEY_PAIR_GEN;
                pMechanismList[1] = CKM_RSA_PKCS;
                pMechanismList[2] = CKM_SHA1_RSA_PKCS;
                pMechanismList[3] = CKM_RSA_PKCS_OAEP;
                pMechanismList[4] = CKM_DES3_CBC;
                pMechanismList[5] = CKM_DES3_KEY_GEN;
                pMechanismList[6] = CKM_SHA_1;
                pMechanismList[7] = CKM_XOR_BASE_AND_DATA;
                pMechanismList[8] = CKM_AES_CBC;

                *pulCount = 9;
        }

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GetMechanismInfo)(CK_SLOT_ID slotID, CK_MECHANISM_TYPE type, CK_MECHANISM_INFO_PTR pInfo)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_SLOT_ID != slotID)
                return CKR_SLOT_ID_INVALID;

        if (NULL == pInfo)
                return CKR_ARGUMENTS_BAD;

        switch (type)
        {
                case CKM_RSA_PKCS_KEY_PAIR_GEN:
                        pInfo->ulMinKeySize = 1024;
                        pInfo->ulMaxKeySize = 1024;
                        pInfo->flags = CKF_GENERATE_KEY_PAIR;
                        break;

                case CKM_RSA_PKCS:
                        pInfo->ulMinKeySize = 1024;
                        pInfo->ulMaxKeySize = 1024;
                        pInfo->flags = CKF_ENCRYPT | CKF_DECRYPT | CKF_SIGN | CKF_SIGN_RECOVER | CKF_VERIFY | CKF_VERIFY_RECOVER | CKF_WRAP | CKF_UNWRAP;
                        break;

                case CKM_SHA1_RSA_PKCS:
                        pInfo->ulMinKeySize = 1024;
                        pInfo->ulMaxKeySize = 1024;
                        pInfo->flags = CKF_SIGN | CKF_VERIFY;
                        break;

                case CKM_RSA_PKCS_OAEP:
                        pInfo->ulMinKeySize = 1024;
                        pInfo->ulMaxKeySize = 1024;
                        pInfo->flags = CKF_ENCRYPT | CKF_DECRYPT;
                        break;

                case CKM_DES3_CBC:
                        pInfo->ulMinKeySize = 192;
                        pInfo->ulMaxKeySize = 192;
                        pInfo->flags = CKF_ENCRYPT | CKF_DECRYPT;
                        break;

                case CKM_DES3_KEY_GEN:
                        pInfo->ulMinKeySize = 192;
                        pInfo->ulMaxKeySize = 192;
                        pInfo->flags = CKF_GENERATE;
                        break;

                case CKM_SHA_1:
                        pInfo->ulMinKeySize = 0;
                        pInfo->ulMaxKeySize = 0;
                        pInfo->flags = CKF_DIGEST;
                        break;

                case CKM_XOR_BASE_AND_DATA:
                        pInfo->ulMinKeySize = 128;
                        pInfo->ulMaxKeySize = 256;
                        pInfo->flags = CKF_DERIVE;
                        break;

                case CKM_AES_CBC:
                        pInfo->ulMinKeySize = 128;
                        pInfo->ulMaxKeySize = 256;
                        pInfo->flags = CKF_ENCRYPT | CKF_DECRYPT;
                        break;

                case CKM_RSA_PKCS_PSS:
                        // FIXME: Made up key sizes
                        pInfo->ulMinKeySize = 256;
                        pInfo->ulMaxKeySize = 256;
                        // Flags based on table here: http://docs.oasis-open.org/pkcs11/pkcs11-curr/v2.40/csd01/pkcs11-curr-v2.40-csd01.html
                        pInfo->flags = CKF_SIGN | CKF_VERIFY;
                        break;

                default:
                        return CKR_MECHANISM_INVALID;
        }

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_InitToken)(CK_SLOT_ID slotID, CK_UTF8CHAR_PTR pPin, CK_ULONG ulPinLen, CK_UTF8CHAR_PTR pLabel)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_SLOT_ID != slotID)
                return CKR_SLOT_ID_INVALID;

        if (NULL == pPin)
                return CKR_ARGUMENTS_BAD;

        if ((ulPinLen < PKCS11_MOCK_CK_TOKEN_INFO_MIN_PIN_LEN) || (ulPinLen > PKCS11_MOCK_CK_TOKEN_INFO_MAX_PIN_LEN))
                return CKR_PIN_LEN_RANGE;

        if (NULL == pLabel)
                return CKR_ARGUMENTS_BAD;

        if (CK_TRUE == pkcs11_mock_session_opened)
                return CKR_SESSION_EXISTS;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_InitPIN)(CK_SESSION_HANDLE hSession, CK_UTF8CHAR_PTR pPin, CK_ULONG ulPinLen)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (CKS_RW_SO_FUNCTIONS != pkcs11_mock_session_state)
                return CKR_USER_NOT_LOGGED_IN;

        if (NULL == pPin)
                return CKR_ARGUMENTS_BAD;

        if ((ulPinLen < PKCS11_MOCK_CK_TOKEN_INFO_MIN_PIN_LEN) || (ulPinLen > PKCS11_MOCK_CK_TOKEN_INFO_MAX_PIN_LEN))
                return CKR_PIN_LEN_RANGE;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_SetPIN)(CK_SESSION_HANDLE hSession, CK_UTF8CHAR_PTR pOldPin, CK_ULONG ulOldLen, CK_UTF8CHAR_PTR pNewPin, CK_ULONG ulNewLen)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if ((CKS_RO_PUBLIC_SESSION == pkcs11_mock_session_state) || (CKS_RO_USER_FUNCTIONS == pkcs11_mock_session_state))
                return CKR_SESSION_READ_ONLY;

        if (NULL == pOldPin)
                return CKR_ARGUMENTS_BAD;

        if ((ulOldLen < PKCS11_MOCK_CK_TOKEN_INFO_MIN_PIN_LEN) || (ulOldLen > PKCS11_MOCK_CK_TOKEN_INFO_MAX_PIN_LEN))
                return CKR_PIN_LEN_RANGE;

        if (NULL == pNewPin)
                return CKR_ARGUMENTS_BAD;

        if ((ulNewLen < PKCS11_MOCK_CK_TOKEN_INFO_MIN_PIN_LEN) || (ulNewLen > PKCS11_MOCK_CK_TOKEN_INFO_MAX_PIN_LEN))
                return CKR_PIN_LEN_RANGE;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_OpenSession)(CK_SLOT_ID slotID, CK_FLAGS flags, CK_VOID_PTR pApplication, CK_NOTIFY Notify, CK_SESSION_HANDLE_PTR phSession)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (CK_TRUE == pkcs11_mock_session_opened)
                return CKR_SESSION_COUNT;

        if (PKCS11_MOCK_CK_SLOT_ID != slotID)
                return CKR_SLOT_ID_INVALID;

        if (!(flags & CKF_SERIAL_SESSION))
                return CKR_SESSION_PARALLEL_NOT_SUPPORTED;

        IGNORE(pApplication);

        IGNORE(Notify);

        if (NULL == phSession)
                return CKR_ARGUMENTS_BAD;

        pkcs11_mock_session_opened = CK_TRUE;
        pkcs11_mock_session_state = (flags & CKF_RW_SESSION) ? CKS_RW_PUBLIC_SESSION : CKS_RO_PUBLIC_SESSION;
        *phSession = PKCS11_MOCK_CK_SESSION_ID;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_CloseSession)(CK_SESSION_HANDLE hSession)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        pkcs11_mock_session_opened = CK_FALSE;
        pkcs11_mock_session_state = CKS_RO_PUBLIC_SESSION;
        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_CloseAllSessions)(CK_SLOT_ID slotID)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_SLOT_ID != slotID)
                return CKR_SLOT_ID_INVALID;

        pkcs11_mock_session_opened = CK_FALSE;
        pkcs11_mock_session_state = CKS_RO_PUBLIC_SESSION;
        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GetSessionInfo)(CK_SESSION_HANDLE hSession, CK_SESSION_INFO_PTR pInfo)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pInfo)
                return CKR_ARGUMENTS_BAD;

        pInfo->slotID = PKCS11_MOCK_CK_SLOT_ID;
        pInfo->state = pkcs11_mock_session_state;
        pInfo->flags = CKF_SERIAL_SESSION;
        if ((pkcs11_mock_session_state != CKS_RO_PUBLIC_SESSION) && (pkcs11_mock_session_state != CKS_RO_USER_FUNCTIONS))
                pInfo->flags = pInfo->flags | CKF_RW_SESSION;
        pInfo->ulDeviceError = 0;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GetOperationState)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pOperationState, CK_ULONG_PTR pulOperationStateLen)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pulOperationStateLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pOperationState)
        {
                *pulOperationStateLen = 256;
        }
        else
        {
                if (256 > *pulOperationStateLen)
                        return CKR_BUFFER_TOO_SMALL;

                memset(pOperationState, 1, 256);
                *pulOperationStateLen = 256;
        }

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_SetOperationState)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pOperationState, CK_ULONG ulOperationStateLen, CK_OBJECT_HANDLE hEncryptionKey, CK_OBJECT_HANDLE hAuthenticationKey)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pOperationState)
                return CKR_ARGUMENTS_BAD;

        if (256 != ulOperationStateLen)
                return CKR_ARGUMENTS_BAD;

        IGNORE(hEncryptionKey);

        IGNORE(hAuthenticationKey);

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_Login)(CK_SESSION_HANDLE hSession, CK_USER_TYPE userType, CK_UTF8CHAR_PTR pPin, CK_ULONG ulPinLen)
{
        // More hardcoding
        const char *password = "ABC123";

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if ((CKU_SO != userType) && (CKU_USER != userType) && (CKU_CONTEXT_SPECIFIC != userType))
                return CKR_USER_TYPE_INVALID;

        if (NULL == pPin)
                return CKR_ARGUMENTS_BAD;

        if ((ulPinLen < PKCS11_MOCK_CK_TOKEN_INFO_MIN_PIN_LEN) || (ulPinLen > PKCS11_MOCK_CK_TOKEN_INFO_MAX_PIN_LEN))
                return CKR_PIN_LEN_RANGE;

        // FIXME: gnutls bug? It calls this before an operation
        // if (pkcs11_mock_active_operation == PKCS11_MOCK_CK_OPERATION_NONE && CKU_CONTEXT_SPECIFIC != userType)
        //         return CKR_OPERATION_NOT_INITIALIZED;

        if (mock_logged_in_state == TRUE)
                return CKR_USER_ALREADY_LOGGED_IN;

        if (ulPinLen == strlen (password) && strncmp ((char*)pPin, password, ulPinLen) == 0)
        {
                mock_logged_in_state = TRUE;
                mock_login_attempts = 0;
                return CKR_OK;
        }
        else
        {
                mock_login_attempts += 1;
                return CKR_PIN_INCORRECT;
        }

        // TODO: We don't test any of these states atm
        // switch (pkcs11_mock_session_state)
        // {
        //         case CKS_RO_PUBLIC_SESSION:

        //                 if (CKU_SO == userType)
        //                         rv = CKR_SESSION_READ_ONLY_EXISTS;
        //                 else
        //                         pkcs11_mock_session_state = CKS_RO_USER_FUNCTIONS;

        //                 break;

        //         case CKS_RO_USER_FUNCTIONS:
        //         case CKS_RW_USER_FUNCTIONS:

        //                 rv = (CKU_SO == userType) ? CKR_USER_ANOTHER_ALREADY_LOGGED_IN : CKR_USER_ALREADY_LOGGED_IN;

        //                 break;

        //         case CKS_RW_PUBLIC_SESSION:

        //                 pkcs11_mock_session_state = (CKU_SO == userType) ? CKS_RW_SO_FUNCTIONS : CKS_RW_USER_FUNCTIONS;

        //                 break;

        //         case CKS_RW_SO_FUNCTIONS:

        //                 rv = (CKU_SO == userType) ? CKR_USER_ALREADY_LOGGED_IN : CKR_USER_ANOTHER_ALREADY_LOGGED_IN;

        //                 break;
        // }

        // return rv;
}


CK_DEFINE_FUNCTION(CK_RV, C_Logout)(CK_SESSION_HANDLE hSession)
{

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (mock_logged_in_state == FALSE)
                return CKR_USER_NOT_LOGGED_IN;

        // if ((pkcs11_mock_session_state == CKS_RO_PUBLIC_SESSION) || (pkcs11_mock_session_state == CKS_RW_PUBLIC_SESSION))
        //         return CKR_USER_NOT_LOGGED_IN;

        mock_logged_in_state =  FALSE;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_CreateObject)(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phObject)
{
        CK_ULONG i = 0;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pTemplate)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulCount)
                return CKR_ARGUMENTS_BAD;

        if (NULL == phObject)
                return CKR_ARGUMENTS_BAD;

        for (i = 0; i < ulCount; i++)
        {
                if (NULL == pTemplate[i].pValue)
                        return CKR_ATTRIBUTE_VALUE_INVALID;

                if (0 >= pTemplate[i].ulValueLen)
                        return CKR_ATTRIBUTE_VALUE_INVALID;
        }

        *phObject = PKCS11_MOCK_CK_OBJECT_HANDLE_DATA;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_CopyObject)(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phNewObject)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (PKCS11_MOCK_CK_OBJECT_HANDLE_DATA != hObject)
                return CKR_OBJECT_HANDLE_INVALID;

        if (NULL == phNewObject)
                return CKR_ARGUMENTS_BAD;

        *phNewObject = PKCS11_MOCK_CK_OBJECT_HANDLE_DATA;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_DestroyObject)(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if ((PKCS11_MOCK_CK_OBJECT_HANDLE_DATA != hObject) &&
                (PKCS11_MOCK_CK_OBJECT_HANDLE_SECRET_KEY != hObject) &&
                (PKCS11_MOCK_CK_OBJECT_HANDLE_PUBLIC_KEY != hObject) &&
                (PKCS11_MOCK_CK_OBJECT_HANDLE_PRIVATE_KEY != hObject))
                return CKR_OBJECT_HANDLE_INVALID;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GetObjectSize)(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ULONG_PTR pulSize)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (hObject > G_N_ELEMENTS (mock_objects))
                return CKR_OBJECT_HANDLE_INVALID;

        if (NULL == pulSize)
                return CKR_ARGUMENTS_BAD;

        *pulSize = 0; // FIXME: mock_objects[hObject].size;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GetAttributeValue)(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
        CK_ULONG i = 0;
        MockObject obj;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (hObject >= G_N_ELEMENTS (mock_objects))
                return CKR_OBJECT_HANDLE_INVALID;

        if (NULL == pTemplate)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulCount)
                return CKR_ARGUMENTS_BAD;

        obj = mock_objects[hObject];

        for (i = 0; i < ulCount; i++)
        {
                if (CKA_LABEL == pTemplate[i].type || CKA_ID == pTemplate[i].type)
                {
                        if (NULL != pTemplate[i].pValue)
                        {
                                if (pTemplate[i].ulValueLen < strlen((char*)obj.info.label))
                                        return CKR_BUFFER_TOO_SMALL;
                                else
                                        memcpy(pTemplate[i].pValue, obj.info.label, strlen((char*)obj.info.label));
                        }

                        pTemplate[i].ulValueLen = strlen((char*)obj.info.label);
                }
                else if (CKA_EXTRACTABLE == pTemplate[i].type)
                {
                        *((CK_BBOOL *) pTemplate[i].pValue) = obj.object_class == CKO_CERTIFICATE ? CK_TRUE : CK_FALSE;
                        pTemplate[i].ulValueLen = sizeof(CK_BBOOL);
                }
                else if (CKA_NEVER_EXTRACTABLE == pTemplate[i].type || CKA_SENSITIVE == pTemplate[i].type)
                {
                        *((CK_BBOOL *) pTemplate[i].pValue) = obj.object_class == CKO_PRIVATE_KEY ? CK_TRUE : CK_FALSE;
                        pTemplate[i].ulValueLen = sizeof(CK_BBOOL);
                }
		else if (CKA_CERTIFICATE_CATEGORY == pTemplate[i].type)
		{
			CK_ULONG t;
			if (pTemplate[i].ulValueLen < sizeof(CK_ULONG))
				return CKR_BUFFER_TOO_SMALL;

                        if (obj.object_class == CKO_CERTIFICATE)
                                t = CK_CERTIFICATE_CATEGORY_AUTHORITY;
                        else
                                t = CK_CERTIFICATE_CATEGORY_UNSPECIFIED;

			memcpy(pTemplate[i].pValue, &t, sizeof(CK_ULONG));
		}
                else if (CKA_SUBJECT == pTemplate[i].type)
                {
                        int status;
                        gnutls_datum_t data;
                        gnutls_x509_dn_t dn; /* Owned by cert */

                        g_assert (obj.object_class == CKO_CERTIFICATE);

                        status = gnutls_x509_crt_get_subject(obj.cert, &dn);
                        g_assert(status == GNUTLS_E_SUCCESS);
                        status = gnutls_x509_dn_get_str(dn, &data);
                        g_assert(status == GNUTLS_E_SUCCESS);

                        if (data.size > pTemplate[i].ulValueLen)
                        {
                                gnutls_free(data.data);
                                pTemplate[i].ulValueLen = data.size;
                                if (pTemplate[i].pValue != NULL) /* If NULL return OK */
                                        return CKR_BUFFER_TOO_SMALL;
                        }
                        else
                        {
                                memcpy(pTemplate[i].pValue, data.data, data.size);
                                pTemplate[i].ulValueLen = data.size;
                                gnutls_free(data.data);
                        }
                }
                else if (CKA_VALUE == pTemplate[i].type)
                {
                        if (obj.object_class == CKO_CERTIFICATE)
                        {
                                int status;
                                gnutls_datum_t data;

                                status = gnutls_x509_crt_export2(obj.cert, GNUTLS_X509_FMT_DER, &data);
                                g_assert(status == GNUTLS_E_SUCCESS);

                                if (data.size > pTemplate[i].ulValueLen)
                                {
                                        gnutls_free(data.data);
                                        pTemplate[i].ulValueLen = data.size;
                                        if (pTemplate[i].pValue != NULL) /* If NULL return OK */
                                                return CKR_BUFFER_TOO_SMALL;
                                }
                                else
                                {
                                        memcpy(pTemplate[i].pValue, data.data, data.size);
                                        gnutls_free(data.data);
                                        pTemplate[i].ulValueLen = data.size;
                                }
                        }
                        else
                        {
                                pTemplate[i].ulValueLen = CK_UNAVAILABLE_INFORMATION;
                        }
                }
                else if (CKA_CLASS == pTemplate[i].type)
                {
                        if (NULL != pTemplate[i].pValue)
                                *((CK_ULONG *) pTemplate[i].pValue) = obj.object_class;
                        pTemplate[i].ulValueLen = sizeof (obj.object_class);
                }
                 else if (CKA_CERTIFICATE_TYPE == pTemplate[i].type)
                {
                        CK_CERTIFICATE_TYPE ret = CKC_X_509;

			if (pTemplate[i].ulValueLen != sizeof(CK_CERTIFICATE_TYPE))
				return CKR_ARGUMENTS_BAD;

                        /* TODO: Test both TRUE and FALSE */
			memcpy(pTemplate[i].pValue, &ret, sizeof(CK_CERTIFICATE_TYPE));
                }
		else if (CKA_KEY_TYPE == pTemplate[i].type)
		{
			CK_KEY_TYPE t;
			if (pTemplate[i].ulValueLen != sizeof(CK_KEY_TYPE))
				return CKR_ARGUMENTS_BAD;

                        if (obj.object_class != CKO_PRIVATE_KEY)
                                return CKR_ARGUMENTS_BAD;

                        switch (gnutls_privkey_get_pk_algorithm (obj.key, NULL))
                        {
                                case GNUTLS_PK_RSA:
                                        t = CKK_RSA;
                                        break;
                                case GNUTLS_PK_DSA:
                                        t = CKK_DSA;
                                        break;
                                case GNUTLS_PK_DH:
                                        t = CKK_DH;
                                        break;
                                case GNUTLS_PK_EC:
                                        t = CKK_EC;
                                        break;
                                default:
                                        pTemplate[i].ulValueLen = CK_UNAVAILABLE_INFORMATION;
                                        return CKR_ATTRIBUTE_TYPE_INVALID;
                        }

			memcpy(pTemplate[i].pValue, &t, sizeof(CK_KEY_TYPE));
		}
		else if (CKA_ALWAYS_AUTHENTICATE == pTemplate[i].type)
		{
                        CK_BBOOL ret = CK_TRUE;

			if (pTemplate[i].ulValueLen != sizeof(CK_BBOOL))
				return CKR_ARGUMENTS_BAD;

                        /* TODO: Test both TRUE and FALSE */
			memcpy(pTemplate[i].pValue, &ret, sizeof(CK_BBOOL));
		}
		else if (CKA_MODULUS == pTemplate[i].type && obj.object_class == CKO_PRIVATE_KEY)
		{
                        /* Hardcode RSA for now */
                        gnutls_datum_t modulus;
                        int status = gnutls_privkey_export_rsa_raw (obj.key, &modulus, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
                        g_assert (status == GNUTLS_E_SUCCESS);

                        if (modulus.size > pTemplate[i].ulValueLen)
                        {
                                gnutls_free(modulus.data);
                                pTemplate[i].ulValueLen = modulus.size;
                                if (pTemplate[i].pValue != NULL) /* If NULL return OK */
                                        return CKR_BUFFER_TOO_SMALL;
                        }
                        else
                        {
                                memcpy(pTemplate[i].pValue, modulus.data, modulus.size);
                                gnutls_free(modulus.data);
                                pTemplate[i].ulValueLen = modulus.size;
                        }
		}
                else if (CKA_SIGN == pTemplate[i].type && obj.object_class == CKO_PRIVATE_KEY) /* Any key type in future */
                {
                        CK_BBOOL ret = CK_TRUE;

			if (pTemplate[i].ulValueLen != sizeof(CK_BBOOL))
				return CKR_ARGUMENTS_BAD;

			memcpy(pTemplate[i].pValue, &ret, sizeof(CK_BBOOL));
                }
                else
                {
                        return CKR_ATTRIBUTE_TYPE_INVALID;
                }
        }

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_SetAttributeValue)(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
        CK_ULONG i = 0;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (hObject > G_N_ELEMENTS (mock_objects))
        if ((PKCS11_MOCK_CK_OBJECT_HANDLE_DATA != hObject) &&
                (PKCS11_MOCK_CK_OBJECT_HANDLE_SECRET_KEY != hObject) &&
                (PKCS11_MOCK_CK_OBJECT_HANDLE_PUBLIC_KEY != hObject) &&
                (PKCS11_MOCK_CK_OBJECT_HANDLE_PRIVATE_KEY != hObject))
                return CKR_OBJECT_HANDLE_INVALID;

        if (NULL == pTemplate)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulCount)
                return CKR_ARGUMENTS_BAD;

        for (i = 0; i < ulCount; i++)
        {
                if ((CKA_LABEL == pTemplate[i].type) || (CKA_VALUE == pTemplate[i].type))
                {
                        if (NULL == pTemplate[i].pValue)
                                return CKR_ATTRIBUTE_VALUE_INVALID;

                        if (0 >= pTemplate[i].ulValueLen)
                                return CKR_ATTRIBUTE_VALUE_INVALID;
                }
                else
                {
                        return CKR_ATTRIBUTE_TYPE_INVALID;
                }
        }

        return CKR_OK;
}

CK_DEFINE_FUNCTION(CK_RV, C_FindObjectsInit)(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
        CK_ULONG i = 0;
        CK_ULONG_PTR cka_class_value = NULL;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_NONE != pkcs11_mock_active_operation)
                return CKR_OPERATION_ACTIVE;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pTemplate && ulCount != 0)
                return CKR_ARGUMENTS_BAD;

        mock_search_template_class = PKCS11_MOCK_CKO_ANYTHING;
        g_clear_pointer (&mock_search_template_label, g_free);

        for (i = 0; i < ulCount; i++)
        {
                if (NULL == pTemplate[i].pValue)
                        return CKR_ATTRIBUTE_VALUE_INVALID;

                if (0 >= pTemplate[i].ulValueLen)
                        return CKR_ATTRIBUTE_VALUE_INVALID;

                if (CKA_CLASS == pTemplate[i].type)
                {
                        if (sizeof(CK_ULONG) != pTemplate[i].ulValueLen)
                                return CKR_ATTRIBUTE_VALUE_INVALID;

                        cka_class_value = (CK_ULONG_PTR) pTemplate[i].pValue;
                        mock_search_template_class = *cka_class_value;
                }
                else if (CKA_LABEL == pTemplate[i].type)
                {
                        const char *cka_label_value = (char*)pTemplate[i].pValue;
                        g_clear_pointer (&mock_search_template_label, g_free);
                        mock_search_template_label = g_strndup (cka_label_value, pTemplate[i].ulValueLen);
                }
                else
                {
                        g_info ("Ignoring search template for %lu", pTemplate[i].type);
                }
        }

        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_FIND;
        mock_search_iterator = 0;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_FindObjects)(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject, CK_ULONG ulMaxObjectCount, CK_ULONG_PTR pulObjectCount)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_FIND != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if ((NULL == phObject) && (0 < ulMaxObjectCount))
                return CKR_ARGUMENTS_BAD;

        if (NULL == pulObjectCount)
                return CKR_ARGUMENTS_BAD;

        *pulObjectCount = 0;
        for (; mock_search_iterator < G_N_ELEMENTS (mock_objects) && *pulObjectCount < ulMaxObjectCount; mock_search_iterator++)
        {
                if ((mock_search_template_class == PKCS11_MOCK_CKO_ANYTHING || mock_objects[mock_search_iterator].object_class == mock_search_template_class) &&
                    (mock_search_template_label == NULL || g_strcmp0 ((char*)mock_objects[mock_search_iterator].info.label, mock_search_template_label) == 0))
                {
                        phObject[*pulObjectCount] = mock_search_iterator;
                        *pulObjectCount = *pulObjectCount + 1;
                }
        }

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_FindObjectsFinal)(CK_SESSION_HANDLE hSession)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_FIND != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_EncryptInit)(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((PKCS11_MOCK_CK_OPERATION_NONE != pkcs11_mock_active_operation) &&
                (PKCS11_MOCK_CK_OPERATION_DIGEST != pkcs11_mock_active_operation) && 
                (PKCS11_MOCK_CK_OPERATION_SIGN != pkcs11_mock_active_operation))
                return CKR_OPERATION_ACTIVE;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pMechanism)
                return CKR_ARGUMENTS_BAD;

        switch (pMechanism->mechanism)
        {
                case CKM_RSA_PKCS:

                        if ((NULL != pMechanism->pParameter) || (0 != pMechanism->ulParameterLen))
                                return CKR_MECHANISM_PARAM_INVALID;

                        if (PKCS11_MOCK_CK_OBJECT_HANDLE_PUBLIC_KEY != hKey)
                                return CKR_KEY_TYPE_INCONSISTENT;

                        break;

                case CKM_RSA_PKCS_OAEP:

                        if ((NULL == pMechanism->pParameter) || (sizeof(CK_RSA_PKCS_OAEP_PARAMS) != pMechanism->ulParameterLen))
                                return CKR_MECHANISM_PARAM_INVALID;

                        if (PKCS11_MOCK_CK_OBJECT_HANDLE_PUBLIC_KEY != hKey)
                                return CKR_KEY_TYPE_INCONSISTENT;

                        break;

                case CKM_DES3_CBC:

                        if ((NULL == pMechanism->pParameter) || (8 != pMechanism->ulParameterLen))
                                return CKR_MECHANISM_PARAM_INVALID;

                        if (PKCS11_MOCK_CK_OBJECT_HANDLE_SECRET_KEY != hKey)
                                return CKR_KEY_TYPE_INCONSISTENT;

                        break;

                case CKM_AES_CBC:
                        
                        if ((NULL == pMechanism->pParameter) || (16 != pMechanism->ulParameterLen))
                                return CKR_MECHANISM_PARAM_INVALID;

                        if (PKCS11_MOCK_CK_OBJECT_HANDLE_SECRET_KEY != hKey)
                                return CKR_KEY_TYPE_INCONSISTENT;

                        break;

                default:

                        return CKR_MECHANISM_INVALID;
        }

        switch (pkcs11_mock_active_operation)
        {
                case PKCS11_MOCK_CK_OPERATION_NONE:
                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_ENCRYPT;
                        break;
                case PKCS11_MOCK_CK_OPERATION_DIGEST:
                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_DIGEST_ENCRYPT;
                        break;
                case PKCS11_MOCK_CK_OPERATION_SIGN:
                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_SIGN_ENCRYPT;
                        break;
                default:
                        return CKR_FUNCTION_FAILED;
        }

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_Encrypt)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData, CK_ULONG ulDataLen, CK_BYTE_PTR pEncryptedData, CK_ULONG_PTR pulEncryptedDataLen)
{
        CK_ULONG i = 0;


        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_ENCRYPT != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pData)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulDataLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pulEncryptedDataLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pEncryptedData)
        {
                if (ulDataLen > *pulEncryptedDataLen)
                {
                        return CKR_BUFFER_TOO_SMALL;
                }
                else
                {
                        for (i = 0; i < ulDataLen; i++)
                                pEncryptedData[i] = pData[i] ^ 0xAB;

                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;
                }
        }

        *pulEncryptedDataLen = ulDataLen;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_EncryptUpdate)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart, CK_ULONG ulPartLen, CK_BYTE_PTR pEncryptedPart, CK_ULONG_PTR pulEncryptedPartLen)
{
        CK_ULONG i = 0;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_ENCRYPT != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pPart)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pulEncryptedPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pEncryptedPart)
        {
                if (ulPartLen > *pulEncryptedPartLen)
                {
                        return CKR_BUFFER_TOO_SMALL;
                }
                else
                {
                        for (i = 0; i < ulPartLen; i++)
                                pEncryptedPart[i] = pPart[i] ^ 0xAB;
                }
        }

        *pulEncryptedPartLen = ulPartLen;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_EncryptFinal)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pLastEncryptedPart, CK_ULONG_PTR pulLastEncryptedPartLen)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((PKCS11_MOCK_CK_OPERATION_ENCRYPT != pkcs11_mock_active_operation) &&
                (PKCS11_MOCK_CK_OPERATION_DIGEST_ENCRYPT != pkcs11_mock_active_operation) &&
                (PKCS11_MOCK_CK_OPERATION_SIGN_ENCRYPT != pkcs11_mock_active_operation))
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pulLastEncryptedPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pLastEncryptedPart)
        {
                switch (pkcs11_mock_active_operation)
                {
                        case PKCS11_MOCK_CK_OPERATION_ENCRYPT:
                                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;
                                break;
                        case PKCS11_MOCK_CK_OPERATION_DIGEST_ENCRYPT:
                                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_DIGEST;
                                break;
                        case PKCS11_MOCK_CK_OPERATION_SIGN_ENCRYPT:
                                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_SIGN;
                                break;
                        default:
                                return CKR_FUNCTION_FAILED;
                }
        }

        *pulLastEncryptedPartLen = 0;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_DecryptInit)(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((PKCS11_MOCK_CK_OPERATION_NONE != pkcs11_mock_active_operation) &&
                (PKCS11_MOCK_CK_OPERATION_DIGEST != pkcs11_mock_active_operation) && 
                (PKCS11_MOCK_CK_OPERATION_VERIFY != pkcs11_mock_active_operation))
                return CKR_OPERATION_ACTIVE;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pMechanism)
                return CKR_ARGUMENTS_BAD;

        switch (pMechanism->mechanism)
        {
                case CKM_RSA_PKCS:

                        if ((NULL != pMechanism->pParameter) || (0 != pMechanism->ulParameterLen))
                                return CKR_MECHANISM_PARAM_INVALID;

                        if (PKCS11_MOCK_CK_OBJECT_HANDLE_PRIVATE_KEY != hKey)
                                return CKR_KEY_TYPE_INCONSISTENT;

                        break;

                case CKM_RSA_PKCS_OAEP:

                        if ((NULL == pMechanism->pParameter) || (sizeof(CK_RSA_PKCS_OAEP_PARAMS) != pMechanism->ulParameterLen))
                                return CKR_MECHANISM_PARAM_INVALID;

                        if (PKCS11_MOCK_CK_OBJECT_HANDLE_PRIVATE_KEY != hKey)
                                return CKR_KEY_TYPE_INCONSISTENT;

                        break;

                case CKM_DES3_CBC:

                        if ((NULL == pMechanism->pParameter) || (8 != pMechanism->ulParameterLen))
                                return CKR_MECHANISM_PARAM_INVALID;

                        if (PKCS11_MOCK_CK_OBJECT_HANDLE_SECRET_KEY != hKey)
                                return CKR_KEY_TYPE_INCONSISTENT;

                        break;

                case CKM_AES_CBC:
                        
                        if ((NULL == pMechanism->pParameter) || (16 != pMechanism->ulParameterLen))
                                return CKR_MECHANISM_PARAM_INVALID;

                        if (PKCS11_MOCK_CK_OBJECT_HANDLE_SECRET_KEY != hKey)
                                return CKR_KEY_TYPE_INCONSISTENT;

                        break;

                default:

                        return CKR_MECHANISM_INVALID;
        }

        switch (pkcs11_mock_active_operation)
        {
                case PKCS11_MOCK_CK_OPERATION_NONE:
                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_DECRYPT;
                        break;
                case PKCS11_MOCK_CK_OPERATION_DIGEST:
                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_DECRYPT_DIGEST;
                        break;
                case PKCS11_MOCK_CK_OPERATION_VERIFY:
                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_DECRYPT_VERIFY;
                        break;
                default:
                        return CKR_FUNCTION_FAILED;
        }

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_Decrypt)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pEncryptedData, CK_ULONG ulEncryptedDataLen, CK_BYTE_PTR pData, CK_ULONG_PTR pulDataLen)
{
        CK_ULONG i = 0;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_DECRYPT != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pEncryptedData)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulEncryptedDataLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pulDataLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pData)
        {
                if (ulEncryptedDataLen > *pulDataLen)
                {
                        return CKR_BUFFER_TOO_SMALL;
                }
                else
                {
                        for (i = 0; i < ulEncryptedDataLen; i++)
                                pData[i] = pEncryptedData[i] ^ 0xAB;

                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;
                }
        }

        *pulDataLen = ulEncryptedDataLen;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_DecryptUpdate)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pEncryptedPart, CK_ULONG ulEncryptedPartLen, CK_BYTE_PTR pPart, CK_ULONG_PTR pulPartLen)
{
        CK_ULONG i = 0;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_DECRYPT != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pEncryptedPart)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulEncryptedPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pulPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pPart)
        {
                if (ulEncryptedPartLen > *pulPartLen)
                {
                        return CKR_BUFFER_TOO_SMALL;
                }
                else
                {
                        for (i = 0; i < ulEncryptedPartLen; i++)
                                pPart[i] = pEncryptedPart[i] ^ 0xAB;
                }
        }

        *pulPartLen = ulEncryptedPartLen;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_DecryptFinal)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pLastPart, CK_ULONG_PTR pulLastPartLen)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((PKCS11_MOCK_CK_OPERATION_DECRYPT != pkcs11_mock_active_operation) &&
                (PKCS11_MOCK_CK_OPERATION_DECRYPT_DIGEST != pkcs11_mock_active_operation) &&
                (PKCS11_MOCK_CK_OPERATION_DECRYPT_VERIFY != pkcs11_mock_active_operation))
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pulLastPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pLastPart)
        {
                switch (pkcs11_mock_active_operation)
                {
                        case PKCS11_MOCK_CK_OPERATION_DECRYPT:
                                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;
                                break;
                        case PKCS11_MOCK_CK_OPERATION_DECRYPT_DIGEST:
                                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_DIGEST;
                                break;
                        case PKCS11_MOCK_CK_OPERATION_DECRYPT_VERIFY:
                                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_VERIFY;
                                break;
                        default:
                                return CKR_FUNCTION_FAILED;
                }
        }

        *pulLastPartLen = 0;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_DigestInit)(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism)
{

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((PKCS11_MOCK_CK_OPERATION_NONE != pkcs11_mock_active_operation) &&
                (PKCS11_MOCK_CK_OPERATION_ENCRYPT != pkcs11_mock_active_operation) && 
                (PKCS11_MOCK_CK_OPERATION_DECRYPT != pkcs11_mock_active_operation))
                return CKR_OPERATION_ACTIVE;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pMechanism)
                return CKR_ARGUMENTS_BAD;

        if (CKM_SHA_1 != pMechanism->mechanism)
                return CKR_MECHANISM_INVALID;

        if ((NULL != pMechanism->pParameter) || (0 != pMechanism->ulParameterLen))
                return CKR_MECHANISM_PARAM_INVALID;

        switch (pkcs11_mock_active_operation)
        {
                case PKCS11_MOCK_CK_OPERATION_NONE:
                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_DIGEST;
                        break;
                case PKCS11_MOCK_CK_OPERATION_ENCRYPT:
                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_DIGEST_ENCRYPT;
                        break;
                case PKCS11_MOCK_CK_OPERATION_DECRYPT:
                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_DECRYPT_DIGEST;
                        break;
                default:
                        return CKR_FUNCTION_FAILED;
        }

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_Digest)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData, CK_ULONG ulDataLen, CK_BYTE_PTR pDigest, CK_ULONG_PTR pulDigestLen)
{
        CK_BYTE hash[20] = { 0x7B, 0x50, 0x2C, 0x3A, 0x1F, 0x48, 0xC8, 0x60, 0x9A, 0xE2, 0x12, 0xCD, 0xFB, 0x63, 0x9D, 0xEE, 0x39, 0x67, 0x3F, 0x5E };

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_DIGEST != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pData)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulDataLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pulDigestLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pDigest)
        {
                if (sizeof(hash) > *pulDigestLen)
                {
                        return CKR_BUFFER_TOO_SMALL;
                }
                else
                {
                        memcpy(pDigest, hash, sizeof(hash));
                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;
                }
        }

        *pulDigestLen = sizeof(hash);

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_DigestUpdate)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart, CK_ULONG ulPartLen)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_DIGEST != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pPart)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulPartLen)
                return CKR_ARGUMENTS_BAD;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_DigestKey)(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hKey)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_DIGEST != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (PKCS11_MOCK_CK_OBJECT_HANDLE_SECRET_KEY != hKey)
                return CKR_OBJECT_HANDLE_INVALID;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_DigestFinal)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pDigest, CK_ULONG_PTR pulDigestLen)
{
        CK_BYTE hash[20] = { 0x7B, 0x50, 0x2C, 0x3A, 0x1F, 0x48, 0xC8, 0x60, 0x9A, 0xE2, 0x12, 0xCD, 0xFB, 0x63, 0x9D, 0xEE, 0x39, 0x67, 0x3F, 0x5E };

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((PKCS11_MOCK_CK_OPERATION_DIGEST != pkcs11_mock_active_operation) && 
                (PKCS11_MOCK_CK_OPERATION_DIGEST_ENCRYPT != pkcs11_mock_active_operation) && 
                (PKCS11_MOCK_CK_OPERATION_DECRYPT_DIGEST != pkcs11_mock_active_operation))
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pulDigestLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pDigest)
        {
                if (sizeof(hash) > *pulDigestLen)
                {
                        return CKR_BUFFER_TOO_SMALL;
                }
                else
                {
                        memcpy(pDigest, hash, sizeof(hash));

                        switch (pkcs11_mock_active_operation)
                        {
                                case PKCS11_MOCK_CK_OPERATION_DIGEST:
                                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;
                                        break;
                                case PKCS11_MOCK_CK_OPERATION_DIGEST_ENCRYPT:
                                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_ENCRYPT;
                                        break;
                                case PKCS11_MOCK_CK_OPERATION_DECRYPT_DIGEST:
                                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_DECRYPT;
                                        break;
                                default:
                                        return CKR_FUNCTION_FAILED;
                        }
                }
        }

        *pulDigestLen = sizeof(hash);

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_SignInit)(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((PKCS11_MOCK_CK_OPERATION_NONE != pkcs11_mock_active_operation) &&
                (PKCS11_MOCK_CK_OPERATION_ENCRYPT != pkcs11_mock_active_operation))
                return CKR_OPERATION_ACTIVE;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (hKey >= G_N_ELEMENTS(mock_objects) || mock_objects[hKey].object_class != CKO_PRIVATE_KEY)
                return CKR_KEY_HANDLE_INVALID;

        if (NULL == pMechanism)
                return CKR_ARGUMENTS_BAD;

        mock_sign_algo = pMechanism->mechanism;

        // TODO: Hardcoded list
        if (CKM_RSA_PKCS_PSS == pMechanism->mechanism)
        {
                CK_RSA_PKCS_PSS_PARAMS *params;

                if ((NULL == pMechanism->pParameter) || (0 == pMechanism->ulParameterLen))
                         return CKR_MECHANISM_PARAM_INVALID;

                params = pMechanism->pParameter;

                g_assert (params->hashAlg == CKM_SHA256);
                g_assert (params->mgf == CKG_MGF1_SHA256);
                // if (PKCS11_MOCK_CK_OBJECT_HANDLE_PRIVATE_KEY != hKey)
                //         return CKR_KEY_TYPE_INCONSISTENT;
        }
        else if (CKM_RSA_PKCS == pMechanism->mechanism)
        {
                // FIXME: Also assert SHA256?
        }
        else
        {
                g_assert_not_reached ();
                return CKR_MECHANISM_INVALID;
        }

        if (PKCS11_MOCK_CK_OPERATION_NONE == pkcs11_mock_active_operation)
                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_SIGN;
        else
                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_SIGN_ENCRYPT;

        pkcs11_mock_sign_key = hKey;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_Sign)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData, CK_ULONG ulDataLen, CK_BYTE_PTR pSignature, CK_ULONG_PTR pulSignatureLen)
{
        const gnutls_datum_t data = {
                .data = pData,
                .size = ulDataLen,
        };
        gnutls_datum_t signature;
        int status;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_SIGN != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pData)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulDataLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pulSignatureLen)
                return CKR_ARGUMENTS_BAD;

        // TODO: Handle user not logged in

        // TODO: Hardcoded algo list
        if (mock_sign_algo == CKM_RSA_PKCS_PSS)
                status = gnutls_privkey_sign_hash2 (mock_objects[pkcs11_mock_sign_key].key, GNUTLS_SIGN_RSA_PSS_SHA256,
                                                    GNUTLS_PRIVKEY_SIGN_FLAG_RSA_PSS, &data, &signature);
        else if (mock_sign_algo == CKM_RSA_PKCS)
                status = gnutls_privkey_sign_hash2 (mock_objects[pkcs11_mock_sign_key].key, GNUTLS_SIGN_RSA_SHA256,
                                                    GNUTLS_PRIVKEY_SIGN_FLAG_TLS1_RSA, &data, &signature);
        else
                g_assert_not_reached ();

        // g_assert (status == GNUTLS_E_SUCCESS);
        if (status != GNUTLS_E_SUCCESS)
                return CKR_FUNCTION_FAILED; // TODO: Best return code?

        if (signature.size > *pulSignatureLen)
        {
                gnutls_free (signature.data);
                *pulSignatureLen = signature.size;
                if (pSignature != NULL)
                        return CKR_BUFFER_TOO_SMALL;
        }
        else
        {
                // This is called twice, once with NULL to just query size
                if (pSignature != NULL)
                {
                        memcpy (pSignature, signature.data, signature.size);
                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;
                }
                *pulSignatureLen = signature.size;
                gnutls_free (signature.data);
        }

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_SignUpdate)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart, CK_ULONG ulPartLen)
{

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_SIGN != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pPart)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulPartLen)
                return CKR_ARGUMENTS_BAD;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_SignFinal)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSignature, CK_ULONG_PTR pulSignatureLen)
{
        CK_BYTE signature[10] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 };


        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((PKCS11_MOCK_CK_OPERATION_SIGN != pkcs11_mock_active_operation) && 
                (PKCS11_MOCK_CK_OPERATION_SIGN_ENCRYPT != pkcs11_mock_active_operation))
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pulSignatureLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pSignature)
        {
                if (sizeof(signature) > *pulSignatureLen)
                {
                        return CKR_BUFFER_TOO_SMALL;
                }
                else
                {
                        memcpy(pSignature, signature, sizeof(signature));

                        if (PKCS11_MOCK_CK_OPERATION_SIGN == pkcs11_mock_active_operation)
                                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;
                        else
                                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_ENCRYPT;
                }
        }

        *pulSignatureLen = sizeof(signature);

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_SignRecoverInit)(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_NONE != pkcs11_mock_active_operation)
                return CKR_OPERATION_ACTIVE;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pMechanism)
                return CKR_ARGUMENTS_BAD;

        if (CKM_RSA_PKCS == pMechanism->mechanism)
        {
                if ((NULL != pMechanism->pParameter) || (0 != pMechanism->ulParameterLen))
                        return CKR_MECHANISM_PARAM_INVALID;

                if (PKCS11_MOCK_CK_OBJECT_HANDLE_PRIVATE_KEY != hKey)
                        return CKR_KEY_TYPE_INCONSISTENT;
        }
        else
        {
                return CKR_MECHANISM_INVALID;
        }

        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_SIGN_RECOVER;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_SignRecover)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData, CK_ULONG ulDataLen, CK_BYTE_PTR pSignature, CK_ULONG_PTR pulSignatureLen)
{
        CK_ULONG i = 0;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_SIGN_RECOVER != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pData)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulDataLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pulSignatureLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pSignature)
        {
                if (ulDataLen > *pulSignatureLen)
                {
                        return CKR_BUFFER_TOO_SMALL;
                }
                else
                {
                        for (i = 0; i < ulDataLen; i++)
                                pSignature[i] = pData[i] ^ 0xAB;

                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;
                }
        }

        *pulSignatureLen = ulDataLen;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_VerifyInit)(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((PKCS11_MOCK_CK_OPERATION_NONE != pkcs11_mock_active_operation) &&
                (PKCS11_MOCK_CK_OPERATION_DECRYPT != pkcs11_mock_active_operation))
                return CKR_OPERATION_ACTIVE;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pMechanism)
                return CKR_ARGUMENTS_BAD;

        if ((CKM_RSA_PKCS == pMechanism->mechanism) || (CKM_SHA1_RSA_PKCS == pMechanism->mechanism))
        {
                if ((NULL != pMechanism->pParameter) || (0 != pMechanism->ulParameterLen))
                        return CKR_MECHANISM_PARAM_INVALID;

                if (PKCS11_MOCK_CK_OBJECT_HANDLE_PUBLIC_KEY != hKey)
                        return CKR_KEY_TYPE_INCONSISTENT;
        }
        else
        {
                return CKR_MECHANISM_INVALID;
        }

        if (PKCS11_MOCK_CK_OPERATION_NONE == pkcs11_mock_active_operation)
                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_VERIFY;
        else
                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_DECRYPT_VERIFY;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_Verify)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData, CK_ULONG ulDataLen, CK_BYTE_PTR pSignature, CK_ULONG ulSignatureLen)
{
        CK_BYTE signature[10] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 };


        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_VERIFY != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pData)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulDataLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pSignature)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulSignatureLen)
                return CKR_ARGUMENTS_BAD;

        if (sizeof(signature) != ulSignatureLen)
                return CKR_SIGNATURE_LEN_RANGE;

        if (0 != memcmp(pSignature, signature, sizeof(signature)))
                return CKR_SIGNATURE_INVALID;

        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_VerifyUpdate)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart, CK_ULONG ulPartLen)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_VERIFY != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pPart)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulPartLen)
                return CKR_ARGUMENTS_BAD;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_VerifyFinal)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSignature, CK_ULONG ulSignatureLen)
{
        CK_BYTE signature[10] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 };

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((PKCS11_MOCK_CK_OPERATION_VERIFY != pkcs11_mock_active_operation) &&
                (PKCS11_MOCK_CK_OPERATION_DECRYPT_VERIFY != pkcs11_mock_active_operation))
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pSignature)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulSignatureLen)
                return CKR_ARGUMENTS_BAD;

        if (sizeof(signature) != ulSignatureLen)
                return CKR_SIGNATURE_LEN_RANGE;

        if (0 != memcmp(pSignature, signature, sizeof(signature)))
                return CKR_SIGNATURE_INVALID;

        if (PKCS11_MOCK_CK_OPERATION_VERIFY == pkcs11_mock_active_operation)
                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;
        else
                pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_DECRYPT;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_VerifyRecoverInit)(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_NONE != pkcs11_mock_active_operation)
                return CKR_OPERATION_ACTIVE;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pMechanism)
                return CKR_ARGUMENTS_BAD;

        if (CKM_RSA_PKCS == pMechanism->mechanism)
        {
                if ((NULL != pMechanism->pParameter) || (0 != pMechanism->ulParameterLen))
                        return CKR_MECHANISM_PARAM_INVALID;

                if (PKCS11_MOCK_CK_OBJECT_HANDLE_PUBLIC_KEY != hKey)
                        return CKR_KEY_TYPE_INCONSISTENT;
        }
        else
        {
                return CKR_MECHANISM_INVALID;
        }

        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_VERIFY_RECOVER;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_VerifyRecover)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSignature, CK_ULONG ulSignatureLen, CK_BYTE_PTR pData, CK_ULONG_PTR pulDataLen)
{
        CK_ULONG i = 0;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_VERIFY_RECOVER != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pSignature)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulSignatureLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pulDataLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pData)
        {
                if (ulSignatureLen > *pulDataLen)
                {
                        return CKR_BUFFER_TOO_SMALL;
                }
                else
                {
                        for (i = 0; i < ulSignatureLen; i++)
                                pData[i] = pSignature[i] ^ 0xAB;

                        pkcs11_mock_active_operation = PKCS11_MOCK_CK_OPERATION_NONE;
                }
        }

        *pulDataLen = ulSignatureLen;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_DigestEncryptUpdate)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart, CK_ULONG ulPartLen, CK_BYTE_PTR pEncryptedPart, CK_ULONG_PTR pulEncryptedPartLen)
{
        CK_ULONG i = 0;


        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_DIGEST_ENCRYPT != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pPart)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pulEncryptedPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pEncryptedPart)
        {
                if (ulPartLen > *pulEncryptedPartLen)
                {
                        return CKR_BUFFER_TOO_SMALL;
                }
                else
                {
                        for (i = 0; i < ulPartLen; i++)
                                pEncryptedPart[i] = pPart[i] ^ 0xAB;
                }
        }

        *pulEncryptedPartLen = ulPartLen;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_DecryptDigestUpdate)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pEncryptedPart, CK_ULONG ulEncryptedPartLen, CK_BYTE_PTR pPart, CK_ULONG_PTR pulPartLen)
{
        CK_ULONG i = 0;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_DECRYPT_DIGEST != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pEncryptedPart)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulEncryptedPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pulPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pPart)
        {
                if (ulEncryptedPartLen > *pulPartLen)
                {
                        return CKR_BUFFER_TOO_SMALL;
                }
                else
                {
                        for (i = 0; i < ulEncryptedPartLen; i++)
                                pPart[i] = pEncryptedPart[i] ^ 0xAB;
                }
        }

        *pulPartLen = ulEncryptedPartLen;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_SignEncryptUpdate)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart, CK_ULONG ulPartLen, CK_BYTE_PTR pEncryptedPart, CK_ULONG_PTR pulEncryptedPartLen)
{
        CK_ULONG i = 0;


        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_SIGN_ENCRYPT != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pPart)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pulEncryptedPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pEncryptedPart)
        {
                if (ulPartLen > *pulEncryptedPartLen)
                {
                        return CKR_BUFFER_TOO_SMALL;
                }
                else
                {
                        for (i = 0; i < ulPartLen; i++)
                                pEncryptedPart[i] = pPart[i] ^ 0xAB;
                }
        }

        *pulEncryptedPartLen = ulPartLen;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_DecryptVerifyUpdate)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pEncryptedPart, CK_ULONG ulEncryptedPartLen, CK_BYTE_PTR pPart, CK_ULONG_PTR pulPartLen)
{
        CK_ULONG i = 0;

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_OPERATION_DECRYPT_VERIFY != pkcs11_mock_active_operation)
                return CKR_OPERATION_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pEncryptedPart)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulEncryptedPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pulPartLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pPart)
        {
                if (ulEncryptedPartLen > *pulPartLen)
                {
                        return CKR_BUFFER_TOO_SMALL;
                }
                else
                {
                        for (i = 0; i < ulEncryptedPartLen; i++)
                                pPart[i] = pEncryptedPart[i] ^ 0xAB;
                }
        }

        *pulPartLen = ulEncryptedPartLen;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GenerateKey)(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phKey)
{
        CK_ULONG i = 0;


        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pMechanism)
                return CKR_ARGUMENTS_BAD;

        if (CKM_DES3_KEY_GEN != pMechanism->mechanism)
                return CKR_MECHANISM_INVALID;

        if ((NULL != pMechanism->pParameter) || (0 != pMechanism->ulParameterLen))
                return CKR_MECHANISM_PARAM_INVALID;

        if (NULL == pTemplate)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulCount)
                return CKR_ARGUMENTS_BAD;

        if (NULL == phKey)
                return CKR_ARGUMENTS_BAD;

        for (i = 0; i < ulCount; i++)
        {
                if (NULL == pTemplate[i].pValue)
                        return CKR_ATTRIBUTE_VALUE_INVALID;

                if (0 >= pTemplate[i].ulValueLen)
                        return CKR_ATTRIBUTE_VALUE_INVALID;
        }

        *phKey = PKCS11_MOCK_CK_OBJECT_HANDLE_SECRET_KEY;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GenerateKeyPair)(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_ATTRIBUTE_PTR pPublicKeyTemplate, CK_ULONG ulPublicKeyAttributeCount, CK_ATTRIBUTE_PTR pPrivateKeyTemplate, CK_ULONG ulPrivateKeyAttributeCount, CK_OBJECT_HANDLE_PTR phPublicKey, CK_OBJECT_HANDLE_PTR phPrivateKey)
{
        CK_ULONG i = 0;


        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pMechanism)
                return CKR_ARGUMENTS_BAD;

        if (CKM_RSA_PKCS_KEY_PAIR_GEN != pMechanism->mechanism)
                return CKR_MECHANISM_INVALID;

        if ((NULL != pMechanism->pParameter) || (0 != pMechanism->ulParameterLen))
                return CKR_MECHANISM_PARAM_INVALID;

        if (NULL == pPublicKeyTemplate)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulPublicKeyAttributeCount)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pPrivateKeyTemplate)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulPrivateKeyAttributeCount)
                return CKR_ARGUMENTS_BAD;

        if (NULL == phPublicKey)
                return CKR_ARGUMENTS_BAD;

        if (NULL == phPrivateKey)
                return CKR_ARGUMENTS_BAD;

        for (i = 0; i < ulPublicKeyAttributeCount; i++)
        {
                if (NULL == pPublicKeyTemplate[i].pValue)
                        return CKR_ATTRIBUTE_VALUE_INVALID;

                if (0 >= pPublicKeyTemplate[i].ulValueLen)
                        return CKR_ATTRIBUTE_VALUE_INVALID;
        }

        for (i = 0; i < ulPrivateKeyAttributeCount; i++)
        {
                if (NULL == pPrivateKeyTemplate[i].pValue)
                        return CKR_ATTRIBUTE_VALUE_INVALID;

                if (0 >= pPrivateKeyTemplate[i].ulValueLen)
                        return CKR_ATTRIBUTE_VALUE_INVALID;
        }

        *phPublicKey = PKCS11_MOCK_CK_OBJECT_HANDLE_PUBLIC_KEY;
        *phPrivateKey = PKCS11_MOCK_CK_OBJECT_HANDLE_PRIVATE_KEY;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_WrapKey)(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hWrappingKey, CK_OBJECT_HANDLE hKey, CK_BYTE_PTR pWrappedKey, CK_ULONG_PTR pulWrappedKeyLen)
{
        CK_BYTE wrappedKey[10] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 };


        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pMechanism)
                return CKR_ARGUMENTS_BAD;

        if (CKM_RSA_PKCS != pMechanism->mechanism)
                return CKR_MECHANISM_INVALID;

        if ((NULL != pMechanism->pParameter) || (0 != pMechanism->ulParameterLen))
                return CKR_MECHANISM_PARAM_INVALID;

        if (PKCS11_MOCK_CK_OBJECT_HANDLE_PUBLIC_KEY != hWrappingKey)
                return CKR_KEY_HANDLE_INVALID;

        if (PKCS11_MOCK_CK_OBJECT_HANDLE_SECRET_KEY != hKey)
                return CKR_KEY_HANDLE_INVALID;

        if (NULL != pWrappedKey)
        {
                if (sizeof(wrappedKey) > *pulWrappedKeyLen)
                        return CKR_BUFFER_TOO_SMALL;
                else
                        memcpy(pWrappedKey, wrappedKey, sizeof(wrappedKey));
        }

        *pulWrappedKeyLen = sizeof(wrappedKey);

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_UnwrapKey)(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hUnwrappingKey, CK_BYTE_PTR pWrappedKey, CK_ULONG ulWrappedKeyLen, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulAttributeCount, CK_OBJECT_HANDLE_PTR phKey)
{
        CK_ULONG i = 0;


        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pMechanism)
                return CKR_ARGUMENTS_BAD;

        if (CKM_RSA_PKCS != pMechanism->mechanism)
                return CKR_MECHANISM_INVALID;

        if ((NULL != pMechanism->pParameter) || (0 != pMechanism->ulParameterLen))
                return CKR_MECHANISM_PARAM_INVALID;

        if (PKCS11_MOCK_CK_OBJECT_HANDLE_PRIVATE_KEY != hUnwrappingKey)
                return CKR_KEY_HANDLE_INVALID;

        if (NULL == pWrappedKey)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulWrappedKeyLen)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pTemplate)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulAttributeCount)
                return CKR_ARGUMENTS_BAD;

        if (NULL == phKey)
                return CKR_ARGUMENTS_BAD;

        for (i = 0; i < ulAttributeCount; i++)
        {
                if (NULL == pTemplate[i].pValue)
                        return CKR_ATTRIBUTE_VALUE_INVALID;

                if (0 >= pTemplate[i].ulValueLen)
                        return CKR_ATTRIBUTE_VALUE_INVALID;
        }

        *phKey = PKCS11_MOCK_CK_OBJECT_HANDLE_SECRET_KEY;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_DeriveKey)(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hBaseKey, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulAttributeCount, CK_OBJECT_HANDLE_PTR phKey)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pMechanism)
                return CKR_ARGUMENTS_BAD;

        if (CKM_XOR_BASE_AND_DATA != pMechanism->mechanism)
                return CKR_MECHANISM_INVALID;

        if ((NULL == pMechanism->pParameter) || (sizeof(CK_KEY_DERIVATION_STRING_DATA) != pMechanism->ulParameterLen))
                return CKR_MECHANISM_PARAM_INVALID;

        if (PKCS11_MOCK_CK_OBJECT_HANDLE_SECRET_KEY != hBaseKey)
                return CKR_OBJECT_HANDLE_INVALID;

        if (NULL == phKey)
                return CKR_ARGUMENTS_BAD;

        *phKey = PKCS11_MOCK_CK_OBJECT_HANDLE_SECRET_KEY;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_SeedRandom)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSeed, CK_ULONG ulSeedLen)
{

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == pSeed)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulSeedLen)
                return CKR_ARGUMENTS_BAD;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GenerateRandom)(CK_SESSION_HANDLE hSession, CK_BYTE_PTR RandomData, CK_ULONG ulRandomLen)
{

        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        if (NULL == RandomData)
                return CKR_ARGUMENTS_BAD;

        if (0 >= ulRandomLen)
                return CKR_ARGUMENTS_BAD;

        memset(RandomData, 1, ulRandomLen);

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_GetFunctionStatus)(CK_SESSION_HANDLE hSession)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;
        
        return CKR_FUNCTION_NOT_PARALLEL;
}


CK_DEFINE_FUNCTION(CK_RV, C_CancelFunction)(CK_SESSION_HANDLE hSession)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;
        
        return CKR_FUNCTION_NOT_PARALLEL;
}


CK_DEFINE_FUNCTION(CK_RV, C_WaitForSlotEvent)(CK_FLAGS flags, CK_SLOT_ID_PTR pSlot, CK_VOID_PTR pReserved)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((0 != flags)  && (CKF_DONT_BLOCK != flags))
                return CKR_ARGUMENTS_BAD;

        if (NULL == pSlot)
                return CKR_ARGUMENTS_BAD;

        if (NULL != pReserved)
                return CKR_ARGUMENTS_BAD;

        return CKR_NO_EVENT;
}


CK_DEFINE_FUNCTION(CK_RV, C_GetUnmanagedStructSizeList)(CK_ULONG_PTR pSizeList, CK_ULONG_PTR pulCount)
{
        CK_ULONG sizes[] = {
                sizeof(CK_ATTRIBUTE),
                sizeof(CK_C_INITIALIZE_ARGS),
                sizeof(CK_FUNCTION_LIST),
                sizeof(CK_INFO),
                sizeof(CK_MECHANISM),
                sizeof(CK_MECHANISM_INFO),
                sizeof(CK_SESSION_INFO),
                sizeof(CK_SLOT_INFO),
                sizeof(CK_TOKEN_INFO),
                sizeof(CK_VERSION),
                sizeof(CK_AES_CBC_ENCRYPT_DATA_PARAMS),
                sizeof(CK_AES_CTR_PARAMS),
                sizeof(CK_CMS_SIG_PARAMS),
                sizeof(CK_DES_CBC_ENCRYPT_DATA_PARAMS),
                sizeof(CK_ECDH1_DERIVE_PARAMS),
                sizeof(CK_ECDH2_DERIVE_PARAMS),
                sizeof(CK_ECMQV_DERIVE_PARAMS),
                sizeof(CK_EXTRACT_PARAMS),
                sizeof(CK_KEA_DERIVE_PARAMS),
                sizeof(CK_KEY_DERIVATION_STRING_DATA),
                sizeof(CK_KEY_WRAP_SET_OAEP_PARAMS),
                sizeof(CK_MAC_GENERAL_PARAMS),
                sizeof(CK_PBE_PARAMS),
                sizeof(CK_PKCS5_PBKD2_PARAMS),
                sizeof(CK_RC2_CBC_PARAMS),
                sizeof(CK_RC2_MAC_GENERAL_PARAMS),
                sizeof(CK_RC2_PARAMS),
                sizeof(CK_RC5_CBC_PARAMS),
                sizeof(CK_RC5_MAC_GENERAL_PARAMS),
                sizeof(CK_RC5_PARAMS),
                sizeof(CK_RSA_PKCS_OAEP_PARAMS),
                sizeof(CK_RSA_PKCS_PSS_PARAMS),
                sizeof(CK_SKIPJACK_PRIVATE_WRAP_PARAMS),
                sizeof(CK_SKIPJACK_RELAYX_PARAMS),
                sizeof(CK_SSL3_KEY_MAT_OUT),
                sizeof(CK_SSL3_KEY_MAT_PARAMS),
                sizeof(CK_SSL3_MASTER_KEY_DERIVE_PARAMS),
                sizeof(CK_SSL3_RANDOM_DATA),
                sizeof(CK_TLS_PRF_PARAMS),
                sizeof(CK_WTLS_KEY_MAT_OUT),
                sizeof(CK_WTLS_KEY_MAT_PARAMS),
                sizeof(CK_WTLS_MASTER_KEY_DERIVE_PARAMS),
                sizeof(CK_WTLS_PRF_PARAMS),
                sizeof(CK_WTLS_RANDOM_DATA),
                sizeof(CK_X9_42_DH1_DERIVE_PARAMS),
                sizeof(CK_X9_42_DH2_DERIVE_PARAMS),
                sizeof(CK_X9_42_MQV_DERIVE_PARAMS),
        };

        CK_ULONG sizes_count = sizeof(sizes) / sizeof(CK_ULONG);

        if (NULL == pulCount)
                return CKR_ARGUMENTS_BAD;

        if (NULL == pSizeList)
        {
                *pulCount = sizes_count;
        }
        else
        {
                if (sizes_count > *pulCount)
                        return CKR_BUFFER_TOO_SMALL;

                memcpy(pSizeList, sizes, sizeof(sizes));
                *pulCount = sizes_count;
        }

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_EjectToken)(CK_SLOT_ID slotID)
{
        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if (PKCS11_MOCK_CK_SLOT_ID != slotID)
                return CKR_SLOT_ID_INVALID;

        return CKR_OK;
}


CK_DEFINE_FUNCTION(CK_RV, C_InteractiveLogin)(CK_SESSION_HANDLE hSession)
{
        CK_RV rv = CKR_OK;


        if (CK_FALSE == pkcs11_mock_initialized)
                return CKR_CRYPTOKI_NOT_INITIALIZED;

        if ((CK_FALSE == pkcs11_mock_session_opened) || (PKCS11_MOCK_CK_SESSION_ID != hSession))
                return CKR_SESSION_HANDLE_INVALID;

        switch (pkcs11_mock_session_state)
        {
                case CKS_RO_PUBLIC_SESSION:

                        pkcs11_mock_session_state = CKS_RO_USER_FUNCTIONS;

                        break;

                case CKS_RO_USER_FUNCTIONS:
                case CKS_RW_USER_FUNCTIONS:

                        rv = CKR_USER_ALREADY_LOGGED_IN;

                        break;

                case CKS_RW_PUBLIC_SESSION:

                        pkcs11_mock_session_state = CKS_RW_USER_FUNCTIONS;

                        break;

                case CKS_RW_SO_FUNCTIONS:

                        rv = CKR_USER_ANOTHER_ALREADY_LOGGED_IN;

                        break;
        }

        return rv;
}

/* LCOV_EXCL_STOP */
