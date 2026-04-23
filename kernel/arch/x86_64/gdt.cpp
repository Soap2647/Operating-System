// gdt.cpp — GDT implementasyonu
// x86_64 long mode'da GDT segment'leri minimal —
// güvenlik ve izolasyon paging ile yapılır

#include "gdt.hpp"

GDT* g_gdt = nullptr;

// ============================================================
// Access byte bit anlamları:
//   Bit 7: Present (1 = geçerli)
//   Bit 6-5: DPL (Descriptor Privilege Level, 0=kernel 3=user)
//   Bit 4: Descriptor type (1 = code/data, 0 = system)
//   Bit 3: Executable (1 = code segment)
//   Bit 2: Direction/Conforming
//   Bit 1: Readable/Writable
//   Bit 0: Accessed (CPU set eder)
// Granularity byte:
//   Bit 7: Granularity (1 = 4KB blocks)
//   Bit 6: Size (0 = 64-bit, 1 = 32-bit)
//   Bit 5: Long mode (1 = 64-bit code segment)
//   Bit 4: AVL (available)
// ============================================================

void GDT::setDescriptor(int index, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t granularity)
{
    entries[index].base_low    = (base & 0xFFFF);
    entries[index].base_mid    = (base >> 16) & 0xFF;
    entries[index].base_high   = (base >> 24) & 0xFF;
    entries[index].limit_low   = (limit & 0xFFFF);
    entries[index].granularity = ((limit >> 16) & 0x0F) | (granularity & 0xF0);
    entries[index].access      = access;
}

GDT::GDT()
{
    // 0: Null descriptor (zorunlu — CPU sıfırlama sonrası DS=0 olabilir)
    setDescriptor(0, 0, 0, 0, 0);

    // 1: Kernel Code Segment (Ring 0, 64-bit)
    //    Access: Present | DPL=0 | Code | Executable | Readable
    //    Gran:   Long Mode (L=1), Granularity=1
    setDescriptor(1, 0, 0xFFFFF, 0x9A, 0xA0);
    //              base  limit    access  gran
    //   0x9A = 1001 1010 = Present | DPL0 | Code | Exec | Read
    //   0xA0 = 1010 0000 = Gran | Long Mode

    // 2: Kernel Data Segment (Ring 0)
    //    Access: Present | DPL=0 | Data | Writable
    setDescriptor(2, 0, 0xFFFFF, 0x92, 0xC0);
    //   0x92 = 1001 0010 = Present | DPL0 | Data | Write
    //   0xC0 = 1100 0000 = Gran | 32-bit (data segment)

    // 3: User Code Segment (Ring 3, 64-bit) — ileride process için
    setDescriptor(3, 0, 0xFFFFF, 0xFA, 0xA0);
    //   0xFA = 1111 1010 = Present | DPL3 | Code | Exec | Read

    // 4: User Data Segment (Ring 3)
    setDescriptor(4, 0, 0xFFFFF, 0xF2, 0xC0);
    //   0xF2 = 1111 0010 = Present | DPL3 | Data | Write

    pointer.limit = sizeof(entries) - 1;
    pointer.base  = reinterpret_cast<uint64_t>(&entries[0]);

    load();
}

// GDT yükle ve segment register'larını güncelle
void GDT::load()
{
    asm volatile (
        "lgdt %0\n\t"           // GDT'yi yükle
        // Kod segment'ini güncellemek için far jump (ret trick)
        "push %1\n\t"           // Yeni CS'i stack'e koy
        "lea  1f(%%rip), %%rax\n\t"  // Sonraki instruction adresi
        "push %%rax\n\t"
        "lretq\n\t"             // Far return → CS güncellenir
        "1:\n\t"
        // Data segment register'larını güncelle
        "mov %2, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        :
        : "m"(pointer),
          "i"(KERNEL_CODE_SELECTOR),
          "i"(KERNEL_DATA_SELECTOR)
        : "rax", "memory"
    );
}
