#!/usr/bin/env bash
set -euo pipefail
BOOTLOADER=$1
KERNEL=$2
IMAGE=$3

SIZE=$((64*1024*1024))                    # 64 MiB
rm -f "$IMAGE"
dd if=/dev/zero of="$IMAGE" bs=$SIZE count=1

# --- create GPT with 1-MiB alignment ---
parted "$IMAGE" --script -- \
  mklabel gpt \
  mkpart ESP fat32 1MiB 100% \
  set 1 esp on

mv build/BOOTX64.EFI+ build/BOOTX64.EFI

# ---- format partition & copy files ----
LOOP=$(losetup --show -fP "$IMAGE")       # /dev/loopX with …p1
mkfs.fat -F 32 "${LOOP}p1"

mmd   -i "${LOOP}p1" ::/EFI ::/EFI/BOOT
mcopy -i "${LOOP}p1" "$BOOTLOADER" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "${LOOP}p1" "$KERNEL"     ::/kernel.elf

losetup -d "$LOOP"
echo "ESP image ready: $IMAGE"
