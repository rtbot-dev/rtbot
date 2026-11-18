#!/usr/bin/env bash
set -euo pipefail
fixture="$TEST_TMPDIR/stamping"
mkdir -p "$fixture/tools"
cp "$TEST_SRCDIR/_main/tools/workspace_status.sh" "$fixture/tools/"
printf '{"version": "0.6.1"}\n' > "$fixture/package.json"
cd "$fixture"
git init -q
git -c user.name=test -c user.email=test@example.invalid add .
git -c user.name=test -c user.email=test@example.invalid commit -qm init
git tag v0.6.2
bash tools/workspace_status.sh > status.txt
grep -x 'STABLE_RTBOT_NPM_VERSION 0.6.2' status.txt
git tag -d v0.6.2 >/dev/null
git tag v0.6.2-rc.1
bash tools/workspace_status.sh > status.txt
grep -x 'STABLE_RTBOT_NPM_VERSION 0.6.2-rc.1' status.txt
git tag -d v0.6.2-rc.1 >/dev/null
git tag v0.6.1
git -c user.name=test -c user.email=test@example.invalid commit --allow-empty -qm next
bash tools/workspace_status.sh > status.txt
grep -x 'STABLE_RTBOT_NPM_VERSION 0.6.1-dev.1' status.txt
