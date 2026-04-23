// vmm.hpp — Virtual Memory Manager
// x86_64 4-level paging: PML4 → PDPT → PD → PT → Page
// OOP: Encapsulation — page table walk tamamen içeride

#pragma once
#include <stdint.h>
#include "pmm.hpp"

// ============================================================
// Page Table Entry (PTE) flag'leri — Intel SDM Vol.3 4.5
// ============================================================
namespace PageFlags {
    static constexpr uint64_t PRESENT      = (1ULL << 0);   // Sayfa bellekte
    static constexpr uint64_t WRITABLE     = (1ULL << 1);   // Yazılabilir
    static constexpr uint64_t USER         = (1ULL << 2);   // User-space erişim
    static constexpr uint64_t WRITE_THRU   = (1ULL << 3);   // Write-through cache
    static constexpr uint64_t NO_CACHE     = (1ULL << 4);   // Cache devre dışı
    static constexpr uint64_t ACCESSED     = (1ULL << 5);   // CPU set eder
    static constexpr uint64_t DIRTY        = (1ULL << 6);   // CPU set eder (PT için)
    static constexpr uint64_t HUGE         = (1ULL << 7);   // 2MB/1GB huge page
    static constexpr uint64_t GLOBAL       = (1ULL << 8);   // TLB'de tut
    static constexpr uint64_t NO_EXECUTE   = (1ULL << 63);  // NX bit

    // Kernel sayfası için standart flag seti
    static constexpr uint64_t KERNEL_RW    = PRESENT | WRITABLE;
    // Kernel read-only (kod için)
    static constexpr uint64_t KERNEL_RO    = PRESENT;
    // User sayfası
    static constexpr uint64_t USER_RW      = PRESENT | WRITABLE | USER;
}

// Page table entry'nin fiziksel adres maskesi (bit 12-51)
static constexpr uint64_t PTE_ADDR_MASK = 0x000FFFFFFFFFF000ULL;

// ============================================================
// VMM Sınıfı — Virtual Address Space Yöneticisi
//
// entry.asm'deki 2MB huge page identity mapping devam ediyor.
// VMM bu mapping'i yönetir ve yeni mapping'ler ekleyebilir.
//
// Midterm OOP:
//   - Encapsulation: page table walk private helper'larda
//   - RAII: constructor CR3'ten mevcut PML4'ü okur
//   - PMM ile Composition ilişkisi (page table için page ayırır)
// ============================================================
class VMM {
public:
    // ---- Yapıcı ----
    // cr3: mevcut PML4 fiziksel adresi (CR3 register'dan okunur)
    explicit VMM(PMM* pmm);

    // ---- Mapping İşlemleri ----

    // Sanal adresi fiziksel adrese bağla
    // virt_addr ve phys_addr PAGE_SIZE'a hizalı olmalı
    bool mapPage(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);

    // N sayfa bağla (ardışık sanal + fiziksel)
    bool mapPages(uint64_t virt_addr, uint64_t phys_addr,
                  uint64_t count, uint64_t flags);

    // Mapping'i kaldır
    void unmapPage(uint64_t virt_addr);

    // ---- Sorgulama ----

    // Sanal → fiziksel adres çözümle (map değilse 0 döner)
    uint64_t physAddr(uint64_t virt_addr) const;

    // Sayfa map'lı mı?
    bool isMapped(uint64_t virt_addr) const;

    // ---- Address Space ----

    // PML4 fiziksel adresi (CR3 değeri)
    uint64_t getPML4() const { return m_pml4_phys; }

    // Bu VMM'i aktif et (CR3 yükle → TLB flush)
    void activate() const;

    // TLB'yi belirli bir sanal adres için flush et
    static void flushTLB(uint64_t virt_addr);

    // Tüm TLB'yi flush et (CR3 yeniden yükle)
    void flushAllTLB() const;

private:
    PMM*     m_pmm;         // Page table page'leri için
    uint64_t m_pml4_phys;   // PML4 tablosunun fiziksel adresi

    // ---- Page Table Walk Yardımcıları ----

    // 4 seviyeli page table index'lerini çıkar
    static uint64_t pml4Index(uint64_t virt) { return (virt >> 39) & 0x1FF; }
    static uint64_t pdptIndex(uint64_t virt) { return (virt >> 30) & 0x1FF; }
    static uint64_t pdIndex  (uint64_t virt) { return (virt >> 21) & 0x1FF; }
    static uint64_t ptIndex  (uint64_t virt) { return (virt >> 12) & 0x1FF; }

    // PTE'den fiziksel adresi çıkar
    static uint64_t pteToAddr(uint64_t pte)  { return pte & PTE_ADDR_MASK; }

    // Bir üst tablonun entry'si yoksa yeni tablo oluştur
    // next_table_phys: oluşturulan tablonun adresi (out param)
    // Dönen değer: entry'ye yazılacak PTE değeri
    uint64_t* getOrCreateTable(uint64_t* parent_table, uint64_t index,
                                uint64_t flags);

    // Fiziksel adresi virtual'a çevir (kernel identity-mapped)
    // entry.asm'deki identity mapping sayesinde phys == virt (ilk 1GB için)
    static uint64_t* physToVirt(uint64_t phys) {
        return reinterpret_cast<uint64_t*>(phys);
    }
};

extern VMM* g_vmm;
