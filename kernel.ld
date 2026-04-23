/* kernel.ld — MertOS Linker Script
 * Kernel'i 1MB (0x100000) fiziksel adresine yerleştirir.
 * x86_64 higher-half mapping için sanal adres 0xFFFFFFFF80100000.
 * Şu an için identity mapping (fiziksel = sanal) kullanıyoruz.
 */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)                    /* entry.asm içindeki _start sembolü */

SECTIONS {
    /* Kernel 1MB'dan başlar (GRUB konvansiyonu) */
    . = 0x100000;

    /* --- Kod Section'ı --- */
    .text ALIGN(4096) : {
        *(.multiboot2)           /* Multiboot2 header — EN BAŞTA olmalı */
        *(.text)
        *(.text.*)
    }

    /* --- Global Constructor/Destructor Tabloları --- */
    /* RAII ve static nesneler için gerekli */
    .init_array ALIGN(8) : {
        __init_array_start = .;
        *(.init_array)
        *(.ctors)
        __init_array_end = .;
    }

    .fini_array ALIGN(8) : {
        __fini_array_start = .;
        *(.fini_array)
        *(.dtors)
        __fini_array_end = .;
    }

    /* --- Read-Only Data --- */
    .rodata ALIGN(4096) : {
        *(.rodata)
        *(.rodata.*)
    }

    /* --- Initialized Data --- */
    .data ALIGN(4096) : {
        *(.data)
        *(.data.*)
    }

    /* --- BSS (Zero-initialized) --- */
    .bss ALIGN(4096) : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* Kernel'in bittiği adres — memory manager için gerekli */
    kernel_end = .;

    /* Debug/DWARF bilgileri (strip edilebilir) */
    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
    }
}
