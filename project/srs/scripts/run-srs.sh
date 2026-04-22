#!/usr/bin/env bash
set -euo pipefail

if ! command -v srs >/dev/null 2>&1; then
  echo "srs command not found. Please install SRS first."
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONF_FILE="${SCRIPT_DIR}/../conf/srs.conf"

echo "Starting SRS with config: ${CONF_FILE}"
exec srs -c "${CONF_FILE}"
