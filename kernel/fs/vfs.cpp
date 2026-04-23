// vfs.cpp — Virtual Filesystem implementasyonu
// OOP: RAII (constructor), Encapsulation (m_root private)
#include "vfs.hpp"
#include "../../libc/include/kstring.hpp"

VFS* g_vfs = nullptr;

// ============================================================
// Constructor — RAII: / kök dizinini hemen oluştur
// ============================================================
VFS::VFS()
{
    m_root = new INode();
    kmemset(m_root, 0, sizeof(INode));
    m_root->name[0] = '/';
    m_root->name[1] = '\0';
    m_root->is_dir  = true;
    m_root->parent  = nullptr;
    m_root->child_count = 0;
    m_root->data    = nullptr;
    m_root->size    = 0;
}

// ============================================================
// Yardımcı: yeni INode oluştur
// ============================================================
INode* VFS::makeNode(const char* name, bool is_dir, INode* parent)
{
    INode* node = new INode();
    kmemset(node, 0, sizeof(INode));
    kstrncpy(node->name, name, 63);
    node->name[63]  = '\0';
    node->is_dir    = is_dir;
    node->parent    = parent;
    node->child_count = 0;
    node->data      = nullptr;
    node->size      = 0;
    return node;
}

// ============================================================
// Yardımcı: bir dizin içinde isimle çocuk bul
// ============================================================
INode* VFS::findChild(INode* dir, const char* name)
{
    if (!dir || !dir->is_dir) return nullptr;
    for (int i = 0; i < dir->child_count; i++) {
        if (kstrcmp(dir->children[i]->name, name) == 0)
            return dir->children[i];
    }
    return nullptr;
}

// ============================================================
// Path tokenize: "/a/b/c" → tokens = ["a","b","c"]
// buf: çalışma tamponu (path kopyalanır)
// tokens[]: her token'ın başına pointer
// Döner: token sayısı
// ============================================================
int VFS::tokenizePath(const char* path, char* buf, char* tokens[], int max_tokens)
{
    if (!path || path[0] == '\0') return 0;

    kstrncpy(buf, path, 255);
    buf[255] = '\0';

    int count = 0;
    int i = 0;
    // Baştaki '/' atla
    if (buf[0] == '/') i++;

    while (buf[i] != '\0' && count < max_tokens) {
        tokens[count++] = &buf[i];
        // '/' bulana kadar ilerle
        while (buf[i] != '\0' && buf[i] != '/') i++;
        if (buf[i] == '/') {
            buf[i] = '\0';
            i++;
        }
    }
    return count;
}

// ============================================================
// mkdir: verilen path'de dizin oluştur
// Örnek: "/documents" → root altına "documents" dizini
// Zaten varsa mevcut node döner
// ============================================================
INode* VFS::mkdir(const char* path)
{
    if (!path) return nullptr;

    char buf[256];
    char* tokens[16];
    int count = tokenizePath(path, buf, tokens, 16);
    if (count == 0) return m_root;

    INode* cur = m_root;
    for (int i = 0; i < count; i++) {
        INode* child = findChild(cur, tokens[i]);
        if (!child) {
            // Oluştur
            if (cur->child_count >= 16) return nullptr;
            child = makeNode(tokens[i], true, cur);
            cur->children[cur->child_count++] = child;
        }
        cur = child;
    }
    return cur;
}

// ============================================================
// create: verilen path'de dosya oluştur, content pointer'ını sakla
// Örnek: "/documents/readme.drk" → documents/ altına readme.drk
// ============================================================
INode* VFS::create(const char* path, const char* content)
{
    if (!path) return nullptr;

    char buf[256];
    char* tokens[16];
    int count = tokenizePath(path, buf, tokens, 16);
    if (count == 0) return nullptr;

    // Son token dosya adı, öncekiler dizin
    INode* cur = m_root;
    for (int i = 0; i < count - 1; i++) {
        INode* child = findChild(cur, tokens[i]);
        if (!child) {
            if (cur->child_count >= 16) return nullptr;
            child = makeNode(tokens[i], true, cur);
            cur->children[cur->child_count++] = child;
        }
        cur = child;
    }

    // Dosya var mı?
    const char* fname = tokens[count - 1];
    INode* file = findChild(cur, fname);
    if (!file) {
        if (cur->child_count >= 16) return nullptr;
        file = makeNode(fname, false, cur);
        cur->children[cur->child_count++] = file;
    }

    // İçerik pointer'ını sakla (kopyalama yapılmaz)
    file->data = reinterpret_cast<uint8_t*>(const_cast<char*>(content));
    file->size = content ? static_cast<uint64_t>(kstrlen(content)) : 0;
    return file;
}

// ============================================================
// open: verilen path'deki INode'u bul
// ============================================================
INode* VFS::open(const char* path)
{
    if (!path) return nullptr;
    if (path[0] == '/' && path[1] == '\0') return m_root;

    char buf[256];
    char* tokens[16];
    int count = tokenizePath(path, buf, tokens, 16);
    if (count == 0) return m_root;

    INode* cur = m_root;
    for (int i = 0; i < count; i++) {
        cur = findChild(cur, tokens[i]);
        if (!cur) return nullptr;
    }
    return cur;
}

// ============================================================
// listDir: dizin içeriğini callback ile listele
// ============================================================
void VFS::listDir(INode* node, void(*cb)(INode*, void*), void* ud)
{
    if (!node || !node->is_dir || !cb) return;
    for (int i = 0; i < node->child_count; i++) {
        cb(node->children[i], ud);
    }
}
