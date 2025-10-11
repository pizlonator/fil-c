#include "internal/deprecated.h"

#include <openssl/camellia.h>
#include "cmll_local.h"
#include <string.h>
#include <stdlib.h>

#include <stdfil.h>

int Camellia_Ekeygen(int keyBitLength, const u8* rawKey, KEY_TABLE_TYPE k)
{
    ZSAFETY_CHECK(keyBitLength == 128 || keyBitLength == 192 || keyBitLength == 256);
    zcheck_readonly(rawKey, (keyBitLength + 7) / 8);
    zcheck(k, sizeof(KEY_TABLE_TYPE));
    return zunsafe_fast_call("Camellia_Ekeygen", keyBitLength, rawKey, k);
}

void Camellia_EncryptBlock_Rounds(int grandRounds, const u8 plaintext[],
                                  const KEY_TABLE_TYPE keyTable,
                                  u8 ciphertext[])
{
    zcheck_readonly(plaintext, 16);
    zcheck_readonly(keyTable, sizeof(KEY_TABLE_TYPE));
    zcheck(ciphertext, 16);
    zunsafe_fast_call("Camellia_EncryptBlock_Rounds", grandRounds, plaintext, keyTable, ciphertext);
}

void Camellia_EncryptBlock(int keyBitLength, const u8 plaintext[],
                           const KEY_TABLE_TYPE keyTable, u8 ciphertext[])
{
    ZSAFETY_CHECK(keyBitLength == 128 || keyBitLength == 192 || keyBitLength == 256);
    zcheck_readonly(plaintext, 16);
    zcheck_readonly(keyTable, sizeof(KEY_TABLE_TYPE));
    zcheck(ciphertext, 16);
    zunsafe_fast_call("Camellia_EncryptBlock", keyBitLength, plaintext, keyTable, ciphertext);
}

void Camellia_DecryptBlock_Rounds(int grandRounds, const u8 ciphertext[],
                                  const KEY_TABLE_TYPE keyTable,
                                  u8 plaintext[])
{
    zcheck_readonly(ciphertext, 16);
    zcheck_readonly(keyTable, sizeof(KEY_TABLE_TYPE));
    zcheck(plaintext, 16);
    zunsafe_fast_call("Camellia_DecryptBlock_Rounds", grandRounds, ciphertext, keyTable, plaintext);
}

void Camellia_DecryptBlock(int keyBitLength, const u8 ciphertext[],
                           const KEY_TABLE_TYPE keyTable, u8 plaintext[])
{
    ZSAFETY_CHECK(keyBitLength == 128 || keyBitLength == 192 || keyBitLength == 256);
    zcheck_readonly(ciphertext, 16);
    zcheck_readonly(keyTable, sizeof(KEY_TABLE_TYPE));
    zcheck(plaintext, 16);
    zunsafe_fast_call("Camellia_DecryptBlock", keyBitLength, ciphertext, keyTable, plaintext);
}

void Camellia_cbc_encrypt(const unsigned char *in, unsigned char *out,
                          size_t len, const CAMELLIA_KEY *key,
                          unsigned char *ivec, const int enc)
{
    zcheck_readonly(in, len);
    zcheck(out, len);
    zcheck_readonly(key, sizeof(CAMELLIA_KEY));
    zcheck(ivec, 16);
    zunsafe_buf_call(len, "Camellia_cbc_encrypt", in, out, len, key, ivec, enc);
}
