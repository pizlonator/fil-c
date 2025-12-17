#!/bin/bash

set -e

msg() {
  echo
  echo "* $1 ..."
}

cd `dirname $0`

echo
echo "This script regenerates all private keys and certificates"
echo "needed to run glib-networking tests. Please note this script"
echo "depends on datefudge, openssl, and python3's cryptography module."
echo

read -p "Press [Enter] key to continue..."

#######################################################################
### Obsolete/Untrusted Root CA
#######################################################################

echo "00" > serial

msg "Creating CA private key for obsolete/untrusted CA"
openssl genrsa -out old-ca-key.pem 2048

msg "Creating CA certificate for obsolete/untrusted CA"
openssl req -x509 -new -config ssl/old-ca.conf -days 10950 -key old-ca-key.pem -out old-ca.pem

#######################################################################
### New Root CA
#######################################################################

msg "Creating CA private key"
openssl genrsa -out ca-key.pem 2048

msg "Creating CA certificate"
openssl req -x509 -new -config ssl/ca.conf -days 10950 -key ca-key.pem -out ca.pem

#######################################################################
### New Root CA with OCSP MustStaple
#######################################################################

msg "Creating CA (OCSP) certificate"
openssl req -x509 -new -config ssl/ca.conf -addext tlsfeature=status_request -days 10950 -key ca-key.pem -out ca-ocsp.pem

#######################################################################
### New Root CA, issued by Obsolete/Untrusted Root CA
#######################################################################

msg "Creating CA certificate request"
openssl req -config ssl/ca.conf -key ca-key.pem -new -out root-ca-csr.pem

msg "Creating alternative certificate with same keys as CA"
openssl x509 -req -in root-ca-csr.pem -days 10950 -CA old-ca.pem -CAkey old-ca-key.pem -CAserial serial -extfile ssl/ca.conf -extensions v3_req_ext -out ca-alternative.pem

#######################################################################
### Server
#######################################################################

msg "Creating server private key"
openssl genrsa -out server-key.pem 2048

msg "Creating server certificate request"
openssl req -config ssl/server.conf -key server-key.pem -new -out server-csr.pem

msg "Creating server certificate"
openssl x509 -req -in server-csr.pem -days 9125 -CA ca.pem -CAkey ca-key.pem -CAserial serial -extfile ssl/server.conf -extensions v3_req_ext -out server.pem

msg "Concatenating server certificate and private key into a single file"
cat server.pem > server-and-key.pem
cat server-key.pem >> server-and-key.pem

msg "Updating digest of the new certificate in connections.c"
DIGEST=$( openssl x509 -outform der -in server.pem | openssl sha256 -binary | base64 | sed 's/\//\\\//g' )
sed -i "/define SERVER_CERT_DIGEST_B64/s/\"\([^\"]\+\)\"/\"$DIGEST\"/" ../connection.c

msg "Converting server certificate from PEM to DER"
openssl x509 -in server.pem -outform DER -out server.der

msg "Converting server private key from PEM to DER"
openssl rsa -in server-key.pem -outform DER -out server-key.der

msg "Converting server private key to PKCS #8"
openssl pkcs8 -topk8 -in server-key.pem -outform PEM -nocrypt -out server-key-pkcs8.pem
openssl pkcs8 -topk8 -in server-key.pem -outform DER -nocrypt -out server-key-pkcs8.der

#######################################################################
### Server (OCSP required by CA)
#######################################################################

msg "Creating server (OCSP required by CA) certificate"
openssl x509 -req -in server-csr.pem -days 9125 -CA ca-ocsp.pem -CAkey ca-key.pem -CAserial serial -extfile ssl/server.conf -extensions v3_req_ext -out server-ocsp-required-by-ca.pem

msg "Concatenating server (OCSP required by CA) certificate and private key into a single file"
cat server-ocsp-required-by-ca.pem > server-ocsp-required-by-ca-and-key.pem
cat server-key.pem >> server-ocsp-required-by-ca-and-key.pem

#######################################################################
### Server (OCSP required by server)
#######################################################################

msg "Creating server (OCSP required by server) certificate"
openssl x509 -req -in server-csr.pem -days 9125 -CA ca.pem -CAkey ca-key.pem -CAserial serial -extfile ssl/server-muststaple.conf -extensions v3_req_ext -out server-ocsp-required-by-server.pem

msg "Concatenating server (OCSP required by server) certificate and private key into a single file"
cat server-ocsp-required-by-server.pem > server-ocsp-required-by-server-and-key.pem
cat server-key.pem >> server-ocsp-required-by-server-and-key.pem

#######################################################################
### Server (self-signed)
#######################################################################

msg "Creating server self-signed certificate"
openssl x509 -req -days 9125 -in server-csr.pem -signkey server-key.pem -out server-self.pem

#######################################################################
### Client
#######################################################################

msg "Creating client private key"
openssl genrsa -out client-key.pem 2048

msg "Creating client certificate request"
openssl req -config ssl/client.conf -key client-key.pem -new -out client-csr.pem

msg "Creating client certificate"
openssl x509 -req -in client-csr.pem -days 9125 -CA ca.pem -CAkey ca-key.pem -CAserial serial -out client.pem

msg "Concatenating client certificate and private key into a single file"
cat client.pem > client-and-key.pem
cat client-key.pem >> client-and-key.pem

msg "Concatenating the full client chain into a single file"
cat ca.pem > client-and-key-fullchain.pem
cat client-and-key.pem >> client-and-key-fullchain.pem

# It is not possible to specify the start and end date using the "x509" tool.
# It would be better to use the "ca" tool. Sorry!
msg "Creating client certificate (past)"
datefudge "17 JUL 2000 18:00:00" openssl x509 -req -in client-csr.pem -days 365 -startdate -enddate -CA ca.pem -CAkey ca-key.pem -CAserial serial -out client-past.pem
touch client-past.pem

msg "Creating client certificate (future)"
datefudge "17 JUL 2060 18:00:00" openssl x509 -req -in client-csr.pem -days 365 -startdate -enddate -CA ca.pem -CAkey ca-key.pem -CAserial serial -out client-future.pem
touch client-future.pem

msg "Creating second client key pair"
openssl genrsa -out client2-key.pem 2048
openssl req -config ssl/client.conf -key client2-key.pem -new -out client2-csr.pem
openssl x509 -req -in client2-csr.pem -days 9125 -CA ca.pem -CAkey ca-key.pem -CAserial serial -out client2.pem

msg "Concatenating second client certificate and private key into a single file"
cat client2.pem client2-key.pem > client2-and-key.pem

#######################################################################
### Concatenate all non-CA certificates
#######################################################################

msg "Concatenating all non-CA certificates into a single file"
echo "client.pem:" > non-ca.pem
cat client.pem >> non-ca.pem
echo >> non-ca.pem
echo "client-future.pem:" >> non-ca.pem
cat client-future.pem >> non-ca.pem
echo >> non-ca.pem
echo "client-past.pem:" >> non-ca.pem
cat client-past.pem >> non-ca.pem
echo >> non-ca.pem
echo "server.pem:" >> non-ca.pem
cat server.pem >> non-ca.pem
echo >> non-ca.pem
echo "server-self.pem:" >> non-ca.pem
cat server-self.pem >> non-ca.pem

#######################################################################
### Intermediate CA
#######################################################################

echo "00" > intermediate-serial

msg "Creating intermediate CA private key"
openssl genrsa -out intermediate-ca-key.pem 2048

msg "Creating intermediate CA certificate request"
openssl req -config ssl/intermediate-ca.conf -key intermediate-ca-key.pem -new -out intermediate-ca-csr.pem

msg "Creating intermediate CA certificate"
openssl x509 -req -in intermediate-ca-csr.pem -days 9125 -CA ca.pem -CAkey ca-key.pem -CAserial serial -extfile ssl/intermediate-ca.conf -extensions v3_req_ext -out intermediate-ca.pem

#######################################################################
### Server (signed by Intermediate CA)
#######################################################################

msg "Creating server (intermediate CA) private key"
openssl genrsa -out server-intermediate-key.pem 2048

msg "Creating server (intermediate CA) certificate request"
openssl req -config ssl/server-intermediate.conf -key server-intermediate-key.pem -new -out server-intermediate-csr.pem

msg "Creating server (intermediate CA) certificate"
openssl x509 -req -in server-intermediate-csr.pem -days 9125 -CA intermediate-ca.pem -CAkey intermediate-ca-key.pem -CAserial intermediate-serial -extfile ssl/server-intermediate.conf -extensions v3_req_ext -out server-intermediate.pem

msg "Concatenating server (intermediate CA) chain into a file"
cat server-intermediate.pem > chain.pem
cat intermediate-ca.pem >> chain.pem
cat ca.pem >> chain.pem

#######################################################################
### Updating CA Root files
#######################################################################

msg "Updating CA Root files"
./update-chain-with-new-root.py ca-roots.pem ca.pem
./update-chain-with-new-root.py ca-roots-bad.pem ca.pem

#######################################################################
### Update test expectations
#######################################################################

msg "Updating test expectations"
./update-test-database.py ca.pem ../file-database.h
./update-certificate-test.py server.pem ../certificate.h

#######################################################################
### Generate PKCS #12 format copies for testing
#######################################################################

msg "Generating PKCS #12 files"
# Not encrypted p12 file
openssl pkcs12 -in client-and-key.pem -export -keypbe NONE -certpbe NONE -nomaciter -out client-and-key.p12 -passout 'pass:' -name "No password"
# Encrypted key only
openssl pkcs12 -in client-and-key.pem -export -certpbe NONE -nomaciter -out client-and-key-password.p12 -passout 'pass:1234' -name "With Password"
# Encrypted p12 file
openssl pkcs12 -in client-and-key.pem -export -out client-and-key-password-enckey.p12 -passout 'pass:1234' -name "With Password and encrypted privkey"

#######################################################################
### Cleanup
#######################################################################

# We don't need the serial files anymore
rm -f serial
rm -f intermediate-serial
