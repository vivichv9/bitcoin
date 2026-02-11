#!/usr/bin/env bash
set -euo pipefail

# Collect Bitcoin depends source archives into a single local directory
# so builds can run without downloading from external mirrors.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DEPENDS_DIR="${REPO_ROOT}/depends"

OUT_DIR="${1:-${DEPENDS_DIR}/offline-sources}"
TARGET="${2:-host}"

mkdir -p "${OUT_DIR}"
OUT_DIR="$(cd "${OUT_DIR}" && pwd)"

MAKE_BIN="make"
if command -v gmake >/dev/null 2>&1; then
  MAKE_BIN="gmake"
fi

if [[ "${TARGET}" == "host" ]]; then
  HOST_TRIPLET="$(${DEPENDS_DIR}/config.guess)"
  DOWNLOAD_TARGET="download-one"
  EXTRA_ARGS=("HOST=${HOST_TRIPLET}")
elif [[ "${TARGET}" == "linux" ]]; then
  DOWNLOAD_TARGET="download-linux"
  EXTRA_ARGS=()
elif [[ "${TARGET}" == "win" ]]; then
  DOWNLOAD_TARGET="download-win"
  EXTRA_ARGS=()
elif [[ "${TARGET}" == "osx" ]]; then
  DOWNLOAD_TARGET="download-osx"
  EXTRA_ARGS=()
elif [[ "${TARGET}" == "all" ]]; then
  DOWNLOAD_TARGET="download"
  EXTRA_ARGS=()
else
  echo "Unknown target: ${TARGET}" >&2
  echo "Usage: $0 [output_dir] [host|linux|win|osx|all]" >&2
  exit 1
fi

cat <<MSG
Preparing depends source cache
- Output directory: ${OUT_DIR}
- Target: ${TARGET}
- Make binary: ${MAKE_BIN}
MSG

MAKE_ARGS=(
  -C "${DEPENDS_DIR}"
  "${DOWNLOAD_TARGET}"
  "SOURCES_PATH=${OUT_DIR}"
  "${EXTRA_ARGS[@]}"
)

if [[ -n "${NO_BOOST:-}" ]]; then MAKE_ARGS+=("NO_BOOST=${NO_BOOST}"); fi
if [[ -n "${NO_LIBEVENT:-}" ]]; then MAKE_ARGS+=("NO_LIBEVENT=${NO_LIBEVENT}"); fi
if [[ -n "${NO_QT:-}" ]]; then MAKE_ARGS+=("NO_QT=${NO_QT}"); fi
if [[ -n "${NO_QR:-}" ]]; then MAKE_ARGS+=("NO_QR=${NO_QR}"); fi
if [[ -n "${NO_WALLET:-}" ]]; then MAKE_ARGS+=("NO_WALLET=${NO_WALLET}"); fi
if [[ -n "${NO_ZMQ:-}" ]]; then MAKE_ARGS+=("NO_ZMQ=${NO_ZMQ}"); fi
if [[ -n "${NO_USDT:-}" ]]; then MAKE_ARGS+=("NO_USDT=${NO_USDT}"); fi
if [[ -n "${NO_IPC:-}" ]]; then MAKE_ARGS+=("NO_IPC=${NO_IPC}"); fi

"${MAKE_BIN}" "${MAKE_ARGS[@]}"

echo "Done. Sources are in: ${OUT_DIR}"
echo "Use the same SOURCES_PATH for offline builds, for example:"
echo "  ${MAKE_BIN} -C depends HOST=\$(${DEPENDS_DIR}/config.guess) SOURCES_PATH=${OUT_DIR}"
