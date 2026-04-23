// heap.cpp — Kernel Heap Allocator implementasyonu
//
// Algoritma detayı:
//   alloc(N) → free list'te first-fit → split → döner
//   free(P)  → coalesce(prev, self, next) → free list'e ekle
//   grow()   → PMM'den sayfa al → sahte-allocated blok oluştur → free et

#include "heap.hpp"
#include "../drivers/vga.hpp"

Heap* g_heap = nullptr;

// ============================================================
// Statik Yardımcılar
// ============================================================
uint64_t Heap::alignUp(uint64_t size)
{
    uint64_t s = (size < MIN_PAYLOAD) ? MIN_PAYLOAD : size;
    return (s + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

void* Heap::headerToUser(BlockHeader* h)
{
    return reinterpret_cast<void*>(
        reinterpret_cast<uint64_t>(h) + HEADER_SIZE
    );
}

Heap::BlockHeader* Heap::userToHeader(void* ptr)
{
    return reinterpret_cast<BlockHeader*>(
        reinterpret_cast<uint64_t>(ptr) - HEADER_SIZE
    );
}

Heap::BlockFooter* Heap::headerToFooter(BlockHeader* h)
{
    return reinterpret_cast<BlockFooter*>(
        reinterpret_cast<uint64_t>(h) + HEADER_SIZE + h->payload
    );
}

Heap::BlockHeader* Heap::footerToPrevHeader(BlockFooter* f)
{
    // Footer'ın hemen öncesinde o bloğun payload alanı var.
    // Bloğun başına: footer adresi - payload - HEADER_SIZE
    return reinterpret_cast<BlockHeader*>(
        reinterpret_cast<uint64_t>(f) - f->payload - HEADER_SIZE
    );
}

void Heap::syncFooter(BlockHeader* h)
{
    BlockFooter* f = headerToFooter(h);
    f->payload = h->payload;
    f->magic   = h->magic;
}

bool Heap::isFree(const BlockHeader* block) const
{
    return block->magic == MAGIC_FREE;
}

// ============================================================
// Free List İşlemleri
// ============================================================
void Heap::insertFree(BlockHeader* block)
{
    // Başa ekle (LIFO — temporal locality için iyi)
    block->free_prev = nullptr;
    block->free_next = m_free_list;
    if (m_free_list) m_free_list->free_prev = block;
    m_free_list = block;
}

void Heap::removeFree(BlockHeader* block)
{
    if (block->free_prev) block->free_prev->free_next = block->free_next;
    else                  m_free_list                 = block->free_next;

    if (block->free_next) block->free_next->free_prev = block->free_prev;

    block->free_prev = nullptr;
    block->free_next = nullptr;
}

void Heap::markFree(BlockHeader* block)
{
    block->magic = MAGIC_FREE;
    syncFooter(block);
}

void Heap::markUsed(BlockHeader* block)
{
    block->magic     = MAGIC_USED;
    block->free_prev = nullptr;
    block->free_next = nullptr;
    syncFooter(block);
}

// ============================================================
// Constructor — RAII
// PMM'den 'initial_pages' × 4KB alır, ilk free bloğu oluşturur
// ============================================================
Heap::Heap(PMM* pmm, uint64_t initial_pages)
    : m_pmm(pmm), m_free_list(nullptr), m_first(nullptr), m_last(nullptr),
      m_start(0), m_end(0), m_total_bytes(0), m_used_bytes(0),
      m_alloc_count(0), m_free_count(0)
{
    uint64_t phys = pmm->allocPages(initial_pages);
    if (!phys) return; // PMM hatası — heap başlatılamadı

    // Identity mapping sayesinde phys == virt (ilk 1GB)
    m_start      = phys;
    m_end        = phys + initial_pages * PMM::PAGE_SIZE;
    m_total_bytes = initial_pages * PMM::PAGE_SIZE;

    //  Heap layout (başlangıçta):
    //  [ PROLOGUE header ] [ tek büyük FREE blok ] [ EPILOGUE header ]
    //
    //  Prologue  : sıfır payload, USED — backward coalescing sınırı
    //  Epilogue  : sıfır payload, USED — forward  coalescing sınırı

    uint64_t addr = m_start;

    // ---- Prologue bloğu ----
    BlockHeader* prologue = reinterpret_cast<BlockHeader*>(addr);
    prologue->magic     = MAGIC_USED;
    prologue->payload   = 0;
    prologue->phys_prev = nullptr;
    prologue->phys_next = nullptr;
    prologue->free_prev = nullptr;
    prologue->free_next = nullptr;
    // Prologue footer (0 payload → header hemen ardından footer var)
    BlockFooter* pf = headerToFooter(prologue);
    pf->payload = 0;
    pf->magic   = MAGIC_USED;

    addr += HEADER_SIZE + 0 + FOOTER_SIZE; // Prologue toplam boyutu

    // ---- Ana free blok ----
    // Toplam alan: m_total_bytes - prologue - epilogue
    uint64_t main_payload = m_total_bytes
                          - (HEADER_SIZE + FOOTER_SIZE)   // prologue
                          - (HEADER_SIZE + FOOTER_SIZE)   // epilogue
                          - HEADER_SIZE - FOOTER_SIZE;    // ana blok overhead
    // Hizala
    main_payload = (main_payload / ALIGNMENT) * ALIGNMENT;

    BlockHeader* main_block = reinterpret_cast<BlockHeader*>(addr);
    main_block->magic     = MAGIC_FREE;
    main_block->payload   = static_cast<uint32_t>(main_payload);
    main_block->phys_prev = prologue;
    main_block->phys_next = nullptr; // Epilogue'dan önce sonra set edilecek
    main_block->free_prev = nullptr;
    main_block->free_next = nullptr;
    syncFooter(main_block);

    addr += HEADER_SIZE + main_payload + FOOTER_SIZE;

    // ---- Epilogue bloğu ----
    BlockHeader* epilogue = reinterpret_cast<BlockHeader*>(addr);
    epilogue->magic     = MAGIC_USED;
    epilogue->payload   = 0;
    epilogue->phys_prev = main_block;
    epilogue->phys_next = nullptr;
    epilogue->free_prev = nullptr;
    epilogue->free_next = nullptr;
    BlockFooter* ef = headerToFooter(epilogue);
    ef->payload = 0;
    ef->magic   = MAGIC_USED;

    // Bağlantıları tamamla
    prologue->phys_next   = main_block;
    main_block->phys_next = epilogue;

    m_first     = prologue;
    m_last      = epilogue;
    m_free_list = main_block; // Free list'e ekle
}

// ============================================================
// grow — Heap'i genişlet
// PMM'den yeni sayfalar al, epilogue'u genişlet, free blok ekle
// ============================================================
bool Heap::grow(uint64_t min_bytes)
{
    // Yeterli sayfa hesapla
    uint64_t pages = (min_bytes + OVERHEAD + PMM::PAGE_SIZE - 1) / PMM::PAGE_SIZE;
    if (pages < GROW_PAGES) pages = GROW_PAGES;

    uint64_t phys = m_pmm->allocPages(pages);
    if (!phys) return false; // OOM

    // Identity mapping — yeni alan doğrudan erişilebilir
    // Yeni alan m_end'den başlamalı (ardışık olmalı)
    // PMM bitmap allocator ardışık sayfa garantisi vermez —
    // bu yüzden şimdilik sadece ardışık alloklarda grow çalışır
    // (ileride heap segmentation ile çözülecek)
    if (phys != m_end) {
        // Ardışık değil — o sayfaları geri ver (fragmentasyon önlemi)
        m_pmm->freePages(phys, pages);
        return false;
    }

    uint64_t new_bytes = pages * PMM::PAGE_SIZE;
    uint64_t new_end   = m_end + new_bytes;

    // Epilogue'u kaldır, yerine yeni free blok koy, ardından yeni epilogue
    BlockHeader* old_epilogue = m_last;
    uint64_t     epilogue_addr = reinterpret_cast<uint64_t>(old_epilogue);

    // Yeni free blok (eski epilogue konumundan başlar)
    uint64_t new_payload = new_bytes - OVERHEAD;
    new_payload = (new_payload / ALIGNMENT) * ALIGNMENT;

    BlockHeader* new_block    = old_epilogue;
    new_block->magic          = MAGIC_FREE;
    new_block->payload        = static_cast<uint32_t>(new_payload);
    new_block->phys_prev      = old_epilogue->phys_prev;
    // phys_next sonra set edilecek
    new_block->free_prev      = nullptr;
    new_block->free_next      = nullptr;
    syncFooter(new_block);

    // Önceki bloğun phys_next'ini güncelle
    if (new_block->phys_prev)
        new_block->phys_prev->phys_next = new_block;

    // Yeni epilogue
    uint64_t     new_ep_addr  = epilogue_addr + HEADER_SIZE + new_payload + FOOTER_SIZE;
    BlockHeader* new_epilogue = reinterpret_cast<BlockHeader*>(new_ep_addr);
    new_epilogue->magic     = MAGIC_USED;
    new_epilogue->payload   = 0;
    new_epilogue->phys_prev = new_block;
    new_epilogue->phys_next = nullptr;
    new_epilogue->free_prev = nullptr;
    new_epilogue->free_next = nullptr;
    BlockFooter* nef = headerToFooter(new_epilogue);
    nef->payload = 0;
    nef->magic   = MAGIC_USED;

    new_block->phys_next = new_epilogue;
    m_last               = new_epilogue;
    m_end                = new_end;
    m_total_bytes       += new_bytes;

    // Free list'e ekle ve önceki serbest blokla coalesce et
    insertFree(new_block);
    coalesce(new_block);

    return true;
}

// ============================================================
// split — Büyük free bloğu ikiye böl
// block   : payload >= size + MIN_BLOCK olan free blok
// size    : kullanıcıya verilecek payload boyutu
// ============================================================
Heap::BlockHeader* Heap::split(BlockHeader* block, uint64_t size)
{
    uint64_t remaining = block->payload - size - OVERHEAD;

    if (remaining < MIN_PAYLOAD) {
        // Bölmek için yer yok — bloğu olduğu gibi kullan
        return block;
    }

    // ---- Yeni sağ blok oluştur (kalan alan) ----
    uint64_t right_addr = reinterpret_cast<uint64_t>(block)
                        + HEADER_SIZE + size + FOOTER_SIZE;
    BlockHeader* right = reinterpret_cast<BlockHeader*>(right_addr);

    right->magic     = MAGIC_FREE;
    right->payload   = static_cast<uint32_t>(remaining);
    right->phys_prev = block;
    right->phys_next = block->phys_next;
    right->free_prev = nullptr;
    right->free_next = nullptr;
    syncFooter(right);

    // Sağ bloğun sonrasındaki bloğun phys_prev'ini güncelle
    if (right->phys_next)
        right->phys_next->phys_prev = right;

    // ---- Sol bloğu (kullanılacak) güncelle ----
    block->payload   = static_cast<uint32_t>(size);
    block->phys_next = right;
    syncFooter(block);

    // Sağ bloğu free list'e ekle
    insertFree(right);

    return block;
}

// ============================================================
// coalesce — Serbest bırakılan bloğu komşularla birleştir
// Dört durum:
//   1. Her iki komşu used  → sadece ekle
//   2. Sağ komşu free      → sağla birleş
//   3. Sol komşu free      → solabirleş
//   4. Her ikisi de free   → üçünü birleştir
// ============================================================
Heap::BlockHeader* Heap::coalesce(BlockHeader* block)
{
    BlockHeader* prev_block = block->phys_prev;
    BlockHeader* next_block = block->phys_next;

    bool prev_free = prev_block && isFree(prev_block);
    bool next_free = next_block && isFree(next_block);

    if (!prev_free && !next_free) {
        // Durum 1: Komşular dolu → sadece free list'e ekle
        return block;
    }

    if (!prev_free && next_free) {
        // Durum 2: Sadece sağ komşu free → sağla birleş
        removeFree(block);
        removeFree(next_block);

        block->payload   += OVERHEAD + next_block->payload;
        block->phys_next  = next_block->phys_next;
        if (block->phys_next)
            block->phys_next->phys_prev = block;
        syncFooter(block);

        insertFree(block);
        return block;
    }

    if (prev_free && !next_free) {
        // Durum 3: Sadece sol komşu free → solabirleş
        removeFree(prev_block);
        removeFree(block);

        prev_block->payload   += OVERHEAD + block->payload;
        prev_block->phys_next  = block->phys_next;
        if (prev_block->phys_next)
            prev_block->phys_next->phys_prev = prev_block;
        syncFooter(prev_block);

        insertFree(prev_block);
        return prev_block;
    }

    // Durum 4: Her iki komşu da free → üçünü birleştir
    removeFree(prev_block);
    removeFree(block);
    removeFree(next_block);

    prev_block->payload  += OVERHEAD + block->payload
                          + OVERHEAD + next_block->payload;
    prev_block->phys_next = next_block->phys_next;
    if (prev_block->phys_next)
        prev_block->phys_next->phys_prev = prev_block;
    syncFooter(prev_block);

    insertFree(prev_block);
    return prev_block;
}

// ============================================================
// alloc — Ana allokasyon fonksiyonu
// ============================================================
void* Heap::alloc(uint64_t size)
{
    if (size == 0) return nullptr;

    uint64_t aligned_size = alignUp(size);

    // ---- First-fit: free list'te ara ----
    BlockHeader* found = nullptr;
    for (BlockHeader* b = m_free_list; b; b = b->free_next) {
        if (b->payload >= aligned_size) {
            found = b;
            break;
        }
    }

    // ---- Bulunamadıysa heap'i büyüt ----
    if (!found) {
        if (!grow(aligned_size + OVERHEAD)) return nullptr;

        // grow() coalesce etti, yeni blok free list'in başında
        for (BlockHeader* b = m_free_list; b; b = b->free_next) {
            if (b->payload >= aligned_size) {
                found = b;
                break;
            }
        }
        if (!found) return nullptr; // Hâlâ bulunamadı
    }

    // ---- Free list'ten çıkar ----
    removeFree(found);

    // ---- Split (kalan alan varsa) ----
    found = split(found, aligned_size);

    // ---- Dolu işaretle ----
    markUsed(found);

    m_used_bytes += found->payload + OVERHEAD;
    m_alloc_count++;

    return headerToUser(found);
}

// ============================================================
// free — Serbest bırak + coalesce
// ============================================================
void Heap::free(void* ptr)
{
    if (!ptr) return;

    BlockHeader* block = userToHeader(ptr);

    // Magic number doğrulama — corruption tespiti
    if (block->magic != MAGIC_USED) {
        // Double-free veya heap corruption
        if (g_vga) {
            g_vga->printColored("[HEAP] ERROR: free() corruption! ptr=0x",
                                VGAColor::Red);
            g_vga->printHex(reinterpret_cast<uint64_t>(ptr));
            g_vga->print("\n");
        }
        return;
    }

    m_used_bytes -= block->payload + OVERHEAD;
    m_free_count++;

    // Free işaretle ve free list'e ekle
    markFree(block);
    insertFree(block);

    // Komşularla birleştir
    coalesce(block);
}

// ============================================================
// realloc
// ============================================================
void* Heap::realloc(void* ptr, uint64_t new_size)
{
    if (!ptr)      return alloc(new_size);
    if (!new_size) { free(ptr); return nullptr; }

    BlockHeader* block        = userToHeader(ptr);
    uint64_t     aligned_new  = alignUp(new_size);

    // Mevcut blok yeterince büyük
    if (block->payload >= aligned_new) return ptr;

    // Yeni blok ayır, kopyala, eskiyi serbest bırak
    void* new_ptr = alloc(new_size);
    if (!new_ptr) return nullptr;

    // memcpy (STL yok, inline)
    uint8_t* src = reinterpret_cast<uint8_t*>(ptr);
    uint8_t* dst = reinterpret_cast<uint8_t*>(new_ptr);
    uint64_t copy_size = block->payload < aligned_new ? block->payload : aligned_new;
    for (uint64_t i = 0; i < copy_size; i++) dst[i] = src[i];

    free(ptr);
    return new_ptr;
}

// ============================================================
// zalloc — Sıfırlanmış alloc
// ============================================================
void* Heap::zalloc(uint64_t size)
{
    void* ptr = alloc(size);
    if (!ptr) return nullptr;

    uint8_t* p = reinterpret_cast<uint8_t*>(ptr);
    for (uint64_t i = 0; i < size; i++) p[i] = 0;
    return ptr;
}

// ============================================================
// validate — Heap sağlık kontrolü
// ============================================================
bool Heap::validate() const
{
    BlockHeader* b = m_first;
    while (b) {
        if (b->magic != MAGIC_USED && b->magic != MAGIC_FREE) return false;
        BlockFooter* f = headerToFooter(b);
        if (f->payload != b->payload) return false;
        b = b->phys_next;
    }
    return true;
}

// ============================================================
// dumpStats / dumpBlocks — Debug çıktısı
// ============================================================
void Heap::dumpStats() const
{
    if (!g_vga) return;
    g_vga->printColored("[HEAP] ", VGAColor::Magenta);
    g_vga->print("Total=");
    g_vga->printDec(m_total_bytes / 1024);
    g_vga->print("KB | Used=");
    g_vga->printDec(m_used_bytes / 1024);
    g_vga->print("KB | Free=");
    g_vga->printDec(freeBytes() / 1024);
    g_vga->print("KB | Allocs=");
    g_vga->printDec(m_alloc_count);
    g_vga->print(" | Frees=");
    g_vga->printDec(m_free_count);
    g_vga->print("\n");
}

void Heap::dumpBlocks() const
{
    if (!g_vga) return;
    g_vga->printColored("[HEAP] Block dump:\n", VGAColor::Magenta);
    int i = 0;
    for (BlockHeader* b = m_first; b; b = b->phys_next, i++) {
        g_vga->print("  [");
        g_vga->printDec(i);
        g_vga->print("] ");
        g_vga->printHex(reinterpret_cast<uint64_t>(b));
        g_vga->print(b->magic == MAGIC_FREE ? " FREE " : " USED ");
        g_vga->printDec(b->payload);
        g_vga->print("B\n");
        if (i > 20) { g_vga->print("  ...(truncated)\n"); break; }
    }
}

// ============================================================
// Global kmalloc / kfree / krealloc
// ============================================================
void* kmalloc(uint64_t size)           { return g_heap ? g_heap->alloc(size)         : nullptr; }
void  kfree(void* ptr)                  {        if (g_heap) g_heap->free(ptr);                  }
void* krealloc(void* ptr, uint64_t sz) { return g_heap ? g_heap->realloc(ptr, sz)    : nullptr; }
void* kzalloc(uint64_t size)           { return g_heap ? g_heap->zalloc(size)        : nullptr; }

// ============================================================
// C++ operator new / delete — kmalloc/kfree üzerine köprü
// Bu sayede: VGADriver* v = new VGADriver(); çalışır
// OOP: placement new olmadan gerçek dinamik nesne oluşturma
// ============================================================
void* operator new(unsigned long size)          { return kmalloc(size); }
void* operator new[](unsigned long size)        { return kmalloc(size); }
void  operator delete(void* ptr) noexcept       { kfree(ptr); }
void  operator delete[](void* ptr) noexcept     { kfree(ptr); }
void  operator delete(void* ptr, unsigned long) noexcept  { kfree(ptr); }
void  operator delete[](void* ptr, unsigned long) noexcept { kfree(ptr); }
