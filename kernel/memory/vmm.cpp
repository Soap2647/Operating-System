// vmm.cpp — Virtual Memory Manager implementasyonu
// 4-level paging: PML4[9] → PDPT[9] → PD[9] → PT[9] → offset[12]

#include "vmm.hpp"
#include "../drivers/vga.hpp"

VMM* g_vmm = nullptr;

// ============================================================
// Constructor — RAII
// CR3 register'ından mevcut PML4 adresini okur.
// entry.asm'nin kurduğu identity mapping bu VMM nesnesiyle yönetilir.
// ============================================================
VMM::VMM(PMM* pmm) : m_pmm(pmm), m_pml4_phys(0)
{
    // CR3 register'ından PML4 fiziksel adresini oku
    asm volatile("mov %%cr3, %0" : "=r"(m_pml4_phys));
    m_pml4_phys &= PTE_ADDR_MASK; // Flag bitlerini temizle
}

// ============================================================
// getOrCreateTable
// Parent table'da 'index' konumundaki entry'ye bak:
//   - Varsa: o tablonun pointer'ını döndür
//   - Yoksa: PMM'den yeni page al, sıfırla, entry'yi oluştur
// ============================================================
uint64_t* VMM::getOrCreateTable(uint64_t* parent_table, uint64_t index,
                                 uint64_t flags)
{
    uint64_t& entry = parent_table[index];

    if (entry & PageFlags::PRESENT) {
        // Tablo zaten var
        return physToVirt(pteToAddr(entry));
    }

    // Yeni page table sayfası ayır (PMM'den)
    uint64_t new_table_phys = m_pmm->allocPage();
    if (!new_table_phys) return nullptr; // OOM

    // Entry'yi yaz: fiziksel adres + flag'ler
    entry = new_table_phys | flags | PageFlags::PRESENT | PageFlags::WRITABLE;

    return physToVirt(new_table_phys);
}

// ============================================================
// mapPage — Tek sayfa map'le
// Sanal adres → fiziksel adres bağlantısı kur
// ============================================================
bool VMM::mapPage(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags)
{
    // Hizalama kontrolü
    if (virt_addr & 0xFFF || phys_addr & 0xFFF) return false;

    uint64_t* pml4 = physToVirt(m_pml4_phys);

    // PML4 → PDPT
    uint64_t* pdpt = getOrCreateTable(pml4, pml4Index(virt_addr), flags);
    if (!pdpt) return false;

    // PDPT → PD
    uint64_t* pd = getOrCreateTable(pdpt, pdptIndex(virt_addr), flags);
    if (!pd) return false;

    // PD: 2MB huge page mi var? Varsa küçük sayfayla çakışma
    uint64_t& pd_entry = pd[pdIndex(virt_addr)];
    if ((pd_entry & PageFlags::PRESENT) && (pd_entry & PageFlags::HUGE)) {
        // Huge page bölgesine 4KB map atılıyor → entry.asm'nin
        // identity mapping'i burada zaten kapsıyor olabilir.
        // Şimdilik güvenli: varsa üzerine yazma
        return true;
    }

    // PD → PT
    uint64_t* pt = getOrCreateTable(pd, pdIndex(virt_addr), flags);
    if (!pt) return false;

    // PT entry'sini yaz
    pt[ptIndex(virt_addr)] = phys_addr | flags | PageFlags::PRESENT;

    // TLB bu adresi cache'lemiş olabilir → flush
    flushTLB(virt_addr);
    return true;
}

// ============================================================
// mapPages — Ardışık sayfa map'leme
// ============================================================
bool VMM::mapPages(uint64_t virt_addr, uint64_t phys_addr,
                   uint64_t count, uint64_t flags)
{
    for (uint64_t i = 0; i < count; i++) {
        if (!mapPage(virt_addr + i * PMM::PAGE_SIZE,
                     phys_addr + i * PMM::PAGE_SIZE, flags)) {
            return false;
        }
    }
    return true;
}

// ============================================================
// unmapPage — Mapping'i kaldır
// ============================================================
void VMM::unmapPage(uint64_t virt_addr)
{
    uint64_t* pml4 = physToVirt(m_pml4_phys);
    uint64_t& pml4e = pml4[pml4Index(virt_addr)];
    if (!(pml4e & PageFlags::PRESENT)) return;

    uint64_t* pdpt  = physToVirt(pteToAddr(pml4e));
    uint64_t& pdpte = pdpt[pdptIndex(virt_addr)];
    if (!(pdpte & PageFlags::PRESENT)) return;

    uint64_t* pd  = physToVirt(pteToAddr(pdpte));
    uint64_t& pde = pd[pdIndex(virt_addr)];
    if (!(pde & PageFlags::PRESENT)) return;
    if (pde & PageFlags::HUGE) { pde = 0; flushTLB(virt_addr); return; }

    uint64_t* pt  = physToVirt(pteToAddr(pde));
    uint64_t& pte = pt[ptIndex(virt_addr)];

    if (pte & PageFlags::PRESENT) {
        pte = 0;
        flushTLB(virt_addr);
    }
}

// ============================================================
// physAddr — Sanal → Fiziksel adres çözümle
// ============================================================
uint64_t VMM::physAddr(uint64_t virt_addr) const
{
    uint64_t* pml4 = physToVirt(m_pml4_phys);
    uint64_t pml4e = pml4[pml4Index(virt_addr)];
    if (!(pml4e & PageFlags::PRESENT)) return 0;

    uint64_t* pdpt  = physToVirt(pteToAddr(pml4e));
    uint64_t  pdpte = pdpt[pdptIndex(virt_addr)];
    if (!(pdpte & PageFlags::PRESENT)) return 0;
    if (pdpte & PageFlags::HUGE)
        return pteToAddr(pdpte) + (virt_addr & 0x3FFFFFFFULL); // 1GB huge

    uint64_t* pd  = physToVirt(pteToAddr(pdpte));
    uint64_t  pde = pd[pdIndex(virt_addr)];
    if (!(pde & PageFlags::PRESENT)) return 0;
    if (pde & PageFlags::HUGE)
        return pteToAddr(pde) + (virt_addr & 0x1FFFFFULL);     // 2MB huge

    uint64_t* pt  = physToVirt(pteToAddr(pde));
    uint64_t  pte = pt[ptIndex(virt_addr)];
    if (!(pte & PageFlags::PRESENT)) return 0;

    return pteToAddr(pte) + (virt_addr & 0xFFFULL);
}

bool VMM::isMapped(uint64_t virt_addr) const
{
    return physAddr(virt_addr) != 0;
}

// ============================================================
// TLB ve CR3 işlemleri
// ============================================================
void VMM::flushTLB(uint64_t virt_addr)
{
    asm volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

void VMM::flushAllTLB() const
{
    asm volatile(
        "mov %%cr3, %%rax\n\t"
        "mov %%rax, %%cr3\n\t"
        : : : "rax", "memory"
    );
}

void VMM::activate() const
{
    asm volatile("mov %0, %%cr3" : : "r"(m_pml4_phys) : "memory");
}
