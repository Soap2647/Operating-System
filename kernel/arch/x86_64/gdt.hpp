// gdt.hpp — Global Descriptor Table (GDT) Yöneticisi
// OOP: Encapsulation — GDT yapısı dışarıya gizlenir
// Midterm notu: Bu sınıf "encapsulation" örneğidir

#pragma once
#include <stdint.h>

// GDT'deki her descriptor 8 byte
// Segment Descriptor formatı (Intel SDM Vol.3 3.4.5)
struct GDTDescriptor {
    uint16_t limit_low;         // Segment limit [0:15]
    uint16_t base_low;          // Base address [0:15]
    uint8_t  base_mid;          // Base address [16:23]
    uint8_t  access;            // Access byte (Present, DPL, type)
    uint8_t  granularity;       // Flags + limit [16:19]
    uint8_t  base_high;         // Base address [24:31]
} __attribute__((packed));

// GDTR register'ına yüklenecek pointer yapısı
struct GDTPointer {
    uint16_t limit;             // GDT boyutu - 1
    uint64_t base;              // GDT'nin sanal adresi
} __attribute__((packed));

// ============================================================
// GDT Sınıfı — Midterm OOP Bağlantısı:
//   - Encapsulation: descriptor'lar private
//   - RAII: constructor'da kurulum yapılır
// ============================================================
class GDT {
public:
    // Segment index'leri (GDT selector değerleri / 8)
    static constexpr uint16_t KERNEL_CODE_SELECTOR = 0x08;
    static constexpr uint16_t KERNEL_DATA_SELECTOR = 0x10;
    static constexpr uint16_t USER_CODE_SELECTOR   = 0x18;
    static constexpr uint16_t USER_DATA_SELECTOR   = 0x20;

    // RAII: Constructor GDT'yi kurar ve yükler
    GDT();

    // GDT'yi CPU'ya yükle (assembly gerektirir)
    void load();

private:
    static constexpr int GDT_ENTRIES = 5;

    GDTDescriptor entries[GDT_ENTRIES];
    GDTPointer    pointer;

    // Descriptor oluşturma yardımcısı
    void setDescriptor(int index, uint32_t base, uint32_t limit,
                       uint8_t access, uint8_t granularity);
};

// GDT singleton — kernel genelinde tek örnek
// (Singleton pattern, ileride ProcessManager için de kullanacağız)
extern GDT* g_gdt;
