#
# $Id$
#
# Copyright 2013, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.
#
# Generate a passphrase-less self signed certificate
#
# Note, requires openssl binary to be in path
#

mkdir -p /tmp/$$.tmp
cd /tmp/$$.tmp

echo ""
echo "STEP 1) Creating your rsa 2048 bit private key without a password"
echo ""

openssl genrsa -out server.key 2048

echo ""
echo "STEP 2) Creating your Certificate Signing Request (CSR)"
echo ""
echo "The following will ask you a bunch of questions.  The only important answer is"
echo "what you fill in for \"COMMON NAME\".  This needs to the same domain as the web"
echo "server you are building this certificate for."
echo ""

openssl req -new -key server.key -out server.csr

echo ""
echo "STEP 3) Self-signing your certificate"
echo ""

openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt

echo ""
echo "STEP 4) Creating your PEM file"
echo ""

cat server.key server.crt > server.pem

echo ""
echo "DONE - Your files are:"
echo "---------------------------------------------------------------------------"
echo "/tmp/$$.tmp/server.pem: Self signed PEM file (key + cert in PEM format)"
echo "/tmp/$$.tmp/server.key: Private key (no passphrase)"
echo "/tmp/$$.tmp/server.crt: Self signed server certificate"
echo "/tmp/$$.tmp/server.csr: Certificate signing request"
echo ""

exit 0
