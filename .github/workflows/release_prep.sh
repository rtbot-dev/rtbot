#!/usr/bin/env bash

set -o errexit -o nounset -o pipefail

# Set by GH actions, see
# https://docs.github.com/en/actions/learn-github-actions/environment-variables#default-environment-variables
TAG=${GITHUB_REF_NAME}
# The prefix is chosen to match what GitHub generates for source archives
PREFIX="rtbot-${TAG:1}"
ARCHIVE="rtbot-$TAG.tar.gz"
git archive --format=tar --prefix="${PREFIX}/" "${TAG}" | gzip > "$ARCHIVE"
SHA=$(shasum -a 256 "$ARCHIVE" | awk '{print $1}')

cat << EOF

## Using WORKSPACE

Paste this snippet into your \`WORKSPACE\` file:

\`\`\`starlark
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rtbot",
    sha256 = "${SHA}",
    strip_prefix = "${PREFIX}",
    url = "https://github.com/rtbot/rtbot/releases/download/${TAG}/${ARCHIVE}",
)

load("@rtbot//:deps.bzl", "rtbot_deps" = "deps")
rtbot_deps()
\`\`\`
EOF