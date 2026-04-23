// heap.hpp — Kernel Heap Allocator
//
// Algoritma  : Explicit free list + boundary tag coalescing
// Strateji   : First-fit ile free blok bul, gerekirse split et
// Büyüme     : PMM'den yeni sayfalar isteyerek heap genişler
//
// OOP Bağlantısı (midterm):
//   Encapsulation : Block yapısı ve free list tamamen private
//   RAII          : Constructor PMM'den sayfaları alır, hazır gelir
//   Composition   : PMM* m_pmm — heap, PMM'i kullanır (HAS-A ilişkisi)

#pragma once
#include <stdint.h>
#include "pmm.hpp"

// ============================================================
// Heap Sınıfı
// ============================================================
class Heap {
public:
    // ---- Sabitler ----
    static constexpr uint32_t MAGIC_FREE = 0xFEEBDAED; // Boş blok işareti
    static constexpr uint32_t MAGIC_USED = 0xDEAD1337; // Dolu blok işareti
    static constexpr uint64_t ALIGNMENT  = 16;          // x86_64 ABI gereği
    static constexpr uint64_t MIN_PAYLOAD= 16;          // free_prev+free_next için
    static constexpr uint64_t GROW_PAGES = 256;         // Büyümede alınacak sayfa (1MB)

    // Blok başlığı — her blokta, hem free hem used
    // __attribute__((packed)) YOK — alignment korunmalı
    struct BlockHeader {
        uint32_t     magic;      // MAGIC_FREE veya MAGIC_USED
        uint32_t     payload;    // Kullanıcı veri boyutu (hizalanmış)
        BlockHeader* phys_prev;  // Fiziksel bellekte önceki blok
        BlockHeader* phys_next;  // Fiziksel bellekte sonraki blok
        // Sadece free iken geçerli:
        BlockHeader* free_prev;  // Free list'te önceki
        BlockHeader* free_next;  // Free list'te sonraki
    };
    // Header boyutu = 48 byte (6 × pointer/uint = 6×8 = 48, uint32 × 2 = 8 → toplam 48)

    // Blok sonu etiketi — coalescing için
    struct BlockFooter {
        uint32_t payload; // Header.payload ile aynı değer
        uint32_t magic;   // Header.magic ile aynı değer
    };

    // Toplam blok boyutu = sizeof(Header) + payload + sizeof(Footer)
    static constexpr uint64_t HEADER_SIZE = sizeof(BlockHeader);
    static constexpr uint64_t FOOTER_SIZE = sizeof(BlockFooter);
    static constexpr uint64_t OVERHEAD    = HEADER_SIZE + FOOTER_SIZE;
    static constexpr uint64_t MIN_BLOCK   = OVERHEAD + MIN_PAYLOAD; // 64 byte

    // ---- Yapıcı (RAII) ----
    // PMM'den 'initial_pages' sayfa alır, heap bölgesini hazırlar
    Heap(PMM* pmm, uint64_t initial_pages = 1024);

    // ---- Ana Allokasyon Arayüzü ----

    // size byte allocate et, 16-byte hizalı adres döner (NULL = OOM)
    void* alloc(uint64_t size);

    // Serbest bırak (NULL güvenli)
    void  free(void* ptr);

    // Yeniden boyutlandır (NULL ptr → alloc gibi davranır)
    void* realloc(void* ptr, uint64_t new_size);

    // Sıfırlanmış blok ayır (calloc benzeri)
    void* zalloc(uint64_t size);

    // ---- İstatistikler ----
    uint64_t totalBytes()     const { return m_total_bytes; }
    uint64_t usedBytes()      const { return m_used_bytes;  }
    uint64_t freeBytes()      const { return m_total_bytes - m_used_bytes; }
    uint64_t allocCount()     const { return m_alloc_count; }
    uint64_t freeCount()      const { return m_free_count;  }

    // Heap sağlık kontrolü (corruption tespiti)
    bool validate() const;

    // VGA'ya heap durumunu yaz
    void dumpStats() const;
    void dumpBlocks() const; // Tüm blokları listele (debug)

private:
    PMM*         m_pmm;

    BlockHeader* m_free_list; // Free list'in başı (explicit doubly-linked)
    BlockHeader* m_first;     // Heap'teki ilk blok
    BlockHeader* m_last;      // Heap'teki son blok (epilogue)

    uint64_t m_start;         // Heap başlangıç adresi
    uint64_t m_end;           // Heap bitiş adresi (mevcut son sayfa)
    uint64_t m_total_bytes;   // Toplam heap boyutu
    uint64_t m_used_bytes;    // Kullanımda olan byte
    uint64_t m_alloc_count;   // Toplam alloc çağrısı
    uint64_t m_free_count;    // Toplam free çağrısı

    // ---- Private Yardımcılar ----

    // Boyutu ALIGNMENT'a yuvarla + minimum uygula
    static uint64_t alignUp(uint64_t size);

    // Block ↔ user pointer dönüşümleri
    static void*        headerToUser(BlockHeader* h);
    static BlockHeader* userToHeader(void* ptr);
    static BlockFooter* headerToFooter(BlockHeader* h);

    // Footer'dan bir önceki bloğun header'ına eriş
    static BlockHeader* footerToPrevHeader(BlockFooter* f);

    // Free list işlemleri
    void insertFree(BlockHeader* block);
    void removeFree(BlockHeader* block);

    // Block yönetimi
    void  markFree(BlockHeader* block);
    void  markUsed(BlockHeader* block);
    bool  isFree(const BlockHeader* block) const;

    // Split: block içinden 'size' payload'lı blok ayır, kalanı free bırak
    // Döner: kullanıcıya verilecek blok
    BlockHeader* split(BlockHeader* block, uint64_t size);

    // Coalesce: serbest bırakılan bloğu komşularıyla birleştir
    // Döner: birleştirilmiş blok
    BlockHeader* coalesce(BlockHeader* block);

    // Heap'i genişlet (PMM'den yeni sayfa al)
    bool grow(uint64_t min_bytes);

    // Footer'ı güncelle (header değişince footer da güncellenmiş olmalı)
    void syncFooter(BlockHeader* h);
};

// ============================================================
// Global Heap Pointer
// ============================================================
extern Heap* g_heap;

// ============================================================
// Global kmalloc / kfree / krealloc
// Tüm kernel kodu bu fonksiyonları kullanır
// ============================================================
void* kmalloc(uint64_t size);
void  kfree(void* ptr);
void* krealloc(void* ptr, uint64_t new_size);
void* kzalloc(uint64_t size);  // Sıfırlanmış alloc

// ============================================================
// C++ operator new / delete — OOP için zorunlu
// Bu sayede: new VGADriver() gibi yazabiliyoruz
// ============================================================
void* operator new   (unsigned long size);
void* operator new[] (unsigned long size);
void  operator delete   (void* ptr) noexcept;
void  operator delete[] (void* ptr) noexcept;
void  operator delete   (void* ptr, unsigned long) noexcept;
void  operator delete[] (void* ptr, unsigned long) noexcept;

// Placement new (mevcut buffer'a oluştur — heap kullanmaz)
inline void* operator new  (unsigned long, void* p) noexcept { return p; }
inline void* operator new[](unsigned long, void* p) noexcept { return p; }
