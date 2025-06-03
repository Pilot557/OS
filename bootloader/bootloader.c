/* bootloader/bootloader.c */
#include <efi.h>
#include <efilib.h>
#include "elf.h"           /* minimal ELF64 structs */

/* -------- Shared struct passed to kernel -------- */
typedef struct {
    void   *framebuffer;
    UINT32  width;
    UINT32  height;
    UINT32  pixels_per_scanline;
    UINT32  pixel_format;   /* 0 = RGB, 1 = BGR */
} BootInfo;

/* ------------------------------------------------ */
static EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

/* Very small helper to read a file from the ESP */
static EFI_STATUS load_file(IN EFI_HANDLE ImageHandle,
                            IN CHAR16    *Path,
                            OUT UINT8   **Buffer,
                            OUT UINTN    *Size)
{
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL *Root, *File;
    EFI_STATUS st;

    st = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle,
                           &LoadedImageProtocol, (void**)&LoadedImage);
    if (EFI_ERROR(st)) return st;

    st = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle,
                           &gEfiSimpleFileSystemProtocolGuid, (void**)&FileSystem);
    if (EFI_ERROR(st)) return st;

    st = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(st)) return st;

    st = uefi_call_wrapper(Root->Open, 5, Root, &File, Path,
                           EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(st)) return st;

    EFI_FILE_INFO *FileInfo;
    UINTN info_size = SIZE_OF_EFI_FILE_INFO + 200;
    st = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
                           info_size, (void**)&FileInfo);
    if (EFI_ERROR(st)) return st;

    st = uefi_call_wrapper(File->GetInfo, 4, File,
               &gEfiFileInfoGuid, &info_size, FileInfo);
    if (EFI_ERROR(st)) return st;

    *Size = FileInfo->FileSize;
    st = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
                           *Size, (void**)Buffer);
    if (EFI_ERROR(st)) return st;

    st = uefi_call_wrapper(File->Read, 3, File, Size, *Buffer);
    return st;
}

/* Simple ELF64 loader – supports only PT_LOAD */
static EFI_STATUS load_elf(UINT8 *elf, EFI_PHYSICAL_ADDRESS *entry)
{
    Elf64_Ehdr *eh = (Elf64_Ehdr*)elf;
    if (eh->e_ident[EI_MAG0] != ELFMAG0 ||
        eh->e_ident[EI_MAG1] != ELFMAG1 ||
        eh->e_ident[EI_MAG2] != ELFMAG2 ||
        eh->e_ident[EI_MAG3] != ELFMAG3)
        return EFI_LOAD_ERROR;

    Elf64_Phdr *ph = (Elf64_Phdr*)(elf + eh->e_phoff);
    for (UINT16 i = 0; i < eh->e_phnum; ++i, ++ph) {
        if (ph->p_type != PT_LOAD) continue;
        UINTN pages = (ph->p_memsz + 0xfff) / 0x1000;
        EFI_PHYSICAL_ADDRESS seg = ph->p_paddr;
        EFI_STATUS st = uefi_call_wrapper(BS->AllocatePages, 4,
                            AllocateAddress, EfiLoaderData, pages, &seg);
        if (EFI_ERROR(st)) return st;
        CopyMem((void*)(seg), elf + ph->p_offset, ph->p_filesz);
        SetMem((void*)(seg + ph->p_filesz), ph->p_memsz - ph->p_filesz, 0);
    }
    *entry = eh->e_entry;
    return EFI_SUCCESS;
}

/* ---------- UEFI entry point ---------- */
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    /* 1. Find GOP and pick a graphics mode */
    EFI_STATUS st = uefi_call_wrapper(BS->LocateProtocol, 3,
                  &gEfiGraphicsOutputProtocolGuid, NULL, (void**)&gop);
    if (EFI_ERROR(st)) return st;

    UINT32 best = 0;
    for (UINT32 i = 0; i < gop->Mode->MaxMode; ++i) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        UINTN sz;
        if (gop->QueryMode(gop, i, &sz, &info) == EFI_SUCCESS) {
            if (info->HorizontalResolution >= 640 && info->VerticalResolution >= 480)
                best = i;
        }
    }
    gop->SetMode(gop, best);

    /* 2. Read kernel from the ESP */
    UINT8 *kernel_buf; UINTN kernel_size;
    st = load_file(ImageHandle, L"\\kernel.elf", &kernel_buf, &kernel_size);
    if (EFI_ERROR(st)) { Print(L"Load kernel failed: %r\n", st); return st; }

    /* 3. Parse / load ELF */
    EFI_PHYSICAL_ADDRESS kernel_entry;
    st = load_elf(kernel_buf, &kernel_entry);
    if (EFI_ERROR(st)) { Print(L"ELF load failed: %r\n", st); return st; }

    /* 4. Prepare BootInfo */
    BootInfo *bi;
    st = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
                           sizeof(BootInfo), (void**)&bi);
    if (EFI_ERROR(st)) return st;
    bi->framebuffer         = (void*)gop->Mode->FrameBufferBase;
    bi->width               = gop->Mode->Info->HorizontalResolution;
    bi->height              = gop->Mode->Info->VerticalResolution;
    bi->pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine;
    bi->pixel_format        = (gop->Mode->Info->PixelFormat ==
                               PixelRedGreenBlueReserved8BitPerColor) ? 0 : 1;

    /* 5. Exit boot services */
    UINTN map_key, desc_sz; UINT32 desc_ver;
    EFI_MEMORY_DESCRIPTOR *mem_map = NULL;
    UINTN mmap_sz = 0;
    BS->GetMemoryMap(&mmap_sz, mem_map, &map_key, &desc_sz, &desc_ver);
    mmap_sz += desc_sz * 8;
    BS->AllocatePool(EfiLoaderData, mmap_sz, (void**)&mem_map);
    st = BS->GetMemoryMap(&mmap_sz, mem_map, &map_key, &desc_sz, &desc_ver);
    st = BS->ExitBootServices(ImageHandle, map_key);
    if (EFI_ERROR(st)) return st;  /* Try again in real boot code */

    /* 6. Jump into kernel */
    void (*kentry)(BootInfo*) = (void(*)(BootInfo*))kernel_entry;
    kentry(bi);

    /* Should never return */
    for(;;);
}
