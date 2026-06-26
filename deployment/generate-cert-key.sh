#!/bin/bash

IP="valvos.dev"
SUBJECT_CA="/C=SE/ST=Bristol/L=Bristol/O=valvos/OU=CA/CN=$IP"
SUBJECT_SERVER="/C=SE/ST=Bristol/L=Bristol/O=valvos/OU=Server/CN=$IP"
SUBJECT_CLIENT="/C=SE/ST=Bristol/L=Bristol/O=valvos/OU=Client/CN=$IP"

function generate_CA () {
   echo "$SUBJECT_CA"
   openssl req -x509 -nodes -sha256 -newkey rsa:2048 -subj "$SUBJECT_CA"  -days 365 -keyout ./certs/valvos-dev-ca.key -out ./certs/valvos-dev-ca.crt
}

function generate_server () {
   echo "$SUBJECT_SERVER"
   openssl req -nodes -sha256 -new -subj "$SUBJECT_SERVER" -keyout ./certs/server.key -out ./certs/server.csr
   openssl x509 -req -sha256 -in ./certs/server.csr -CA ./certs/valvos-dev-ca.crt -CAkey ./certs/valvos-dev-ca.key -CAcreateserial -out ./certs/server.crt -days 365
}

function generate_client () {
   echo "$SUBJECT_CLIENT"
   openssl req -new -nodes -sha256 -subj "$SUBJECT_CLIENT" -out ./certs/client.csr -keyout ./certs/client.key 
   openssl x509 -req -sha256 -in ./certs/client.csr -CA ./certs/valvos-dev-ca.crt -CAkey ./certs/valvos-dev-ca.key -CAcreateserial -out ./certs/client.crt -days 365
}

generate_CA
generate_server
generate_client