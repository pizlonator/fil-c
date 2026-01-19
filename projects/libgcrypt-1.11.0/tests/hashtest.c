/* hashtest.c - Check the hash functions
 * Copyright (C) 2013 g10 Code GmbH
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../src/gcrypt-int.h"

#include "stopwatch.h"

#define PGM "hashtest"
#include "t-common.h"

static int use_hugeblock;
static int missing_test_vectors;

static struct {
  int algo;
  int gigs;
  int bytes;
  const char *hex;
} testvectors[] = {
  { GCRY_MD_SHA1, 256, -64, "92fc51850c7b750e6e774b75f294f6979d4059f0" },
  { GCRY_MD_SHA1, 256,  -1, "4bddeeb4c08683f02d4944d93dbcb02ebab50134" },
  { GCRY_MD_SHA1, 256,  -0, "71b923afde1c8c040884c723a2e3335b333e64c6" },
  { GCRY_MD_SHA1, 256,   1, "2d99f9b5b86e9c9c937104f4242bd6b8bc0927ef" },
  { GCRY_MD_SHA1, 256,  64, "a60dabe8d749f798b7ec3a684cc3eab487451482" },

  { GCRY_MD_SHA224, 256, -64,
    "b5672b54d2480a5688a2dc727a1ad4db7a81ef31ce8999e0bbaeffdc" },
  { GCRY_MD_SHA224, 256,  -1,
    "814ea7159473e6ffc1c64b90026a542e13ac6980f7f3ca3c4582a9b8" },
  { GCRY_MD_SHA224, 256,   0,
    "9ec0e1829455db8650ec7a8b06912196f97a7358bc3a73c79911cd4e" },
  { GCRY_MD_SHA224, 256,   1,
    "e578d5d523320876565bbbc892511a485427caee6dd754d57e3e58c2" },
  { GCRY_MD_SHA224, 256,  64,
    "ff0464df248cd298b63765bc4f87f21e25c93c657fdf3656d3c878e5" },

  { GCRY_MD_SHA256, 256, -64,
    "87a9828d3de78d55d252341db2a622908c4e0ceaee9961ecf9768700fc799ec8" },
  { GCRY_MD_SHA256, 256,  -1,
    "823bf95f64ef04a4a77579c38760b1d401b56bf3a8e664bdf56ca15afb468a03" },
  { GCRY_MD_SHA256, 256,   0,
    "2d0723878cb2c3d5c59dfad910cdb857f4430a6ba2a7d687938d7a20e63dde47" },
  { GCRY_MD_SHA256, 256,   1,
    "5a2e21b1e79cd866acf53a2a18ca76bd4e02c4b01bf4627354171824c812d95f" },
  { GCRY_MD_SHA256, 256,  64,
    "34444808af8e9d995e67f9e155ed94bf55f195a51dc1d8a989e6bcf95511c8a2" },

  { GCRY_MD_SHA512, 256, -64,
    "e01bf8140874bf240e8426cb2bcbc377cbed2e6037334116637149e1cd8cd462"
    "96828b71f32b9f002771d4cb51172ce578b73b7939221e4df655ecd08601e655" },
  { GCRY_MD_SHA512, 256,  -1,
    "4917ff94514b1757705c289fdc3e7d6ffcce5771b20ae237ebc03d2ec9eb435f"
    "b7ce9f0e27272be8cced77a5edae1a01a0ad62b0a44169d88bbee45474a17734" },
  { GCRY_MD_SHA512, 256,   0,
    "1e28e8b3c79f2f47da11f3c0b7da4e7981e7d932db6d17d528a31e191922edda"
    "8fc4bb2df10ea876232db5a1c606bc41886e8b2c570a3e721221f60c8c7dc4ab" },
  { GCRY_MD_SHA512, 256,   1,
    "027d3324dd1cf127770ceb53681f4c70937c9bca4e3acd5fd76cb266c7d4527d"
    "58140290a1822e8d60c4d3ae9725fb923183230d6dfd2d7d73c0d74a4757f34a" },
  { GCRY_MD_SHA512, 256,  64,
    "49920704ea9d6ee19f0742d6c868110fa3eda8ac09f026e9ef22cc731af53020"
    "de40eedef66cb1afd94c61e285fa9327e01336e804903740a9145ab1f065c2d5" },

  { GCRY_MD_SHA3_512, 256, -64,
    "c6e082b3db996dbe5f2c5709818a7f325ef4febd883d7e9c545c06bfa7225198"
    "1ecf40103788913cd5a5bdf13246b952ded6651043684b24197eb23544882a97" },
  { GCRY_MD_SHA3_512, 256,  -1,
    "d7bf28e8216bf7d3d0d3969e34078e94b98598e17b6f21f256379389e4eba8ee"
    "74eb288774797263fec00bdfd357d132cea9e408be36b982f5a60ab56ad01613" },
  { GCRY_MD_SHA3_512, 256,  +0,
    "c1270852ba7b1e1a3eaa777969b8a65be28c3894537c61eb8cd22b1df6af703d"
    "b59939f6adadeb64317faece8167d4817e73daf73e28a5ccd26bebee0a35c322" },
  { GCRY_MD_SHA3_512, 256,  +1,
    "8bdfeb3a1a9a1cdcef21172cbc5bb3b87c0d8f7111df0aaf7f1bc03ad4775bd6"
    "a03e0a875c4e7d02d2230c213562c6a57be28d92eaf6e4bea4bc24690454c8ef" },
  { GCRY_MD_SHA3_512, 256, +64,
    "0c91b91665ceaf7af5102e0ed31aa4f050668ab3c57b1f4763946d567efe66b3"
    "ab9a2016cf238dee5b44eae9f0cdfbf7b7a6eb1e759986273243dc35894706b6" },

  { GCRY_MD_SM3, 256, -64,
    "4ceb893abeb43965d4cac7626da9a4be895585b5b2f16f302626801308b1c02a" },
  { GCRY_MD_SM3, 256, -1,
    "825f01e4f2b6084136abc356fa1b343a9411d844a4dc1474293aad817cd2a48f" },
  { GCRY_MD_SM3, 256, +0,
    "d948a4025ac3ea0aa8989f43203411bd22ad17eaa5fd92ebdf9cabf869f1ba1b" },
  { GCRY_MD_SM3, 256, +1,
    "4f6d0e260299c1f286ef1dbb4638a0770979f266b6c007c55410ee6849cba2a8" },
  { GCRY_MD_SM3, 256, +64,
    "ed34869dbadd62e3bec1f511004d7bbfc9cafa965477cc48843b248293bbe867" },

  { GCRY_MD_BLAKE2S_256, 256, -64,
    "8a3d4f712275e8e8da70c76501cce364c75f8dd09748be58cf63c9ce38d62627" },
  { GCRY_MD_BLAKE2S_256, 256, -1,
    "0c01c9ad1e60e27dc889f2c9034a949ca8b9a9dc90dd99be64963af306d47b92" },
  { GCRY_MD_BLAKE2S_256, 256, +0,
    "f8c43d5c4bad93aca702c8c466987c5ac5e640a29b37dd9904252ff27b2348a0" },
  { GCRY_MD_BLAKE2S_256, 256, +1,
    "24c34b167b4eea1a7eb7d572ff3cf669a9856ea91bb112e9ef2ccd4b1aceccb4" },
  { GCRY_MD_BLAKE2S_256, 256, +64,
    "2f8d754f98e2d4ed7744389f89d0bdb9b770c9fa215b8badd3129ea1364af867" },

  { GCRY_MD_BLAKE2B_512, 256, -64,
    "36d32ae4deeacab4119401c52e2aec5545675bd2dce4f67871ddc73671a05f94"
    "e8332c2a31f32f5601878606a571aa7b43029dac3ae71cf9ef141d05651dc4bf" },
  { GCRY_MD_BLAKE2B_512, 256, -1,
    "b5dc439f51664a6c9cbc87e2de98ce608ac4064a779e5140909d75d2120c9b2a"
    "a1d4ae7be9c1ba97025be91ddcfbe42c791c3231cffbfa4b5368ba18f9590e1b" },
  { GCRY_MD_BLAKE2B_512, 256, +0,
    "c413d011ba9abbf118dd96bfc827f5fd94493d8350df9f7aff834faace5adba2"
    "0c3037069dfb2c81718ffc7b418ce1c1320d334b6fe8cddfb5d2dd19eb530853" },
  { GCRY_MD_BLAKE2B_512, 256, +1,
    "b6dfb821f1c8167fb33995c29485010da56abd539c3d04ab9c222844301b8bba"
    "6f57a48e45a748e40847084b93f26706aae82212550671c736becffcc6fb1496" },
  { GCRY_MD_BLAKE2B_512, 256, +64,
    "8c21316a4a02044e302d503d0fe669d905c40d9d80ecd5aafc8e30f1df06736f"
    "51fdaf6002160bb8fe4e868eaad9623fc5ecdd728bcbfee4a19b386503710f48" },

  { GCRY_MD_WHIRLPOOL, 256, -64,
    "aabf62344c1aa82d2dc7605f339b3571d540f1f320f97e6a8c0229645ee61f1f"
    "da796acde2f96caa1c56eb2c2f9a6029a6242ad690479def66feac44334cc3af" },
  { GCRY_MD_WHIRLPOOL, 256, -1,
    "9a35ec14aa9cefd40e04295d45d39f3111a98c2d76d90c54a7d2b8f2f5b9302b"
    "79663eab6b6674625c3ae3e4b5dbb3b0a2f5b2f49a7a59cd1723e2b16a3efea2" },
  { GCRY_MD_WHIRLPOOL, 256, +0,
    "818ad31a5110b6217cc6ffa099d554aaadc9566bf5291e104a5d58b21d51ae4d"
    "c216c6de888d1359066c584e24e6606f530a3fce80ef78aed8564de4a28801c8" },
  { GCRY_MD_WHIRLPOOL, 256, +1,
    "298805f5fc68488712427c1bcb27581d91aa04337c1c6b4657489ed3d239bb8b"
    "c70ef654065d380ac1f5596aca5cb59e6da8044b5a067e32ea4cd94ca606f9f3" },
  { GCRY_MD_WHIRLPOOL, 256, +64,
    "7bd35c3bee621bc0fb8907904b3b84d6cf4fae4c22cc64fbc744c8c5c8de806d"
    "0f11a27892d531dc907426597737762c83e3ddcdc62f50d16d130aaefaeec436" },

  { GCRY_MD_SHA1, 6, -64,
    "eeee82d952403313bd63d6d7c8e342df0a1eea77" },
  { GCRY_MD_SHA1, 6, -1,
    "8217b9f987d67db5880bcfff1d6763a6514d629f" },
  { GCRY_MD_SHA1, 6, +0,
    "2b38aa63c05668217e5331320a4aee0adad7fc3b" },
  { GCRY_MD_SHA1, 6, +1,
    "f3222de4d0704554cff0a537bc95b30f15daa94f" },
  { GCRY_MD_SHA1, 6, +64,
    "b3bdd8065bb92d8208d55d28fad2281c6fbf2601" },

  { GCRY_MD_SHA256, 6, -64,
    "a2d5add5be904b70d6ef9bcd5feb9c6cfc2be0799732a122d9eccb576ff5a922" },
  { GCRY_MD_SHA256, 6, -1,
    "88293b7e0e5a47fdef1148c6e510f95272770db6b5296958380209ba57db7a5d" },
  { GCRY_MD_SHA256, 6, +0,
    "ccee8e8dfc366eba67471e49c45057b0041be0d2206c6de1aa765ce07ecfc434" },
  { GCRY_MD_SHA256, 6, +1,
    "f4a89e92b38e0e61ee17079dc31411de06cfe1f77c83095ae1a2e7aa0205d94b" },
  { GCRY_MD_SHA256, 6, +64,
    "338708608c2356ed2927a85b08fe745223c6140243fb3a87f309e12b31b946a8" },

  { GCRY_MD_SHA512, 6, -64,
    "658f52850932633c00b2f1d65b874c540ab84e2c0fe84a8a6c35f8e90e6f6a9c"
    "2f7e0ccca5064783562a42ad8f47eab48687aaf6998b04ee94441e82c14e834d" },
  { GCRY_MD_SHA512, 6, -1,
    "9ead6d66b46a3a72d77c7990874cfebc1575e5bfda6026430d76b3db6cc62d52"
    "4ca0dd2674b9c24208b2e780d75542572eee8df6724acadcc23a03eed8f82f0a" },
  { GCRY_MD_SHA512, 6, +0,
    "03e4549eb28bd0fb1606c321f1498503b5e889bec8d799cf0688567c7f8ac0d9"
    "a7ec4e84d1d729d6a359797656e286617c3ef82abb51991bb576aaf05f7b6573" },
  { GCRY_MD_SHA512, 6, +1,
    "ffe52f6385ccde6fa7d45845787d8f9993fdcb5833fb58b13c424a84e39ea50f"
    "52d40e254fe667cb0104ffe3837dc8d0eee3c81721cb8eac10d5851dfb1f91db" },
  { GCRY_MD_SHA512, 6, +64,
    "4a19da3d5eaaa79ac1eaff5e4062f23ee56573411f8d302f7bf3c6da8779bd00"
    "a936e9ad7f535597a49162ed308b0cced7724667f97a1bb24540152fcfe3ec95" },

  { GCRY_MD_SHA3_512, 6, -64,
    "a99f2913d3beb9b45273402e30daa4d25c7a5e9eb8cf6039996eb2292a45c04c"
    "b9e3a1a187f71920626f465ed6cf7dc34047ec5578e05516374bb9c56683903a" },
  { GCRY_MD_SHA3_512, 6, -1,
    "fca50bde79c55e5fc4c9d97e66eb5cfacef7032395848731e645ca42f07f8d38"
    "be1d593727c2a82b9a9bc058ebc9744971f867fa920cfa902023448243ac017b" },
  { GCRY_MD_SHA3_512, 6, +0,
    "c61bb345c0a553edaa89fd38114ac9799b6d307ba8e3cde53552ad4c77cfe4b7"
    "2671d82c1519c8e7b23153a9268e2939239564fc7c2060608aa42955e938840d" },
  { GCRY_MD_SHA3_512, 6, +1,
    "502a83d8d1b977312806382a45c1cc9c0e7db437ca962e37eb181754d59db686"
    "14d91df286d510411adf69f7c9befc1027bdc0c33a48a5dd6ae0957b9061e7ca" },
  { GCRY_MD_SHA3_512, 6, +64,
    "207bfb83ae788ddd4531188567f0892bbddbbc88d69bc196b2357bee3e668706"
    "c27f832ecb50e9ae5b63e9f384bdc37373958d4a14f3825146d2f6b1a65d8e51" },

  { GCRY_MD_SM3, 6, -64,
    "41d96d19cef4c942b0f5f4cdc3e1afe440dc62c0bc103a2c0e9eee9e1733a74a" },
  { GCRY_MD_SM3, 6, -1,
    "b7689cc4ef6c7dc795b9e5e6998e5cc3dc1daec02bc1181cdbef8d6812b4957a" },
  { GCRY_MD_SM3, 6, +0,
    "c6eae4a82052423cf98017bde4dee8769947c66120a1a2ff79f0f0dc945a3272" },
  { GCRY_MD_SM3, 6, +1,
    "f6590f161fee11529585c7a9dfc725f8b81951e49b616844097a3dbdc9ffdbec" },
  { GCRY_MD_SM3, 6, +64,
    "f3277fa90c47afe5e4fc52374aadf8e96bc29c2b5a7a4ebf5d704245ada837ea" },

  { GCRY_MD_BLAKE2S_256, 6, -64,
    "0f3c17610777c34d40a0d11a93d5e5ed444ce16edefebabd0bc8e30392d5c2db" },
  { GCRY_MD_BLAKE2S_256, 6, -1,
    "92cbcf142c45de9d64da9791c51dce4e32b58f74d9f3d201b1ea74deac765f51" },
  { GCRY_MD_BLAKE2S_256, 6, +0,
    "b20702cb5a0bee2ab104f38eb513429589310a7edde81dd1f40043be7d16d0de" },
  { GCRY_MD_BLAKE2S_256, 6, +1,
    "bfc17dc74930989841da05aac08402bf0dcb4a597b17c52402a516ea7e541cdf" },
  { GCRY_MD_BLAKE2S_256, 6, +64,
    "d85588cdf5a00bec1327da02f22f1a10b68dd9d6b730f30a3aa65af3a51c1722" },

  { GCRY_MD_BLAKE2B_512, 6, -64,
    "30b6015f94524861b04b83f0455be10a993460e0f8f0fd755fc3d0270b0c7d00"
    "039a6e01684ce0689ce4ef70932bd19a676acf4b4ea521c30337d2f445fc2055" },
  { GCRY_MD_BLAKE2B_512, 6, -1,
    "49abef820ad7fc5e6ed9b63acddce639a69dcd749b0798b140216649bc3b927c"
    "637dbe1cb39a41bbafe7f8b675401ccdcf69a7fba227ae4cda5cd28b9ff36776" },
  { GCRY_MD_BLAKE2B_512, 6, +0,
    "4182a7307a89391b78af9dbc3ba1e8d643708abbed5919086aa6e2bc65ae9597"
    "e40229450c86ac5d3117b006427dd0131f5ae4c1a1d64c81420d2731536c81d8" },
  { GCRY_MD_BLAKE2B_512, 6, +1,
    "33c0d9e65b1b18e9556134a08c1e725c19155bbf6ed4349d7d6d678f1827fef3"
    "74b6e3381471f3d3fff7ffbcb9474ce9038143b99e25cd5f8afbb336313d4648" },
  { GCRY_MD_BLAKE2B_512, 6, +64,
    "d2d7f388611af78a2ea40b06f99993cff156afd25cbc47695bdb567d4d35b992"
    "0ff8c325c359a2bdeddf54ececc671ac7b981031e90a7d63d6e0415ec4484282" },

  { GCRY_MD_WHIRLPOOL, 6, -64,
    "247707d1f9cf31b90ee68527144b1c20ad5ce96293bdccd1a81c8f40bc9df10c"
    "e7441ac3b3097162d6fbf4d4b67b8fa09de451e2d920f16aad78c47ab00cb833" },
  { GCRY_MD_WHIRLPOOL, 6, -1,
    "af49e4a553bdbec1fdafc41713029e0fb1666894753c0ab3ecb280fc5af6eff8"
    "253120745a229d7a8b5831711e4fd16ed0741258504d8a47e2b42aa2f1886968" },
  { GCRY_MD_WHIRLPOOL, 6, +0,
    "f269ffa424bc2aad2da654f01783fc9b2b431219f2b05784d718da0935e78792"
    "9207b000ebbfb63dfdcc8adf8e5bd321d9616c1b8357430b9be6cb4640df8609" },
  { GCRY_MD_WHIRLPOOL, 6, +1,
    "52b77eb13129151b69b63c09abb655dc9cb046cafd4cbf7d4a82ae04b61ef9e6"
    "531dde04cae7c5ab400ed8ee8da2e3f490d177289b2b3aa29b12b292954b902c" },
  { GCRY_MD_WHIRLPOOL, 6, +64,
    "60a950c92f3f08abbc81c41c86ce0463679ffd5ab420e988e15b210615b454ae"
    "69607d14a1806fa44aacf8c926fbdcee998af46f56e0c642d3fb4ee54c8fb917" },

  { GCRY_MD_CRC32, 6, -64, "20739052" },
  { GCRY_MD_CRC32, 6, -1,  "971a5a74" },
  { GCRY_MD_CRC32, 6, +0,  "bf48113c" },
  { GCRY_MD_CRC32, 6, +1,  "c7678ad5" },
  { GCRY_MD_CRC32, 6, +64, "1efa7255" },

  { GCRY_MD_CRC24_RFC2440, 6, -64, "747e81" },
  { GCRY_MD_CRC24_RFC2440, 6, -1,  "deb97d" },
  { GCRY_MD_CRC24_RFC2440, 6, +0,  "7d5bea" },
  { GCRY_MD_CRC24_RFC2440, 6, +1,  "acc351" },
  { GCRY_MD_CRC24_RFC2440, 6, +64, "9d9032" },

  { 0 }
};


static void
showhex (const void *buffer, size_t buflen, const char *format, ...)
{
  va_list arg_ptr;
  const unsigned char *s;

  fprintf (stderr, "%s: ", PGM);
  va_start (arg_ptr, format);
  vfprintf (stderr, format, arg_ptr);
  va_end (arg_ptr);

  for (s=buffer; buflen; buflen--, s++)
    fprintf (stderr, "%02x", *s);
  putc ('\n', stderr);
}


static void
show_note (const char *format, ...)
{
  va_list arg_ptr;

  if (!verbose && getenv ("srcdir"))
    fputs ("      ", stderr);  /* To align above "PASS: ".  */
  else
    fprintf (stderr, "%s: ", PGM);
  va_start (arg_ptr, format);
  vfprintf (stderr, format, arg_ptr);
  if (*format && format[strlen(format)-1] != '\n')
    putc ('\n', stderr);
  va_end (arg_ptr);
}

/* Convert STRING consisting of hex characters into its binary
   representation and return it as an allocated buffer. The valid
   length of the buffer is returned at R_LENGTH.  The string is
   delimited by end of string.  The function returns NULL on
   error.  */
static void *
hex2buffer (const char *string, size_t *r_length)
{
  const char *s;
  unsigned char *buffer;
  size_t length;

  buffer = xmalloc (strlen(string)/2+1);
  length = 0;
  for (s=string; *s; s +=2 )
    {
      if (!hexdigitp (s) || !hexdigitp (s+1))
        return NULL;           /* Invalid hex digits. */
      ((unsigned char*)buffer)[length++] = xtoi_2 (s);
    }
  *r_length = length;
  return buffer;
}


static void
run_selftest (int algo)
{
  gpg_error_t err;
  size_t n;

  n = 1;
  err = gcry_md_algo_info (algo, GCRYCTL_SELFTEST, NULL, &n);
  if (err && gpg_err_code (err) != GPG_ERR_NOT_IMPLEMENTED)
    fail ("extended selftest for %s (%d) failed: %s",
          gcry_md_algo_name (algo), algo, gpg_strerror (err));
  else if (err && verbose)
    info ("extended selftest for %s (%d) not implemented",
          gcry_md_algo_name (algo), algo);
  else if (verbose)
    info ("extended selftest for %s (%d) passed",
          gcry_md_algo_name (algo), algo);
}

/* Compare DIGEST of length DIGESTLEN generated using ALGO and GIGS
   plus BYTES with the test vector and print an error message if the
   don't match.  Return 0 on match.  */
static int
cmp_digest (const unsigned char *digest, size_t digestlen,
            int algo, int gigs, int bytes)
{
  int idx;
  unsigned char *tv_digest;
  size_t tv_digestlen = 0;

  for (idx=0; testvectors[idx].algo; idx++)
    {
      if (testvectors[idx].algo == algo
          && testvectors[idx].gigs == gigs
          && testvectors[idx].bytes == bytes)
        break;
    }
  if (!testvectors[idx].algo)
    {
      info ("%d GiB %+3d %-10s warning: %s",
            gigs, bytes, gcry_md_algo_name (algo), "no test vector");
      missing_test_vectors++;
      return 1;
    }

  tv_digest = hex2buffer (testvectors[idx].hex, &tv_digestlen);
  if (tv_digestlen != digestlen) /* Ooops.  */
    {
      fail ("%d GiB %+3d %-10s error: %s",
            gigs, bytes, gcry_md_algo_name (algo), "digest length mismatch");
      xfree (tv_digest);
      return 1;
    }
  if (memcmp (tv_digest, digest, tv_digestlen))
    {
      fail ("%d GiB %+3d %-10s error: %s",
            gigs, bytes, gcry_md_algo_name (algo), "mismatch");
      xfree (tv_digest);
      return 1;
    }
  xfree (tv_digest);

  return 0;
}


static void
run_longtest (int algo, int gigs)
{
  gpg_error_t err;
  gcry_md_hd_t hd;
  gcry_md_hd_t hd_pre = NULL;
  gcry_md_hd_t hd_pre2 = NULL;
  gcry_md_hd_t hd_post = NULL;
  gcry_md_hd_t hd_post2 = NULL;
  char pattern[1024];
  char *hugepattern = NULL;
  size_t hugesize;
  size_t hugegigs;
  int i, g, gppos, gptot;
  const unsigned char *digest;
  unsigned int digestlen;

  memset (pattern, 'a', sizeof pattern);

  if (use_hugeblock)
    {
      hugegigs = 5;
      if (sizeof(size_t) >= 8)
        {
          hugesize = hugegigs*1024*1024*1024;
          hugepattern = malloc(hugesize);
          if (hugepattern != NULL)
            memset(hugepattern, 'a', hugesize);
          else
            show_note ("failed to allocate %zu GiB huge pattern block: %s",
                       hugegigs, strerror(errno));
        }
      else
        show_note ("cannot allocate %zu GiB huge pattern block on 32-bit system",
                   hugegigs);
    }
  if (hugepattern == NULL)
    {
      hugegigs = 0;
      hugesize = 0;
    }

  err = gcry_md_open (&hd, algo, 0);
  if (err)
    {
      fail ("gcry_md_open failed for %s (%d): %s",
            gcry_md_algo_name (algo), algo, gpg_strerror (err));
      free(hugepattern);
      return;
    }

  digestlen = gcry_md_get_algo_dlen (algo);

  gppos = 0;
  gptot = 0;
  for (g=0; g < gigs; )
    {
      if (gppos >= 16)
        {
          gptot += 16;
          gppos -= 16;
          show_note ("%d GiB so far hashed with %s", gptot,
                     gcry_md_algo_name (algo));
        }
      if (g == gigs - 1)
        {
          for (i = 0; i < 1024*1023; i++)
            gcry_md_write (hd, pattern, sizeof pattern);
          for (i = 0; i < 1023; i++)
            gcry_md_write (hd, pattern, sizeof pattern);
          err = gcry_md_copy (&hd_pre, hd);
          if (!err)
            err = gcry_md_copy (&hd_pre2, hd);
          if (err)
            die ("gcry_md_copy failed for %s (%d): %s",
                 gcry_md_algo_name (algo), algo, gpg_strerror (err));
          gcry_md_write (hd, pattern, sizeof pattern);
          g++;
          gppos++;
        }
      else if (hugepattern != NULL && gigs - g > hugegigs)
        {
          gcry_md_write (hd, hugepattern, hugesize);
          g += hugegigs;
          gppos += hugegigs;
        }
      else
        {
          for (i = 0; i < 1024*1024; i++)
            gcry_md_write (hd, pattern, sizeof pattern);
          g++;
          gppos++;
        }
    }
  if (g >= 16 && gppos)
    show_note ("%d GiB hashed with %s", g, gcry_md_algo_name (algo));

  err = gcry_md_copy (&hd_post, hd);
  if (err)
    die ("gcry_md_copy failed for %s (%d): %s",
         gcry_md_algo_name (algo), algo, gpg_strerror (err));
  err = gcry_md_copy (&hd_post2, hd);
  if (err)
    die ("gcry_md_copy failed for %s (%d): %s",
         gcry_md_algo_name (algo), algo, gpg_strerror (err));

  gcry_md_write (hd_pre2, pattern, sizeof pattern - 64);
  gcry_md_write (hd_pre, pattern, sizeof pattern - 1);
  gcry_md_write (hd_post, pattern, 1);
  gcry_md_write (hd_post2, pattern, 64);

  digest = gcry_md_read (hd_pre2, algo);
  if (cmp_digest (digest, digestlen, algo, gigs, -64) || verbose)
    showhex (digest, digestlen, "%d GiB %+3d %-10s ",
             gigs, -64, gcry_md_algo_name (algo));
  digest = gcry_md_read (hd_pre, algo);
  if (cmp_digest (digest, digestlen, algo, gigs, -1) || verbose)
    showhex (digest, digestlen, "%d GiB %+3d %-10s ",
             gigs, -1, gcry_md_algo_name (algo));
  digest = gcry_md_read (hd, algo);
  if (cmp_digest (digest, digestlen, algo, gigs, 0) || verbose)
    showhex (digest, digestlen, "%d GiB %+3d %-10s ",
             gigs, 0, gcry_md_algo_name (algo));
  digest = gcry_md_read (hd_post, algo);
  if (cmp_digest (digest, digestlen, algo, gigs, 1) || verbose)
    showhex (digest, digestlen, "%d GiB %+3d %-10s ",
             gigs, 1, gcry_md_algo_name (algo));
  digest = gcry_md_read (hd_post2, algo);
  if (cmp_digest (digest, digestlen, algo, gigs, 64) || verbose)
    showhex (digest, digestlen, "%d GiB %+3d %-10s ",
             gigs, 64, gcry_md_algo_name (algo));

  gcry_md_close (hd);
  gcry_md_close (hd_pre);
  gcry_md_close (hd_pre2);
  gcry_md_close (hd_post);
  gcry_md_close (hd_post2);

  free(hugepattern);
}


int
main (int argc, char **argv)
{
  int last_argc = -1;
  int gigs = 0;
  int algo = 0;
  int idx;

  if (argc)
    { argc--; argv++; }

  while (argc && last_argc != argc )
    {
      last_argc = argc;
      if (!strcmp (*argv, "--"))
        {
          argc--; argv++;
          break;
        }
      else if (!strcmp (*argv, "--help"))
        {
          fputs ("usage: " PGM " [options] [algos]\n"
                 "Options:\n"
                 "  --verbose                 print timings etc.\n"
                 "  --debug                   flyswatter\n"
                 "  --hugeblock               Use 5 GiB pattern block\n"
                 "  --gigs N                  Run a test on N GiB\n"
                 "  --disable-hwf <features>  Disable hardware acceleration feature(s)\n"
                 "                            for benchmarking.\n",
                 stdout);
          exit (0);
        }
      else if (!strcmp (*argv, "--verbose"))
        {
          verbose++;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--debug"))
        {
          verbose += 2;
          debug++;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--hugeblock"))
        {
          use_hugeblock = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--gigs"))
        {
          argc--; argv++;
          if (argc)
            {
              gigs = atoi (*argv);
              argc--; argv++;
            }
        }
      else if (!strcmp (*argv, "--disable-hwf"))
        {
          argc--;
          argv++;
          if (argc)
            {
              if (gcry_control (GCRYCTL_DISABLE_HWF, *argv, NULL))
                fprintf (stderr,
                        PGM
                        ": unknown hardware feature `%s' - option ignored\n",
                        *argv);
              argc--;
              argv++;
            }
        }
      else if (!strncmp (*argv, "--", 2))
        die ("unknown option '%s'", *argv);
    }

  if (gigs < 0 || gigs > 1024*1024)
    die ("value for --gigs must be in the range 0 to %d", 1024*1024);

  xgcry_control ((GCRYCTL_DISABLE_SECMEM, 0));
  if (!gcry_check_version (GCRYPT_VERSION))
    die ("version mismatch\n");
  if (debug)
    xgcry_control ((GCRYCTL_SET_DEBUG_FLAGS, 1u , 0));
  xgcry_control ((GCRYCTL_ENABLE_QUICK_RANDOM, 0));
  xgcry_control ((GCRYCTL_INITIALIZATION_FINISHED, 0));

  /* A quick check that all given algorithms are valid.  */
  for (idx=0; idx < argc; idx++)
    {
      algo = gcry_md_map_name (argv[idx]);
      if (!algo)
        fail ("invalid algorithm '%s'", argv[idx]);
    }
  if (error_count)
    exit (1);

  /* Start checking.  */
  start_timer ();
  if (!argc)
    {
      for (algo=1; algo < 400; algo++)
        if (!gcry_md_test_algo (algo))
          {
            if (!gigs)
              run_selftest (algo);
            else
              run_longtest (algo, gigs);
          }
     }
  else
    {
      for (idx=0; idx < argc; idx++)
        {
          algo = gcry_md_map_name (argv[idx]);
          if (!algo)
            die ("invalid algorithm '%s'", argv[idx]);

          if (!gigs)
            run_selftest (algo);
          else
            run_longtest (algo, gigs);
        }
    }
  stop_timer ();

  if (missing_test_vectors)
    fail ("Some test vectors are missing");

  if (verbose)
    info ("All tests completed in %s.  Errors: %d\n",
          elapsed_time (1), error_count);
  return !!error_count;
}
