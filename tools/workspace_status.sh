#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

read_default_version() {
  local value
  value="$(sed -nE 's/^[[:space:]]*"version"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/p' package.json | head -n 1)"
  if [[ -z "${value}" ]]; then
    value="0.1.0"
  fi
  printf '%s' "${value}"
}

normalize_tag_to_pep440() {
  local tag="$1"
  local normalized

  normalized="${tag}"
  normalized="${normalized#v}"
  normalized="$(printf '%s' "${normalized}" | sed -E 's/^[^0-9]*([0-9].*)$/\1/; s/-/./g')"

  if [[ "${normalized}" =~ ^[0-9]+(\.[0-9A-Za-z]+)*$ ]]; then
    printf '%s' "${normalized}"
    return
  fi

  printf ''
}

DEFAULT_VERSION="$(read_default_version)"
VERSION="${DEFAULT_VERSION}"
GIT_COMMIT="nogit"

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  GIT_COMMIT="$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"

  EXACT_TAG="$(git describe --tags --exact-match HEAD 2>/dev/null || true)"
  if [[ -n "${EXACT_TAG}" ]]; then
    TAG_VERSION="$(normalize_tag_to_pep440 "${EXACT_TAG}")"
    if [[ -n "${TAG_VERSION}" ]]; then
      VERSION="${TAG_VERSION}"
    fi
  else
    NEAREST_TAG="$(git describe --tags --match 'v[0-9]*' --match '[0-9]*' --abbrev=0 2>/dev/null || true)"
    if [[ -n "${NEAREST_TAG}" ]]; then
      BASE_VERSION="$(normalize_tag_to_pep440 "${NEAREST_TAG}")"
      DISTANCE="$(git rev-list --count "${NEAREST_TAG}"..HEAD 2>/dev/null || echo 0)"
      if [[ -n "${BASE_VERSION}" ]]; then
        VERSION="${BASE_VERSION}.dev${DISTANCE}"
      fi
    else
      COMMIT_COUNT="$(git rev-list --count HEAD 2>/dev/null || echo 0)"
      VERSION="${DEFAULT_VERSION}.dev${COMMIT_COUNT}"
    fi
  fi
fi

printf 'RTBOT_VERSION %s\n' "${VERSION}"
printf 'STABLE_GIT_COMMIT %s\n' "${GIT_COMMIT}"
printf 'BUILD_SCM_REVISION %s\n' "${GIT_COMMIT}"
printf 'BUILD_TIMESTAMP %s\n' "$(date +%s)"
