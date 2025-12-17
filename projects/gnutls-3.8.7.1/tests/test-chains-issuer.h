/*
 * Copyright (C) 2008-2014 Free Software Foundation, Inc.
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * Authors: Simon Josefsson, Nikos Mavrogiannopoulos, Martin Ukrop,
 *   Sahana Prasad, Daiki Ueno
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 */

#ifndef GNUTLS_TESTS_TEST_CHAINS_ISSUER_H
#define GNUTLS_TESTS_TEST_CHAINS_ISSUER_H

#define MAX_CHAIN 15

#define SERVER_CERT                                                          \
	"-----BEGIN CERTIFICATE-----\n"                                      \
	"MIIDATCCAbmgAwIBAgIUQdvdegP8JFszFHLfV4+lrEdafzAwPQYJKoZIhvcNAQEK\n" \
	"MDCgDTALBglghkgBZQMEAgGhGjAYBgkqhkiG9w0BAQgwCwYJYIZIAWUDBAIBogMC\n" \
	"AUAwDzENMAsGA1UEAxMEQ0EtNTAgFw0yMDA0MjAxMTI2NDFaGA85OTk5MTIzMTIz\n" \
	"NTk1OVowEzERMA8GA1UEAxMIc2VydmVyLTYwgZswEAYHKoZIzj0CAQYFK4EEACMD\n" \
	"gYYABAHZ3W5jpYq15WI7tVZxWCT3YtYMEj4xJSdO/ubHV0NnrlQ7+Q95R32qcA2w\n" \
	"4gyPif+M/Au4Towr/RA+b+qgMvD0fQFmNeWkNB/TSW2RNm7uHQU7N66tbrNWvjyS\n" \
	"BZeLB/V03ZWe+rO4cfrPiqtBv9N08k9uMNNCeMlatJNqj0BoFRxhBaN3MHUwDAYD\n" \
	"VR0TAQH/BAIwADAUBgNVHREEDTALgglsb2NhbGhvc3QwDwYDVR0PAQH/BAUDAweA\n" \
	"ADAdBgNVHQ4EFgQUMnSJQI2iHiVoxE1XSByQ9QFrG0owHwYDVR0jBBgwFoAUu9ao\n" \
	"G/58Y/+czHPyWo3C+vs9pFkwPQYJKoZIhvcNAQEKMDCgDTALBglghkgBZQMEAgGh\n" \
	"GjAYBgkqhkiG9w0BAQgwCwYJYIZIAWUDBAIBogMCAUADggEBAAfhLT1jQsc9yk4k\n" \
	"myAAMIXYD1THMkasGZiIv2TLJSLeKc4Rvzvrb/iywwrMdaBHs5sJoyk7amMwemc7\n" \
	"WA2+A2uTeLeDG3ev4r5stNRLyL0HSOr7da+BshUiHJgeihp1Qglm0AUqV5X69i5t\n" \
	"5woB5KENnYfoAWaYmXa1EPRh2xb2XDI0uCHg1bPljg61/T2cJZ4VfkOvsKgFAI4p\n" \
	"lAKQCZSKbEY1oWDdDhVcSipYu2E88RXczvcnEQV3C3p6CGcf8xclZdZIwMAyXYAK\n" \
	"oNccbSIfDlN4iD+2bztCRWHD6hWL1NJsFqmv3Ts8eYU8z8J8NdhtCXr76lFkFmDx\n" \
	"+lfZEv4=\n"                                                         \
	"-----END CERTIFICATE-----\n"

#define CA_CERT_5                                                            \
	"-----BEGIN CERTIFICATE-----\n"                                      \
	"MIIDojCCAlqgAwIBAgIUHRb3xJ2ZGqqgdC/pBq/sDtAwvtowPQYJKoZIhvcNAQEK\n" \
	"MDCgDTALBglghkgBZQMEAgGhGjAYBgkqhkiG9w0BAQgwCwYJYIZIAWUDBAIBogMC\n" \
	"AUAwDzENMAsGA1UEAxMEQ0EtNDAgFw0yMDA0MjAxMTI2NDFaGA85OTk5MTIzMTIz\n" \
	"NTk1OVowDzENMAsGA1UEAxMEQ0EtNTCCAVIwPQYJKoZIhvcNAQEKMDCgDTALBglg\n" \
	"hkgBZQMEAgGhGjAYBgkqhkiG9w0BAQgwCwYJYIZIAWUDBAIBogMCAUADggEPADCC\n" \
	"AQoCggEBAMZqQ7I1HAxkxuwGQBch/jZTWLXRUtWBjlpREnp0wFt+quJOZkKNYrlL\n" \
	"9sngiRknsbEIfJMB2XfoK6m9SwRN/qoxewOrnK9YONG9dj0p30qiseshXIs6ZoMl\n" \
	"v9fZA77UraCtTbX6Xwk/+Or6SuSK2lyz0R5O14xBa5ubpm2Q8XTE9A1SAGx61ofC\n" \
	"Dzfvefp+m3QCy+3K+Yn05VKPxswznuVwM/oJDGzJJhD6/uNPpm5CZoPtcW14Eitu\n" \
	"ip51Ej1VE4lJRBHAtUSOrd3Hks6YasK7Uvu0HjpqW7PqaIhJIR7ofzbXX2vBwVj2\n" \
	"Qlwozk4cVCP7XO3VrVu/GCdSL+G3RAUCAwEAAaNkMGIwDwYDVR0TAQH/BAUwAwEB\n" \
	"/zAPBgNVHQ8BAf8EBQMDB4QAMB0GA1UdDgQWBBS71qgb/nxj/5zMc/JajcL6+z2k\n" \
	"WTAfBgNVHSMEGDAWgBQPB7C8f3nco30et23Lhw7QMTaLYzA9BgkqhkiG9w0BAQow\n" \
	"MKANMAsGCWCGSAFlAwQCAaEaMBgGCSqGSIb3DQEBCDALBglghkgBZQMEAgGiAwIB\n" \
	"QAOCAQEAl90uQvD0lne4jseHNfu8XCIZmCSxaNhF3SD73TwlGERbRjtIKz34Y6hC\n" \
	"z5bZ4tCGnkKAtdHLIGwOnaLSXDvzmUSkQmJmG0QMaDGsVpVXEZD/7+yyIxOcV1iK\n" \
	"XveeQysCKsDEfdrfn1mACQj8eC4lL9KJcHptHdTSLfa58MV2Qe5smCIByXxendO5\n" \
	"UQHZy5UrzWAdtO7y75vXeXynsXAqcE4TTNjdFiCnn6Q5/pVyW14kepfjaOzQFP7H\n" \
	"QlnHtgQDRAlQuB1aGseb6jn2Joy33itpBthvtgBosZIqsMyPoX5YzjqZUSjfPZOP\n" \
	"/aOd/5HR4ZPDWfHdIWbXogYX0ndhNg==\n"                                 \
	"-----END CERTIFICATE-----\n"

#define CA_CERT_4                                                            \
	"-----BEGIN CERTIFICATE-----\n"                                      \
	"MIIDojCCAlqgAwIBAgIUGybZZ1e/iFUKafPdh8xUbh7YVnwwPQYJKoZIhvcNAQEK\n" \
	"MDCgDTALBglghkgBZQMEAgGhGjAYBgkqhkiG9w0BAQgwCwYJYIZIAWUDBAIBogMC\n" \
	"AUAwDzENMAsGA1UEAxMEQ0EtMzAgFw0yMDA0MjAxMTI2NDFaGA85OTk5MTIzMTIz\n" \
	"NTk1OVowDzENMAsGA1UEAxMEQ0EtNDCCAVIwPQYJKoZIhvcNAQEKMDCgDTALBglg\n" \
	"hkgBZQMEAgGhGjAYBgkqhkiG9w0BAQgwCwYJYIZIAWUDBAIBogMCAUADggEPADCC\n" \
	"AQoCggEBAM0vsCM3XxZVHmxOdY2ndCoUHnrlLameRZcEupa77oAXBw9J2ysTIY1v\n" \
	"uP7GbBru4JnBhdem1xL37z0/a5O9+5Rw4SNHNw8Z2jPtWSJd+XwfBshQnX66IvSv\n" \
	"M0etutgO/lZwFq7E4yGI7LS1sGWvVhmjMLT1Yb3j/b8SXeSHyp9J0NdJ1spjjekg\n" \
	"bdiMUOo6Tt1gnZsgLdH6Cbmw4sm/+EGjsPOYdBI0kHW5qqLnIzW/io0NMnRsDBEk\n" \
	"HgXNEMhXZL/qEQfrcSCxjlqB126aALHIvN5TKBrssfE6zn9m96A9qCRJuKGP9NPm\n" \
	"4AFkV1yylCUTUkIRkbqPlI4i1vf8jfcCAwEAAaNkMGIwDwYDVR0TAQH/BAUwAwEB\n" \
	"/zAPBgNVHQ8BAf8EBQMDB4QAMB0GA1UdDgQWBBQPB7C8f3nco30et23Lhw7QMTaL\n" \
	"YzAfBgNVHSMEGDAWgBRjNOT1/2J+aAVCl/aO+EQke/8oETA9BgkqhkiG9w0BAQow\n" \
	"MKANMAsGCWCGSAFlAwQCAaEaMBgGCSqGSIb3DQEBCDALBglghkgBZQMEAgGiAwIB\n" \
	"QAOCAQEAsKDivFD4DflylFdG4zijGrtq/zfSKTiNWxZsLKbMwLoG+Km3dy0HWfUq\n" \
	"TUETPEfQlpXc2Tg1tGxFepAPavVeMIy/MV3SsmjRA3f+PNWjaZUxa9+Jd1y6ONwK\n" \
	"wQ7s/JNNk/SZt4bKjX9GrTscZmOVtrwpZ6uQBHITScsr4V431G6wojZ09iEG0yFQ\n" \
	"ZD8ECn2ZOPVQXIswa75NelcGKup838HoDIjQ3vIvrx8rqf5HRg4t9mXzjECzXHVy\n" \
	"8wDamoE3fLAZZX2RxOWnHfjI8qB83qYyR5kN002EFJ/e060SPia1rTHyLqLngRtq\n" \
	"xgR9bRjZf++h/dg6L87b26J5KdDafw==\n"                                 \
	"-----END CERTIFICATE-----\n"

#define CA_CERT_3                                                            \
	"-----BEGIN CERTIFICATE-----\n"                                      \
	"MIIDojCCAlqgAwIBAgIUHRkWa8ZOaRrqjxigoEhxJHMLM2UwPQYJKoZIhvcNAQEK\n" \
	"MDCgDTALBglghkgBZQMEAgGhGjAYBgkqhkiG9w0BAQgwCwYJYIZIAWUDBAIBogMC\n" \
	"AUAwDzENMAsGA1UEAxMEQ0EtMjAgFw0yMDA0MjAxMTI2NDFaGA85OTk5MTIzMTIz\n" \
	"NTk1OVowDzENMAsGA1UEAxMEQ0EtMzCCAVIwPQYJKoZIhvcNAQEKMDCgDTALBglg\n" \
	"hkgBZQMEAgGhGjAYBgkqhkiG9w0BAQgwCwYJYIZIAWUDBAIBogMCAUADggEPADCC\n" \
	"AQoCggEBAMNSjDqpdcx+02E2vKRB78Z6rYRTuYHeXZGIsVz3LXHxplNYtSlM0MN4\n" \
	"cj0mHj2Rctxk7o6vsQm37ayvO4mquvgPiwtivq+qPv98ZTIuVYkPE4NEPru7Uec+\n" \
	"HQO3faRym4VAzpH+CllMraeaSjQLfAKqXw60UHF+b+ovJXKWbb+keahXT6lWxuxY\n" \
	"pm5vbcDg0Ez++9TJcA0MiPKtk4SMgnmr+2vXAE0tE5PRX9NS7AWPyEg82q+ph2kj\n" \
	"zu5VWoqZp/EwMI6VfLJeemY726LyyOpIqBGWwsUXPn5NdxLla58zHDFggd7/Z/l9\n" \
	"aBfozSdrqW3sWeYzgGxeZmnc5Vm/r6ECAwEAAaNkMGIwDwYDVR0TAQH/BAUwAwEB\n" \
	"/zAPBgNVHQ8BAf8EBQMDB4QAMB0GA1UdDgQWBBRjNOT1/2J+aAVCl/aO+EQke/8o\n" \
	"ETAfBgNVHSMEGDAWgBTVuTCwy3TqMVX2Bvdj/wcoYSTG/zA9BgkqhkiG9w0BAQow\n" \
	"MKANMAsGCWCGSAFlAwQCAaEaMBgGCSqGSIb3DQEBCDALBglghkgBZQMEAgGiAwIB\n" \
	"QAOCAQEAbIw3qtl/QAMJ7OmBPqSMtZv9TaLxfUh7FrqfsKjXBQGVX6/7heO+wCwJ\n" \
	"/1vi2yFUc7uoB3ivEKzUQvtP7Nu6WMM64pAfYadGIk4TYV+tgXF4FJ8FHjTek+Lv\n" \
	"jTu7jvLbRSHkBQFimWorPfgf15nlXSCBtejEwvDLXlptLbKEa3q7VFXDzCyeiKGb\n" \
	"IHRozrAP5qiyIjYFJevXrZ/7bWDwMcJrB0uSQN9TD2mJjNXTCHu3GYnEmnu7KRpb\n" \
	"M3OdswIyjIFYvwlYGe2+GbigSaMZY9KCHR7vkJ1JGdxfh+CADcbL4fwj3kOpyEoe\n" \
	"TTqtWQ93AfQnd2Vm3/SAr/+jSuMbSA==\n"                                 \
	"-----END CERTIFICATE-----\n"

#define CA_CERT_2                                                            \
	"-----BEGIN CERTIFICATE-----\n"                                      \
	"MIIDojCCAlqgAwIBAgIUVd3TT33d1fy/8INiIKhudYmRE5swPQYJKoZIhvcNAQEK\n" \
	"MDCgDTALBglghkgBZQMEAgGhGjAYBgkqhkiG9w0BAQgwCwYJYIZIAWUDBAIBogMC\n" \
	"AUAwDzENMAsGA1UEAxMEQ0EtMTAgFw0yMDA0MjAxMTI2NDFaGA85OTk5MTIzMTIz\n" \
	"NTk1OVowDzENMAsGA1UEAxMEQ0EtMjCCAVIwPQYJKoZIhvcNAQEKMDCgDTALBglg\n" \
	"hkgBZQMEAgGhGjAYBgkqhkiG9w0BAQgwCwYJYIZIAWUDBAIBogMCAUADggEPADCC\n" \
	"AQoCggEBANN3n02MYdl70xAq39SUtcMcNR9Zpe6m4SkHcL/1T4YEpWxqqez1tDW3\n" \
	"1My9Std/sE1e63Q+XJdZhKz1v2KM48iMMeEtJRtriSMxp3KyHQwOxV5L/C5yudYG\n" \
	"3DW0XwrIFL5uXn0z27vYTJ+63RFD4K6Np3ROa2EnHuTcb1pAlrGK1erUzuD8gg7m\n" \
	"mIwxfS7KSeUSmZiXVACNVGmAekClRIf1kMjMqNL6eQ2laNcg7W7RCaIghk58E4Ej\n" \
	"/dyNWTgUUoHla8X4Za/JNXDVHdj5VKIfK8xQkc6aN8Ip5rm9J94yLay27QZdHPQn\n" \
	"AlHEW6IAyRgj/lo+yk1RUigjko62t+0CAwEAAaNkMGIwDwYDVR0TAQH/BAUwAwEB\n" \
	"/zAPBgNVHQ8BAf8EBQMDB4QAMB0GA1UdDgQWBBTVuTCwy3TqMVX2Bvdj/wcoYSTG\n" \
	"/zAfBgNVHSMEGDAWgBS/OulsZ80Bb9MpqM/M1lCC8bO2AzA9BgkqhkiG9w0BAQow\n" \
	"MKANMAsGCWCGSAFlAwQCAaEaMBgGCSqGSIb3DQEBCDALBglghkgBZQMEAgGiAwIB\n" \
	"QAOCAQEAfi/KKbJUsdvS/XDqR6T8VHNhX8lMOGdzHltjBdXdxsWlr2mRolILhyZf\n" \
	"1/wf58b1OE4AlxbwH+S/vWrQ2KVwBfWxtTJXqAMSvHIF3Tq8bIghvhK8CmZG/I49\n" \
	"FTYE+42MFBr6f5SNp9Q+ZUcjSK5DO7yNiyKDFfNffFGxHmnmGj2LhgyrvYA/aNyB\n" \
	"2ichlfihcKkExGBN44ODoK+8/W8oiMt541AvPyJxTJjxWjeJ42EBXO+J5k8wRuCu\n" \
	"nXCW5OjnEIExXGKZLlieH4t8kUyHlrTlHO7spiqA/QM7GUtBQfJTLdPFmvHU3Jtw\n" \
	"qGN2PrhXyLoaUfIpNbWO9Jmj2GYaWg==\n"                                 \
	"-----END CERTIFICATE-----\n"

#define CA_CERT_1                                                            \
	"-----BEGIN CERTIFICATE-----\n"                                      \
	"MIICxjCCAiegAwIBAgIUKnsCQlR0jpxEnpzqxbi+Y2rqwpMwCgYIKoZIzj0EAwQw\n" \
	"DzENMAsGA1UEAxMEQ0EtMDAgFw0yMDA0MjAxMTI2NDFaGA85OTk5MTIzMTIzNTk1\n" \
	"OVowDzENMAsGA1UEAxMEQ0EtMTCCAVIwPQYJKoZIhvcNAQEKMDCgDTALBglghkgB\n" \
	"ZQMEAgGhGjAYBgkqhkiG9w0BAQgwCwYJYIZIAWUDBAIBogMCAUADggEPADCCAQoC\n" \
	"ggEBAOqrWIctrZ7mabfoFuMsT/B2kK4vWAGX32SGQdoDKdy+O0jGJN8/vGnbaOWN\n" \
	"k6sR/eNx+13LahbiLl3dzyecdJ6BeDBokjiRXtDzZN3IdrR6KZ5NjqcMiVBgztoq\n" \
	"gkOglhcixU2cMlSFYCozfvf3i4YElJzSP4XdJbLaPcsHmywny52s06vf64SbNhQy\n" \
	"GucRYO0VqRUVCNpvPyyGlkODlDQuzNsd5nIQZ5WR1bQLTYsVoHVfpLx+Su7BAV05\n" \
	"D5XiGQVGw7kkp4VKHrMhQ0VY+34xmahQvnoqfPEBG9jjfy6psI0oa52JS3FBWF8u\n" \
	"psUiFD2iqQy+efQX44gAdrrnkt0CAwEAAaNkMGIwDwYDVR0TAQH/BAUwAwEB/zAP\n" \
	"BgNVHQ8BAf8EBQMDB4QAMB0GA1UdDgQWBBS/OulsZ80Bb9MpqM/M1lCC8bO2AzAf\n" \
	"BgNVHSMEGDAWgBRBWngghShY2X+P7m45LPH1V4p5czAKBggqhkjOPQQDBAOBjAAw\n" \
	"gYgCQgHnvF1Dq32xBBEME4UlVsVeOflvGw5Sr/hVhbUZ1KfAQIV2ZuBuvJNMBrj8\n" \
	"Pzi/nhRuV8vH5xabyQb9RYVcJ8oilQJCAdduIVVvL6DmUBOJfz1znsxPA5JCBBY2\n" \
	"pAOhFZBrNXE2zZrgttgR6TG4Obst1fQzL3RsmqAYAuWSpKPNz6Hdq+kl\n"         \
	"-----END CERTIFICATE-----\n"

#define CA_CERT_0                                                            \
	"-----BEGIN CERTIFICATE-----\n"                                      \
	"MIIB7TCCAU6gAwIBAgIUWmldb3tGP48wFh5P/cmVytYv5JcwCgYIKoZIzj0EAwQw\n" \
	"DzENMAsGA1UEAxMEQ0EtMDAgFw0yMDA0MjAxMTI2NDFaGA85OTk5MTIzMTIzNTk1\n" \
	"OVowDzENMAsGA1UEAxMEQ0EtMDCBmzAQBgcqhkjOPQIBBgUrgQQAIwOBhgAEAarU\n" \
	"aZXDJBYLdRdjV43Nq+slYxPPn877UBJ63K6GQF1poMaSFFJ7qSXi4lJngh7ueCVq\n" \
	"mJvNH54KbqkPryfCKjUbAZnIQa/8zpPbrZ4iAP6d+Mb6qIkX8j3BP1f6Ap0WTmQk\n" \
	"s5QHCkJFGNqqljut/RQgnbTUbQcGHCNmUx4g0BZv03+Qo0MwQTAPBgNVHRMBAf8E\n" \
	"BTADAQH/MA8GA1UdDwEB/wQFAwMHBgAwHQYDVR0OBBYEFEFaeCCFKFjZf4/ubjks\n" \
	"8fVXinlzMAoGCCqGSM49BAMEA4GMADCBiAJCAcmtP2IVnOTF2wHhfUn13qsUpqyc\n" \
	"3kCI1ueg75NgR7xgpL9JQ1CnPaUbCp+5ROKf5IHn8f1jjZIu45WpiWhnZDkkAkIA\n" \
	"pCTZn7t7memhMJUqrHGywx2gR9fgID/REZUZdVe9KcTzWvwSrbffDMCcf10SpM6C\n" \
	"/YXiDLiWNiK+WV8Z557eWKI=\n"                                         \
	"-----END CERTIFICATE-----\n"

#define UNRELATED                                                            \
	"-----BEGIN CERTIFICATE-----\n"                                      \
	"MIIEaDCCAqCgAwIBAgIMWXi5rBKSNwkPo4olMD0GCSqGSIb3DQEBCjAwoA0wCwYJ\n" \
	"YIZIAWUDBAIDoRowGAYJKoZIhvcNAQEIMAsGCWCGSAFlAwQCA6IDAgFAMA8xDTAL\n" \
	"BgNVBAMTBENBLTAwIBcNMTcwNzI2MTU0NzU2WhgPOTk5OTEyMzEyMzU5NTlaMA8x\n" \
	"DTALBgNVBAMTBENBLTEwggGgMAsGCSqGSIb3DQEBCgOCAY8AMIIBigKCAYEA8Afg\n" \
	"aY9tKN/1UwFdqmDTbxcxiGDQFTDKDFt4zLEy8HoqsiTLEycydVJeAEuw1WNrph1x\n" \
	"nphDETOsiG429CEkIj4rpNaPSevQmfkUP+NFqKgf3egUInmXzSMnKuc3eiDXzSC9\n" \
	"mcYzcs3O6kDruoTBcmujSQxdcPYdj08BkM2uD1PlHVeE1h66axt82I74q8ntT1Zx\n" \
	"IM4TaLSao/Xdn1i5AYHwJj3DzjKlYDuLqkAiyQDI/NrRS007MYRLN4Ebu6bvkuzN\n" \
	"6m7eXYPugV+lSkGSLTi0cbG0wkUqcR1X5JzBqHyXU0epoz3/PpVBwMUNHMun3s7z\n" \
	"TQt5OJY97BeY6l/Wj259iBYj41UvEghT67smaM8zvwFb51+fCPLKPUXG4A2Ksx0k\n" \
	"H+HIP2TIIQbuM4KAS3VmyFNoxzOXs89BdxJCQ+D83RZHSYn4t+76fiSzV+I4baGi\n" \
	"DbPVU7cM5CrOcfTohP83jpOgM/LbPyptGu6S6GKMx93HVLP6LtnZE736dO5XAgMB\n" \
	"AAGjZDBiMA8GA1UdEwEB/wQFMAMBAf8wDwYDVR0PAQH/BAUDAwcEADAdBgNVHQ4E\n" \
	"FgQUNYOAzOqpk/LibJBsXlFFEiD3t4kwHwYDVR0jBBgwFoAULmo+wdwsHxfVzvUw\n" \
	"NyVK9++NokUwPQYJKoZIhvcNAQEKMDCgDTALBglghkgBZQMEAgOhGjAYBgkqhkiG\n" \
	"9w0BAQgwCwYJYIZIAWUDBAIDogMCAUADggGBAIXyJ4S/dWmkPV3kBUENfIXaLV57\n" \
	"mGJjnR/EnUX4gVVxDfKDTNGq2Y1ksCeY1JmvjSHZVkX/D4p3BCHF8bHpLvS7Edts\n" \
	"4NpoL3A4MBdupwDFtF/0Fo4VdZM4ztLL4gBCq2pnukCkbyELCPpe3d/yVujsJNrQ\n" \
	"4faiJMwCjep+3q0ZiytlsN8M3bdGy8ocbzPAi2rMTvQ8I+2e5kLTJmatJ4Qbut25\n" \
	"d1rfJ4ruMt2QOrSlYSENKkA3zjRAg4a2xvVPyOVZBEj48366b1uuji/sOQRckZ/w\n" \
	"3eoeffRfWQXO2y0/K9TUqZM+6n10N32ZkR45I+XSQ13qS73l4QS4djay9z/bAMeb\n" \
	"/zgaf6J790LULzDBEvhPZLNn4bBu/t7WVj2NI+frQvAHyQ9ZhBYkow84qF+//zK9\n" \
	"d/VzQbBQOJFX9TWdWgUxklrWnXE0gmxzGBdq+cMQyHulVVbgShftCRJ8jn8e0Cl1\n" \
	"dl+Cpj08yyLpT9/ZmL8ytgD3Iobw0wPHppb/jQ==\n"                         \
	"-----END CERTIFICATE-----\n"

static const char *missing_middle_single[] = {
	SERVER_CERT, CA_CERT_5, CA_CERT_4, CA_CERT_2, CA_CERT_1, NULL,
};

static const char *missing_middle_single_insert[] = {
	CA_CERT_3,
	NULL,
};

static const char *missing_middle_multiple[] = {
	SERVER_CERT, CA_CERT_5, CA_CERT_4, CA_CERT_1, NULL,
};

static const char *missing_middle_multiple_insert[] = {
	CA_CERT_3 CA_CERT_2,
	NULL,
};

static const char *missing_last_single[] = {
	SERVER_CERT, CA_CERT_5, CA_CERT_4, CA_CERT_3, CA_CERT_2, NULL,
};

static const char *missing_last_single_insert[] = {
	CA_CERT_1,
	NULL,
};

static const char *missing_last_multiple[] = {
	SERVER_CERT, CA_CERT_5, CA_CERT_4, CA_CERT_3, NULL,
};

static const char *missing_last_multiple_insert[] = {
	CA_CERT_2 CA_CERT_1,
	NULL,
};

static const char *missing_skip_single[] = {
	SERVER_CERT, CA_CERT_5, CA_CERT_3, CA_CERT_1, NULL,
};

static const char *missing_skip_single_insert[] = {
	CA_CERT_4,
	CA_CERT_2,
	NULL,
};

static const char *missing_skip_multiple[] = {
	SERVER_CERT,
	CA_CERT_5,
	CA_CERT_3,
	NULL,
};

static const char *missing_skip_multiple_insert[] = {
	CA_CERT_4,
	CA_CERT_2 CA_CERT_1,
	NULL,
};

static const char *missing_middle_single_unsorted[] = {
	SERVER_CERT, CA_CERT_1, CA_CERT_2, CA_CERT_4, CA_CERT_5, NULL,
};

static const char *missing_middle_multiple_unsorted[] = {
	SERVER_CERT, CA_CERT_1, CA_CERT_4, CA_CERT_5, NULL,
};

static const char *missing_last_single_unsorted[] = {
	SERVER_CERT, CA_CERT_2, CA_CERT_3, CA_CERT_4, CA_CERT_5, NULL,
};

static const char *missing_last_multiple_unsorted[] = {
	SERVER_CERT, CA_CERT_3, CA_CERT_4, CA_CERT_5, NULL,
};

static const char *missing_skip_single_unsorted[] = {
	SERVER_CERT, CA_CERT_1, CA_CERT_3, CA_CERT_5, NULL,
};

static const char *missing_skip_multiple_unsorted[] = {
	SERVER_CERT,
	CA_CERT_3,
	CA_CERT_5,
	NULL,
};

static const char *missing_middle_unrelated_insert[] = {
	UNRELATED,
	NULL,
};

static const char *missing_middle_unrelated_extra_insert[] = {
	/* valid CA certificate followed by an unrelated CA: should be accepted */
	CA_CERT_3 UNRELATED,
	NULL,
};

static const char *missing_middle_single_duplicate[] = {
	SERVER_CERT, SERVER_CERT, CA_CERT_5, CA_CERT_5, CA_CERT_4, CA_CERT_4,
	CA_CERT_2,   CA_CERT_2,	  CA_CERT_1, CA_CERT_1, NULL,
};

static const char *missing_middle_multiple_duplicate[] = {
	SERVER_CERT, SERVER_CERT, CA_CERT_5, CA_CERT_5, CA_CERT_4,
	CA_CERT_4,   CA_CERT_1,	  CA_CERT_1, NULL,
};

static const char *missing_last_single_duplicate[] = {
	SERVER_CERT, SERVER_CERT, CA_CERT_5, CA_CERT_5, CA_CERT_4, CA_CERT_4,
	CA_CERT_3,   CA_CERT_3,	  CA_CERT_2, CA_CERT_2, NULL,
};

static const char *missing_last_multiple_duplicate[] = {
	SERVER_CERT, SERVER_CERT, CA_CERT_5, CA_CERT_5, CA_CERT_4,
	CA_CERT_4,   CA_CERT_3,	  CA_CERT_3, NULL,
};

static const char *missing_skip_single_duplicate[] = {
	SERVER_CERT, SERVER_CERT, CA_CERT_5, CA_CERT_5, CA_CERT_3,
	CA_CERT_3,   CA_CERT_1,	  CA_CERT_1, NULL,
};

static const char *missing_skip_multiple_duplicate[] = {
	SERVER_CERT, SERVER_CERT, CA_CERT_5, CA_CERT_5,
	CA_CERT_3,   CA_CERT_3,	  NULL,
};

static const char *missing_ca[] = {
	CA_CERT_0,
	NULL,
};

static const char *middle_single_duplicate_ca[] = {
	SERVER_CERT, CA_CERT_5, CA_CERT_0, CA_CERT_4, CA_CERT_0,
	CA_CERT_2,   CA_CERT_0, CA_CERT_1, NULL,
};

static const char *missing_middle_single_duplicate_ca_unrelated_insert[] = {
	CA_CERT_0,
	NULL,
};

static struct chains {
	const char *name;
	const char **chain;
	const char **insert;
	const char **ca;
	unsigned int verify_flags;
	unsigned int expected_verify_result;
} chains[] = {
	{ "middle single - no sort", missing_middle_single,
	  missing_middle_single_insert, missing_ca,
	  GNUTLS_VERIFY_DO_NOT_ALLOW_UNSORTED_CHAIN, 0 },
	{ "middle multiple - no sort", missing_middle_multiple,
	  missing_middle_multiple_insert, missing_ca,
	  GNUTLS_VERIFY_DO_NOT_ALLOW_UNSORTED_CHAIN, 0 },
	{ "last single - no sort", missing_last_single,
	  missing_last_single_insert, missing_ca,
	  GNUTLS_VERIFY_DO_NOT_ALLOW_UNSORTED_CHAIN, 0 },
	{ "last multiple - no sort", missing_last_multiple,
	  missing_last_multiple_insert, missing_ca,
	  GNUTLS_VERIFY_DO_NOT_ALLOW_UNSORTED_CHAIN, 0 },
	{ "skip single - no sort", missing_skip_single,
	  missing_skip_single_insert, missing_ca,
	  GNUTLS_VERIFY_DO_NOT_ALLOW_UNSORTED_CHAIN, 0 },
	{ "skip multiple - no sort", missing_skip_multiple,
	  missing_skip_multiple_insert, missing_ca,
	  GNUTLS_VERIFY_DO_NOT_ALLOW_UNSORTED_CHAIN, 0 },
	{ "middle single unsorted - no sort", missing_middle_single_unsorted,
	  missing_middle_single_insert, missing_ca,
	  GNUTLS_VERIFY_DO_NOT_ALLOW_UNSORTED_CHAIN,
	  GNUTLS_CERT_INVALID | GNUTLS_CERT_SIGNER_NOT_FOUND },
	{ "middle multiple unsorted - no sort",
	  missing_middle_multiple_unsorted, missing_middle_multiple_insert,
	  missing_ca, GNUTLS_VERIFY_DO_NOT_ALLOW_UNSORTED_CHAIN,
	  GNUTLS_CERT_INVALID | GNUTLS_CERT_SIGNER_NOT_FOUND },
	{ "last single unsorted - no sort", missing_last_single_unsorted,
	  missing_last_single_insert, missing_ca,
	  GNUTLS_VERIFY_DO_NOT_ALLOW_UNSORTED_CHAIN,
	  GNUTLS_CERT_INVALID | GNUTLS_CERT_SIGNER_NOT_FOUND },
	{ "last multiple unsorted - no sort", missing_last_multiple_unsorted,
	  missing_last_multiple_insert, missing_ca,
	  GNUTLS_VERIFY_DO_NOT_ALLOW_UNSORTED_CHAIN,
	  GNUTLS_CERT_INVALID | GNUTLS_CERT_SIGNER_NOT_FOUND },
	{ "skip single unsorted - no sort", missing_skip_single_unsorted,
	  missing_skip_single_insert, missing_ca,
	  GNUTLS_VERIFY_DO_NOT_ALLOW_UNSORTED_CHAIN,
	  GNUTLS_CERT_INVALID | GNUTLS_CERT_SIGNER_NOT_FOUND },
	{ "skip multiple unsorted - no sort", missing_skip_multiple_unsorted,
	  missing_skip_multiple_insert, missing_ca,
	  GNUTLS_VERIFY_DO_NOT_ALLOW_UNSORTED_CHAIN,
	  GNUTLS_CERT_INVALID | GNUTLS_CERT_SIGNER_NOT_FOUND },
	{ "middle single", missing_middle_single, missing_middle_single_insert,
	  missing_ca, 0, 0 },
	{ "middle multiple", missing_middle_multiple,
	  missing_middle_multiple_insert, missing_ca, 0, 0 },
	{ "last single", missing_last_single, missing_last_single_insert,
	  missing_ca, 0, 0 },
	{ "last multiple", missing_last_multiple, missing_last_multiple_insert,
	  missing_ca, 0, 0 },
	{ "skip single", missing_skip_single, missing_skip_single_insert,
	  missing_ca, 0, 0 },
	{ "skip multiple", missing_skip_multiple, missing_skip_multiple_insert,
	  missing_ca, 0, 0 },
	{ "middle single unsorted", missing_middle_single_unsorted,
	  missing_middle_single_insert, missing_ca, 0, 0 },
	{ "middle multiple unsorted", missing_middle_multiple_unsorted,
	  missing_middle_multiple_insert, missing_ca, 0, 0 },
	{ "last single unsorted", missing_last_single_unsorted,
	  missing_last_single_insert, missing_ca, 0, 0 },
	{ "last multiple unsorted", missing_last_multiple_unsorted,
	  missing_last_multiple_insert, missing_ca, 0, 0 },
	{ "skip single unsorted", missing_skip_single_unsorted,
	  missing_skip_single_insert, missing_ca, 0, 0 },
	{ "skip multiple unsorted", missing_skip_multiple_unsorted,
	  missing_skip_multiple_insert, missing_ca, 0, 0 },
	{ "unrelated", missing_middle_single, missing_middle_unrelated_insert,
	  missing_ca, 0, GNUTLS_CERT_INVALID | GNUTLS_CERT_SIGNER_NOT_FOUND },
	{ "unrelated extra", missing_middle_single,
	  missing_middle_unrelated_extra_insert, missing_ca, 0, 0 },
	{ "middle single duplicate", missing_middle_single_duplicate,
	  missing_middle_single_insert, missing_ca, 0, 0 },
	{ "middle multiple duplicate", missing_middle_multiple_duplicate,
	  missing_middle_multiple_insert, missing_ca, 0, 0 },
	{ "last single duplicate", missing_last_single_duplicate,
	  missing_last_single_insert, missing_ca, 0, 0 },
	{ "last multiple duplicate", missing_last_multiple_duplicate,
	  missing_last_multiple_insert, missing_ca, 0, 0 },
	{ "skip single duplicate", missing_skip_single_duplicate,
	  missing_skip_single_insert, missing_ca, 0, 0 },
	{ "skip multiple duplicate", missing_skip_multiple_duplicate,
	  missing_skip_multiple_insert, missing_ca, 0, 0 },
	{ "middle single duplicate ca", middle_single_duplicate_ca,
	  missing_middle_single_insert, missing_ca, 0, 0 },
	{ "middle single duplicate ca - insert unrelated",
	  middle_single_duplicate_ca,
	  missing_middle_single_duplicate_ca_unrelated_insert, missing_ca, 0,
	  GNUTLS_CERT_INVALID | GNUTLS_CERT_SIGNER_NOT_FOUND },
	{ NULL, NULL, NULL, NULL },
};

#endif /* GNUTLS_TESTS_TEST_CHAINS_ISSUER_H */
