# heart-os/Makefile
SHELL      = /usr/bin/env bash
BUILD_DIR  = build
EFIINC     = /usr/include/efi
EFILIB     = /usr/lib
CROSS      = x86_64-elf-
CC_ELF     = $(CROSS)gcc
LD_ELF     = $(CROSS)ld
AS_ELF     = $(CROSS)as
OBJCOPY    = $(CROSS)objcopy
CLANG_PE   = clang           # any recent clang can emit PE/COFF
LLD_LINK   = lld-link

CFLAGS_EFI = -I$(EFIINC) -I$(EFIINC)/x86_64 -ffreestanding -fshort-wchar \
             -fno-stack-protector -mno-red-zone -fPIC -Wall -Wextra -O2
LDFLAGS_EFI= -T $(EFILIB)/elf_x86_64_efi.lds -nostdlib -znocombreloc \
             -shared -Bsymbolic -L$(EFILIB) -lefi -lgnuefi

CFLAGS_KER = -ffreestanding -fno-stack-protector -mno-red-zone -Wall -Wextra -O2
LDFLAGS_KER= -T kernel/linker.ld -nostdlib

all: $(BUILD_DIR)/BOOTX64.EFI $(BUILD_DIR)/kernel.elf fat-image

$(BUILD_DIR):
	@mkdir -p $@

# ---------- Bootloader (UEFI PE/COFF) ----------
$(BUILD_DIR)/bootloader.o: bootloader/bootloader.c bootloader/elf.h | $(BUILD_DIR)
	$(CC_ELF) $(CFLAGS_EFI) -c $< -o $@

$(BUILD_DIR)/BOOTX64.EFI: $(BUILD_DIR)/bootloader.o
	$(LD_ELF) $(LDFLAGS_EFI) $< -o $@+

# ---------- Kernel (ELF) ----------
$(BUILD_DIR)/kernel.o: kernel/kernel.c kernel/bootinfo.h | $(BUILD_DIR)
	$(CC_ELF) $(CFLAGS_KER) -c $< -o $@

$(BUILD_DIR)/kernel.elf: $(BUILD_DIR)/kernel.o kernel/linker.ld
	$(LD_ELF) $(LDFLAGS_KER) $< -o $@

# ---------- Image ----------
fat-image: $(BUILD_DIR)/BOOTX64.EFI $(BUILD_DIR)/kernel.elf fat-image.sh
	bash fat-image.sh $(BUILD_DIR)/BOOTX64.EFI $(BUILD_DIR)/kernel.elf $(BUILD_DIR)/OS.img

run: fat-image
	qemu-system-x86_64 -drive if=pflash,format=raw,readonly,file=/usr/share/OVMF/OVMF_CODE.fd \
	                   -drive format=raw,file=$(BUILD_DIR)/heart.img,index=0,media=disk

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all fat-image run clean
