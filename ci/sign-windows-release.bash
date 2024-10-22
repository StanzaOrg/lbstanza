#!/bin/bash -eu
set -Eeuo pipefail

# sign windows executables with DigiCert smctl with private key in DigiCert KeyLocker

### required environment variables for authentication with DigiCert
# example: SM_API_KEY="00000000000000000000000000_0000000000000000000000000000000000000000000000000000000000000000"
# example: SM_HOST="https://clientauth.one.digicert.com"
# example: SM_CLIENT_CERT_FILE="C:\Users\Administrator\.signingmanager\jwatson-digicert-clientcert-20231212-Certificate_pkcs12.p12"
# example: SM_CLIENT_CERT_PASSWORD="xxxxxxxxxxxx"
# example: SM_KEY_ALIAS="key_000000000"
# example: SMCTL="C:\Program Files\DigiCert\DigiCert Keylocker Tools\smctl.exe"

# Defaulted env var inputs - can override if necessary
echo "              SMCTL:" "${SMCTL:=C:\Program Files\DigiCert\DigiCert Keylocker Tools\smctl.exe}"
echo "            SM_HOST:" "${SM_HOST:=https://clientauth.one.digicert.com}"
export SMCTL SM_HOST

# check for smctl credential env vars
VARERR=0
for V in  SM_API_KEY  SM_HOST  SM_CLIENT_CERT_FILE  SM_CLIENT_CERT_PASSWORD  SM_KEY_ALIAS  SMCTL ; do
  if [ ! -v ${V} ] ; then
    echo "Error: Environment variable ${V} not found"
    VARERR=1
  fi
done
[ ${VARERR} -gt 0 ] && exit -1
for V in  SM_CLIENT_CERT_FILE  SMCTL ; do
  if [ ! -e "${!V}" ] ; then
    echo "Error: ${V} file \"${!V}\" does not exist"
    VARERR=1
  fi
done
[ ${VARERR} -gt 0 ] && exit -1
echo "SM_CLIENT_CERT_FILE:" "${SM_CLIENT_CERT_FILE}"
echo "       SM_KEY_ALIAS:" "${SM_KEY_ALIAS}"

"${SMCTL}" windows certsync --keypair-alias ${SM_KEY_ALIAS}
"${SMCTL}" keypair ls --filter alias=${SM_KEY_ALIAS}

echo "   Signing files in:" "${PWD}"
ls -l
for file in "stanza.exe"
do
  echo "Signing ${file}..."
  "${SMCTL}" sign -i "${PWD}/${file}" --keypair-alias ${SM_KEY_ALIAS} --verbose
  "${SMCTL}" sign verify -i "${PWD}/${file}"
done
