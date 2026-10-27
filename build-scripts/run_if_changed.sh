#!/bin/sh
# Usage: run_if_changed.sh <stamp> <watch-path>... -- <command> [args...]
set -e
stamp=$1; shift
watch=
while [ "$1" != "--" ]; do watch="$watch $1"; shift; done
shift

if [ -f "$stamp" ]; then
    changed=$(find -H $watch -name .git -prune -o -type f -newer "$stamp" -print -quit 2>/dev/null || true)
    [ -z "$changed" ] && exit 0
fi

touch "$stamp.start"
if "$@"; then
    mv "$stamp.start" "$stamp"
else
    rem -f "$stamp.start"
    exit 1
fi
