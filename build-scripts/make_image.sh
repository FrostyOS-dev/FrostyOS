#!/bin/sh
# usage: make_image.sh <boot-dir> <iso> <rootfs> <overlay-dir>...
set -e
boot=$1; iso=$2; rootfs=$3; shift 3
mkdir -p "$rootfs" "$(dirname "$iso")" "$boot/FrostyOS"

changed=
for src in "$@"; do
    c=$(rsync -aHi "$src"/ "$rootfs"/ | grep '^[>c<*]' || true)
    changed="$changed$c"
done

manifest=$(cd "$(dirname "$0")" && pwd)/x86_64/ownership.txt

tar_file=$boot/FrostyOS/initramfs.tar
if [ -n "$changed" ] || [ ! -f "$tar_file" ] || { [ -f "$manifest" ] && [ "$manifest" -nt "$tar_file" ]; }; then
    entries=$(mktemp); excludes=$(mktemp)
    trap 'rm -f "$entries" "$excludes"' EXIT

    # strip comments and blank lines
    [ -f "$manifest" ] && grep -v '^[[:space:]]*\(#\|$\)' "$manifest" > "$entries" || true
    while read -r path uid gid; do echo "$path"; done < "$entries" > "$excludes"

    rm -f "$tar_file.tmp"

    # pass 1: everything as root, except the listed paths
    (cd "$rootfs" && tar -c --no-auto-compress --hard-dereference --owner=0 --group=0 \
        --anchored --exclude-from="$excludes" -f "$tar_file.tmp" *)

    # pass 2: append each listed path with its own owner
    while read -r path uid gid; do
        (cd "$rootfs" && tar -r --no-auto-compress --hard-dereference \
            --owner="$uid" --group="$gid" -f "$tar_file.tmp" "$path")
    done < "$entries"

    mv "$tar_file.tmp" "$tar_file"
fi

if [ ! -f "$iso" ] || [ -n "$(find "$boot" -type f -newer "$iso" -print -quit)" ]; then
    xorriso -report_about SORRY -as mkisofs -quiet --efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label "$boot" -o "$iso"
fi
