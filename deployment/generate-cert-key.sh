#!/bin/bash

# Use the first passed argument as target directory; fallback to script directory if empty
TARGET_DIR="${1:-$(dirname "$0")}"

# Absolute expansion to ensure subshells and CMake paths evaluate safely
TARGET_DIR="$(cd "$TARGET_DIR" && pwd)"

# Ensure the output subdirectory structures exist structurally
mkdir -p "$TARGET_DIR"

IP="valvos.dev"
SUBJECT_CA="/C=SE/ST=Bristol/L=Bristol/O=valvos/OU=CA/CN=$IP"
SUBJECT_SERVER="/C=SE/ST=Bristol/L=Bristol/O=valvos/OU=Server/CN=$IP"
SUBJECT_CLIENT="/C=SE/ST=Bristol/L=Bristol/O=valvos/OU=Client/CN=$IP"

function generate_CA () {
   echo "Generating CA: $SUBJECT_CA"
   openssl req -x509 -nodes -sha256 -newkey rsa:2048 -subj "$SUBJECT_CA" -days 365 -keyout "$TARGET_DIR/valvos-dev-ca.key" -out "$TARGET_DIR/valvos-dev-ca.crt"
}

function generate_server () {
   echo "Generating Server Cert: $SUBJECT_SERVER"
   # Fixed the leading dot typo here (.$CERT_DIR/server.csr -> $TARGET_DIR/server.csr)
   openssl req -nodes -sha256 -new -subj "$SUBJECT_SERVER" -keyout "$TARGET_DIR/server.key" -out "$TARGET_DIR/server.csr"
   openssl x509 -req -sha256 -in "$TARGET_DIR/server.csr" -CA "$TARGET_DIR/valvos-dev-ca.crt" -CAkey "$TARGET_DIR/valvos-dev-ca.key" -CAcreateserial -out "$TARGET_DIR/server.crt" -days 365
   rm -f "$TARGET_DIR/server.csr"
}

function generate_client () {
   echo "Generating Client Cert: $SUBJECT_CLIENT"
   openssl req -new -nodes -sha256 -subj "$SUBJECT_CLIENT" -out "$TARGET_DIR/client.csr" -keyout "$TARGET_DIR/client.key" 
   openssl x509 -req -sha256 -in "$TARGET_DIR/client.csr" -CA "$TARGET_DIR/valvos-dev-ca.crt" -CAkey "$TARGET_DIR/valvos-dev-ca.key" -CAcreateserial -out "$TARGET_DIR/client.crt" -days 365
   rm -f "$TARGET_DIR/client.csr"
}

if [ ! -f "$TARGET_DIR/valvos-dev-ca.crt" ]; then
   generate_CA
   generate_server
   generate_client
fi

if [ ! -f "$TARGET_DIR/server.crt" ]; then
   generate_server
fi

if [ ! -f "$TARGET_DIR/client.crt" ]; then
   generate_client  # Fixed the bug here: your code called generate_server for missing client
fi