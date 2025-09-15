#!/bin/bash

# This script will be run bazel when building process starts to
# generate key-value information that represents the status of the
# workspace. The output should be like
#
# KEY1 VALUE1
# KEY2 VALUE2
#
# If the script exits with non-zero code, it's considered as a failure
# and the output will be discarded.

set -eo pipefail # exit immediately if any command fails.

function remove_url_credentials() {
	which perl >/dev/null && perl -pe 's#//.*?:.*?@#//#' || cat
}

repo_url=$(git config --get remote.origin.url | remove_url_credentials)
echo "REPO_URL $repo_url"

git_tree_status=$(git diff-index --quiet HEAD -- && echo 'Clean' || echo 'Modified')
echo "GIT_TREE_STATUS $git_tree_status"

echo "VERSION $(git describe --tags $(git rev-list --tags --max-count=1))"
echo "CURRENT_TIME $(date +%s)"
echo "STABLE_GIT_COMMIT $(git rev-parse HEAD)"
echo "STABLE_SHORT_GIT_COMMIT $(git rev-parse HEAD | cut -c 1-8)"
echo "STABLE_USER_NAME $USER"

# Generate dynamic version for RtBot Python wheel
if git describe --tags --abbrev=0 >/dev/null 2>&1; then
    LATEST_TAG=$(git describe --tags --abbrev=0)
    GIT_DESCRIBE=$(git describe --tags --always)

    # Remove 'v' prefix if present
    LATEST_TAG=${LATEST_TAG#v}

    # If we're exactly on a tag, use that version
    if [ "$GIT_DESCRIBE" = "v$LATEST_TAG" ] || [ "$GIT_DESCRIBE" = "$LATEST_TAG" ]; then
        RTBOT_VERSION="$LATEST_TAG"
    else
        # Parse format: v0.3.8-135-gc6ee14a -> 0.3.8.dev135+gc6ee14a
        TAG_PART=$(echo "$GIT_DESCRIBE" | cut -d'-' -f1)
        TAG_PART=${TAG_PART#v}
        COMMITS_AHEAD=$(echo "$GIT_DESCRIBE" | cut -d'-' -f2)
        SHORT_SHA=$(echo "$GIT_DESCRIBE" | cut -d'-' -f3)

        if [ -n "$TAG_PART" ] && [ -n "$COMMITS_AHEAD" ] && [ -n "$SHORT_SHA" ]; then
            RTBOT_VERSION="${TAG_PART}.dev${COMMITS_AHEAD}+${SHORT_SHA}"
        else
            RTBOT_VERSION="$LATEST_TAG"
        fi
    fi
else
    RTBOT_VERSION="0.1.0"
fi

echo "RTBOT_VERSION $RTBOT_VERSION"
