#!/usr/bin/env bash
set -euo pipefail

KATAGO_BIN="${1:-./katago}"
MODEL_FILE="${2:-tests/models/g170-b6c96-s175395328-d26788732.bin.gz}"
CONFIG_FILE="${3:-configs/gtp_example.cfg}"
EXTRA_OVERRIDES="${KATAGO_GTP_TEST_OVERRIDES:-}"

TEST_TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/katago-gtp-custom-params.XXXXXX")"
trap 'rm -rf -- "${TEST_TMP_DIR:?}"' EXIT

{
  printf '%s\n' \
    "kata-get-param genmoveWideRootNoise" \
    "kata-get-param genmoveRootPolicyTemperature" \
    "kata-get-param genmoveRootPolicyTemperatureEarly" \
    "kata-get-param analysisRootPolicyTemperature" \
    "kata-get-param analysisRootPolicyTemperatureEarly" \
    'kata-set-params {"genmoveWideRootNoise":0.8,"genmoveRootPolicyTemperature":1.3,"genmoveRootPolicyTemperatureEarly":1.4,"analysisRootPolicyTemperature":1.5,"analysisRootPolicyTemperatureEarly":1.6}' \
    "kata-get-param genmoveWideRootNoise" \
    "kata-get-param genmoveRootPolicyTemperature" \
    "kata-get-param genmoveRootPolicyTemperatureEarly" \
    "kata-get-param analysisRootPolicyTemperature" \
    "kata-get-param analysisRootPolicyTemperatureEarly" \
    "kata-list-params" \
    "kata-get-params" \
    "kata-set-param analysisRootPolicyTemperature 0" \
    "kata-get-param analysisRootPolicyTemperature" \
    "quit"
} > "${TEST_TMP_DIR}/commands.txt"

"${KATAGO_BIN}" gtp \
  -config "${CONFIG_FILE}" \
  -model "${MODEL_FILE}" \
  -override-config "logDir=,logFile=${TEST_TMP_DIR}/gtp.log,logToStderr=false,logAllGTPCommunication=false,numSearchThreads=1${EXTRA_OVERRIDES}" \
  < "${TEST_TMP_DIR}/commands.txt" \
  > "${TEST_TMP_DIR}/stdout.txt" \
  2> "${TEST_TMP_DIR}/stderr.txt"

grep -E '^\? |^= $|^= (-?[0-9]+([.][0-9]+)?)$' "${TEST_TMP_DIR}/stdout.txt" > "${TEST_TMP_DIR}/responses.txt"

{
  printf '%s\n' \
    "= 0.04" \
    "= 1" \
    "= 1" \
    "= 1" \
    "= 1" \
    "= " \
    "= 0.8" \
    "= 1.3" \
    "= 1.4" \
    "= 1.5" \
    "= 1.6" \
    "? Could not set params: Key 'analysisRootPolicyTemperature' must be in the range 0.01 to 100 in config file  and/or command-line and query overrides" \
    "= 1.5" \
    "= "
} > "${TEST_TMP_DIR}/expected.txt"

diff -u "${TEST_TMP_DIR}/expected.txt" "${TEST_TMP_DIR}/responses.txt"
grep -F "analysisRootPolicyTemperature analysisRootPolicyTemperatureEarly genmoveWideRootNoise genmoveRootPolicyTemperature genmoveRootPolicyTemperatureEarly" "${TEST_TMP_DIR}/stdout.txt" > /dev/null
grep -F '"analysisRootPolicyTemperature":"1.5"' "${TEST_TMP_DIR}/stdout.txt" > /dev/null
grep -F '"analysisRootPolicyTemperatureEarly":"1.6"' "${TEST_TMP_DIR}/stdout.txt" > /dev/null
grep -F '"genmoveRootPolicyTemperature":"1.3"' "${TEST_TMP_DIR}/stdout.txt" > /dev/null
grep -F '"genmoveRootPolicyTemperatureEarly":"1.4"' "${TEST_TMP_DIR}/stdout.txt" > /dev/null
grep -F '"genmoveWideRootNoise":"0.8"' "${TEST_TMP_DIR}/stdout.txt" > /dev/null
printf '%s\n' "GTP 독자 검색 옵션 테스트 통과"
