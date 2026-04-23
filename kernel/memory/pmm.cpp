// pmm.cpp — Physical Memory Manager implementasyonu

#include "pmm.hpp"
#include "../drivers/vga.hpp"

PMM* g_pmm = nullptr;

// ============================================================
// Constructor — RAII
// Adımlar:
//   1. Bitmap'i tamamen dolu (1) başlat (güvenli başlangıç)
//   2. Multiboot2 mmap'i parse et → available bölgeleri serbest bırak
//   3. Kernel + bitmap bölgesini tekrar dolu işaretle
// ============================================================
PMM::PMM(Mb2InfoHeader* mb2_info, uint64_t kernel_start, uint64_t kernel_end)
    : m_total_pages(0), m_used_pages(0), m_next_free_hint(0)
{
    // 1. Tüm bitmap'i dolu işaretle
    markAllUsed();

    // 2. Multiboot2 memory map'ini parse et
    Mb2Parser parser(mb2_info);
    parser.forEachMmapEntry([this](const Mb2MmapEntry* entry) {
        if (entry->type == MB2_MMAP_AVAILABLE) {
            // Kullanılabilir bölgeyi boş işaretle
            uint64_t base   = entry->base_addr;
            uint64_t length = entry->length;

            // Page sınırına hizala (güvenli taraf: yukarı)
            uint64_t aligned_base = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            uint64_t aligned_end  = (base + length) & ~(PAGE_SIZE - 1);

            if (aligned_end > aligned_base) {
                markRegionFree(aligned_base, aligned_end - aligned_base);
                m_total_pages += (aligned_end - aligned_base) / PAGE_SIZE;
            }
        }
    });

    // 3. Kritik bölgeleri tekrar dolu işaretle
    // İlk 1MB: BIOS, VGA memory, IVT, BDA vs.
    markRegionUsed(0, 0x100000);

    // Kernel binary'si
    markRegionUsed(kernel_start, kernel_end - kernel_start);

    // PMM'in kendi bitmap'i (bu PMM nesnesi kernel_end'den sonra statik storage'da)
    // kernel_end zaten dolu işaretlendi — bitmap BSS'de olduğu için tamam

    // Kullanılan sayfa sayısını hesapla
    for (uint64_t i = 0; i < BITMAP_UINT64S; i++) {
        // Dolu bit sayısını say (__builtin_popcountll = popcount intrinsic)
        m_used_pages += __builtin_popcountll(m_bitmap[i]);
    }
}

// ============================================================
// Bitmap İşlemleri (private)
// ============================================================
void PMM::setBit(uint64_t page)
{
    if (page >= MAX_PAGES) return;
    m_bitmap[page / 64] |= (1ULL << (page % 64));
}

void PMM::clearBit(uint64_t page)
{
    if (page >= MAX_PAGES) return;
    m_bitmap[page / 64] &= ~(1ULL << (page % 64));
}

bool PMM::testBit(uint64_t page) const
{
    if (page >= MAX_PAGES) return true; // Sınır dışı → dolu say
    return (m_bitmap[page / 64] >> (page % 64)) & 1;
}

void PMM::markAllUsed()
{
    for (uint64_t i = 0; i < BITMAP_UINT64S; i++) {
        m_bitmap[i] = 0xFFFFFFFFFFFFFFFFULL; // Tüm bitler 1
    }
}

void PMM::markRegionFree(uint64_t base, uint64_t length)
{
    uint64_t start_page = base / PAGE_SIZE;
    uint64_t page_count = length / PAGE_SIZE;
    for (uint64_t i = 0; i < page_count; i++) {
        clearBit(start_page + i);
    }
}

void PMM::markRegionUsed(uint64_t base, uint64_t length)
{
    // Sayfa sınırlarına hizala (dışa doğru — güvenli taraf)
    uint64_t start_page = base / PAGE_SIZE;
    uint64_t end_page   = (base + length + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t i = start_page; i < end_page; i++) {
        setBit(i);
    }
}

// ============================================================
// allocPage — Tek fiziksel page ayır
// Strateji: Next-fit (m_next_free_hint'ten başla)
// NASIL: Bitmap'de 0 olan ilk bit → o page'i dolu işaretle → adres döndür
// ============================================================
uint64_t PMM::allocPage()
{
    // Hint'ten başlayarak tüm bitmap'i tara
    for (uint64_t i = m_next_free_hint / 64; i < BITMAP_UINT64S; i++) {
        if (m_bitmap[i] == 0xFFFFFFFFFFFFFFFFULL) continue; // Tamamen dolu, atla

        // Bu uint64'te boş bit var
        // __builtin_ctzll: trailing zero count = ilk 0 bitin pozisyonu
        uint64_t free_bit = __builtin_ctzll(~m_bitmap[i]);
        uint64_t page     = i * 64 + free_bit;

        if (page >= MAX_PAGES) break;

        setBit(page);
        m_used_pages++;
        m_next_free_hint = page + 1;

        // Sayfa içeriğini sıfırla (güvenlik: önceki veriler görünmesin)
        uint64_t* addr = reinterpret_cast<uint64_t*>(page * PAGE_SIZE);
        for (int j = 0; j < 512; j++) addr[j] = 0;

        return page * PAGE_SIZE;
    }

    // Hint'ten sonra bulamadık, baştan dene
    if (m_next_free_hint > 0) {
        m_next_free_hint = 0;
        return allocPage();
    }

    return 0; // OOM — out of memory
}

// ============================================================
// allocPages — N ardışık page ayır
// ============================================================
uint64_t PMM::allocPages(uint64_t count)
{
    if (count == 0) return 0;
    if (count == 1) return allocPage();

    uint64_t consecutive = 0;
    uint64_t start_page  = 0;

    for (uint64_t page = 0; page < MAX_PAGES; page++) {
        if (!testBit(page)) {
            if (consecutive == 0) start_page = page;
            consecutive++;
            if (consecutive == count) {
                // Bulunan bölgeyi işaretle
                markRegionUsed(start_page * PAGE_SIZE, count * PAGE_SIZE);
                m_used_pages += count;
                return start_page * PAGE_SIZE;
            }
        } else {
            consecutive = 0;
        }
    }
    return 0; // Yeterli ardışık page yok
}

// ============================================================
// freePage / freePages
// ============================================================
void PMM::freePage(uint64_t phys_addr)
{
    uint64_t page = phys_addr / PAGE_SIZE;
    if (!testBit(page)) return; // Zaten boş — double free
    clearBit(page);
    m_used_pages--;
    if (page < m_next_free_hint) m_next_free_hint = page; // Hint'i geri al
}

void PMM::freePages(uint64_t phys_addr, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++) {
        freePage(phys_addr + i * PAGE_SIZE);
    }
}

// ============================================================
// dumpStats — VGA'ya bellek istatistiklerini yaz
// ============================================================
void PMM::dumpStats() const
{
    if (!g_vga) return;
    g_vga->printColored("[PMM]  ", VGAColor::Cyan);
    g_vga->print("Total: ");
    g_vga->printDec(totalBytes() / (1024 * 1024));
    g_vga->print(" MB | Used: ");
    g_vga->printDec(usedPages() * PAGE_SIZE / 1024);
    g_vga->print(" KB | Free: ");
    g_vga->printDec(freeBytes() / (1024 * 1024));
    g_vga->print(" MB\n");
}
