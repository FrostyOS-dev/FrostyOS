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

tar_file=$boot/FrostyOS/initramfs.tar
if [ -n "$changed" ] || [ ! -f "$tar_file" ]; then
    (cd "$rootfs" && tar -c --no-auto-compress --hard-dereference --owner=0 --group=0 -f "$tar_file.tmp" *)
    mv "$tar_file.tmp" "$tar_file"
fi

if [ ! -f "$iso" ] || [ -n "$(find "$boot" -type f -newer "$iso" -print -quit)" ]; then
    xorriso -report_about SORRY -as mkisofs -quiet --efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label "$boot" -o "$iso"
fi
