// pmm.hpp — Physical Memory Manager (Page Frame Allocator)
// OOP: Encapsulation — bitmap tamamen private, alloc/free arayüzü public
// Midterm: MemoryManager sınıfı kriteri karşılanıyor

#pragma once
#include <stdint.h>
#include "multiboot2.hpp"

// ============================================================
// PMM Sınıfı — Bitmap Page Frame Allocator
//
// Strateji: Her 4KB fiziksel page için 1 bit.
//   0 = boş (free)
//   1 = dolu (used)
//
// 4GB RAM = 1.048.576 page = 131.072 uint64_t (128 KB bitmap)
//
// Midterm OOP:
//   - Encapsulation: bitmap + istatistikler private
//   - RAII: constructor Multiboot2'yi parse eder, hemen hazır
//   - Static singleton: kernel genelinde tek PMM instance
// ============================================================
class PMM {
public:
    static constexpr uint64_t PAGE_SIZE       = 4096;        // 4 KB
    static constexpr uint64_t PAGE_SHIFT      = 12;          // 2^12 = 4096
    static constexpr uint64_t MAX_PAGES       = 1024 * 1024; // 4 GB / 4 KB
    static constexpr uint64_t BITMAP_UINT64S  = MAX_PAGES / 64; // 16384 entries

    // ---- Yapıcı: RAII ----
    // Multiboot2 memory map'i parse eder, kullanılabilir sayfaları işaretler
    PMM(Mb2InfoHeader* mb2_info, uint64_t kernel_start, uint64_t kernel_end);

    // ---- Allokasyon ----

    // Tek sayfa ayır → fiziksel adres (0 = başarısız)
    uint64_t allocPage();

    // N adet ardışık sayfa ayır → ilk sayfanın fiziksel adresi
    uint64_t allocPages(uint64_t count);

    // ---- Serbest Bırakma ----
    void freePage(uint64_t phys_addr);
    void freePages(uint64_t phys_addr, uint64_t count);

    // ---- Sorgulama ----
    uint64_t totalPages()  const { return m_total_pages;  }
    uint64_t usedPages()   const { return m_used_pages;   }
    uint64_t freePages()   const { return m_total_pages - m_used_pages; }
    uint64_t totalBytes()  const { return m_total_pages  * PAGE_SIZE;   }
    uint64_t freeBytes()   const { return freePages()    * PAGE_SIZE;   }

    // Debug: bitmap durumunu VGA'ya yaz
    void dumpStats() const;

    // Fiziksel adres → page index dönüşümü (public yardımcı)
    static uint64_t addrToPage(uint64_t addr) { return addr >> PAGE_SHIFT; }
    static uint64_t pageToAddr(uint64_t page) { return page << PAGE_SHIFT; }

private:
    // ---- Bitmap ----
    // BSS'de statik: heap hazır olmadan önce kullanılabilir
    uint64_t m_bitmap[BITMAP_UINT64S];

    // ---- İstatistikler ----
    uint64_t m_total_pages;
    uint64_t m_used_pages;

    // Allokasyon için son aranan pozisyon (next-fit hızlandırması)
    uint64_t m_next_free_hint;

    // ---- Bitmap İşlemleri ----
    void setBit(uint64_t page);       // Sayfayı dolu işaretle
    void clearBit(uint64_t page);     // Sayfayı boş işaretle
    bool testBit(uint64_t page) const;

    // Tüm sayfaları dolu işaretle (başlangıçta)
    void markAllUsed();

    // Bir bölgeyi boş işaretle (Multiboot2 available bölgeler için)
    void markRegionFree(uint64_t base, uint64_t length);

    // Bir bölgeyi dolu işaretle (kernel, bitmap kendisi vb.)
    void markRegionUsed(uint64_t base, uint64_t length);
};

// Global PMM singleton
extern PMM* g_pmm;
