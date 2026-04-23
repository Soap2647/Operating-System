// multiboot2.hpp — Multiboot2 Info Yapıları
// GRUB, kernel'e MB2 bilgi pointer'ını (RSI) aktarır.
// Bu yapılar Multiboot2 spec §3.6'ya göre tanımlanmıştır.

#pragma once
#include <stdint.h>

// ============================================================
// Multiboot2 sihirli sayı (entry.asm'den gelir, EDI'de)
// ============================================================
static constexpr uint32_t MB2_MAGIC = 0x36D76289;

// ============================================================
// Tag type sabitleri
// ============================================================
static constexpr uint32_t MB2_TAG_END          = 0;
static constexpr uint32_t MB2_TAG_CMDLINE      = 1;
static constexpr uint32_t MB2_TAG_BOOT_LOADER  = 2;
static constexpr uint32_t MB2_TAG_MMAP         = 6;  // Memory Map
static constexpr uint32_t MB2_TAG_FRAMEBUFFER  = 8;
static constexpr uint32_t MB2_TAG_ACPI_OLD     = 14;
static constexpr uint32_t MB2_TAG_ACPI_NEW     = 15;

// ============================================================
// Temel tag başlığı — tüm tag'ler bu yapıyla başlar
// ============================================================
struct Mb2Tag {
    uint32_t type;
    uint32_t size;
    // Veriler hemen arkasından gelir
} __attribute__((packed));

// ============================================================
// Info header — RSI/ESI pointer'ının gösterdiği yer
// ============================================================
struct Mb2InfoHeader {
    uint32_t total_size;
    uint32_t reserved;
    // Hemen arkasında 8-byte hizalı tag'ler gelir
} __attribute__((packed));

// ============================================================
// Memory Map Tag (type = 6)
// ============================================================
struct Mb2TagMmap {
    uint32_t type;          // = 6
    uint32_t size;
    uint32_t entry_size;    // Her entry'nin byte boyutu (genellikle 24)
    uint32_t entry_version; // = 0
    // Arkasında Mb2MmapEntry dizisi
} __attribute__((packed));

// Memory map entry tipleri
static constexpr uint32_t MB2_MMAP_AVAILABLE    = 1;  // Kullanılabilir RAM
static constexpr uint32_t MB2_MMAP_RESERVED     = 2;  // Rezerve
static constexpr uint32_t MB2_MMAP_ACPI         = 3;  // ACPI reclaimable
static constexpr uint32_t MB2_MMAP_NVS          = 4;  // ACPI NVS
static constexpr uint32_t MB2_MMAP_DEFECTIVE    = 5;  // Bozuk RAM

struct Mb2MmapEntry {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;          // Yukarıdaki sabitler
    uint32_t reserved;
} __attribute__((packed));

// ============================================================
// Framebuffer Tag (type = 8) — ileride VESA için
// ============================================================
struct Mb2TagFramebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t address;       // Framebuffer fiziksel adresi
    uint32_t pitch;         // Satır başına byte sayısı
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;           // Bit per pixel
    uint8_t  fb_type;       // 0=indexed, 1=direct RGB, 2=EGA text (Multiboot2 spec)
    uint16_t reserved;
} __attribute__((packed));

// ============================================================
// Multiboot2 Parser Yardımcısı
// ============================================================
class Mb2Parser {
public:
    explicit Mb2Parser(Mb2InfoHeader* info) : m_info(info) {}

    // Memory map tag'ini bul
    const Mb2TagMmap* findMemoryMap() const {
        return reinterpret_cast<const Mb2TagMmap*>(findTag(MB2_TAG_MMAP));
    }

    // Framebuffer tag'ini bul (ileride VESA için)
    const Mb2TagFramebuffer* findFramebuffer() const {
        return reinterpret_cast<const Mb2TagFramebuffer*>(findTag(MB2_TAG_FRAMEBUFFER));
    }

    // Memory map entry iterator — callback pattern
    // (STL olmadan range-for yerine bu kullanılır)
    template<typename Callback>
    void forEachMmapEntry(Callback cb) const {
        const Mb2TagMmap* mmap = findMemoryMap();
        if (!mmap) return;

        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(mmap) + sizeof(Mb2TagMmap);
        const uint8_t* end = reinterpret_cast<const uint8_t*>(mmap) + mmap->size;

        while (ptr < end) {
            cb(reinterpret_cast<const Mb2MmapEntry*>(ptr));
            ptr += mmap->entry_size;
        }
    }

private:
    const Mb2Tag* findTag(uint32_t type) const {
        if (!m_info) return nullptr;

        // Info header'dan sonra tag'ler başlar (8-byte hizalı)
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(m_info) + sizeof(Mb2InfoHeader);
        const uint8_t* end = reinterpret_cast<const uint8_t*>(m_info) + m_info->total_size;

        while (ptr < end) {
            const Mb2Tag* tag = reinterpret_cast<const Mb2Tag*>(ptr);
            if (tag->type == MB2_TAG_END) break;
            if (tag->type == type) return tag;

            // Bir sonraki tag: mevcut tag + size, 8-byte'a hizala
            ptr += (tag->size + 7) & ~7;
        }
        return nullptr;
    }

    Mb2InfoHeader* m_info;
};
