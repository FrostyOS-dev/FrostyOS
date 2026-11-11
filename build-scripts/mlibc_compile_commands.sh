#!/bin/sh
# Rewrites jinx's container-path compile_commands.json for mlibc into host paths.
# Output goes to .jinx-build/mlibc-cdb, outside every stage's WATCH list.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
js=$root/Userland/jinx-sources
src=$root/.jinx-build/builds/mlibc/compile_commands.json
outdir=$root/.jinx-build/mlibc-cdb
tc=$root/build/toolchain/usr/local/bin

[ -f "$src" ] || { echo "mlibc_compile_commands: $src not found yet, skipping" >&2; exit 0; }

mkdir -p "$outdir"
# Order matters: the relative ../../../base_dir/ form (from /build_dir/builds/mlibc) must go first
sed -e "s|\.\./\.\./\.\./base_dir/|$js/|g" \
    -e "s|\.\./\.\./\.\./build_dir/|$root/.jinx-build/|g" \
    -e "s|/base_dir/|$js/|g" \
    -e "s|/build_dir/|$root/.jinx-build/|g" \
    -e "s|\"x86_64-frostyos-|\"$tc/x86_64-frostyos-|g" \
    "$src" > "$outdir/compile_commands.json"