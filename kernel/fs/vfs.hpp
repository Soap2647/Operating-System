// vfs.hpp — Virtual Filesystem (in-memory tree)
// OOP: VFS sınıfı — Encapsulation (m_root private), RAII (constructor / yaratır)
// Composition: VFS HAS-A INode[]
#pragma once
#include <stdint.h>

// ============================================================
// INode — Dosya sistemi düğümü
// Dizin veya dosya temsil eder
// ============================================================
struct INode {
    char     name[64];          // Dosya/dizin adı
    bool     is_dir;            // true = dizin, false = dosya
    uint8_t* data;              // Dosya içeriği (dizin için nullptr)
    uint64_t size;              // Veri boyutu (byte)
    INode*   parent;            // Üst dizin (root için nullptr)
    INode*   children[16];      // Alt düğümler (sadece dizin için)
    uint8_t  child_count;       // Mevcut alt düğüm sayısı
};

// ============================================================
// VFS — Virtual Filesystem
// In-memory ağaç yapısı, kernel dosya sistemi soyutlaması
// RAII: constructor / kökünü oluşturur
// ============================================================
class VFS {
public:
    // RAII — / kök dizinini oluşturur
    VFS();

    // Kök dizine eriş
    INode* getRoot() { return m_root; }

    // Dizin oluştur (path: "/documents" gibi)
    // Döner: oluşturulan INode* (varsa mevcut)
    INode* mkdir(const char* path);

    // Dosya oluştur ve içeriği ayarla
    // content: statik string pointer (kopyalanmaz, pointer saklanır)
    INode* create(const char* path, const char* content);

    // Dosya/dizin aç (path: "/documents/readme.drk")
    // Döner: INode* (bulunamazsa nullptr)
    INode* open(const char* path);

    // Dizin içeriğini listele
    // cb her alt düğüm için çağrılır
    void listDir(INode* node, void(*cb)(INode*, void*), void* ud);

private:
    INode* m_root;  // Kök dizin (Encapsulation — private)

    // Yardımcılar
    INode* findChild(INode* dir, const char* name);
    INode* makeNode(const char* name, bool is_dir, INode* parent);

    // Path tokenize: /a/b/c → ["a","b","c"]
    // buf'a kopyalar, tokens[] doldurur, token sayısı döner
    int tokenizePath(const char* path, char* buf, char* tokens[], int max_tokens);
};

// Global VFS singleton
extern VFS* g_vfs;
