// file_manager.hpp — Dosya Yöneticisi Uygulaması
// OOP: FileManager : public App (INHERITANCE)
//      Composition: FileManager HAS-A VFS
#pragma once
#include "app.hpp"
#include "../fs/vfs.hpp"

// ============================================================
// FileManager — Dosya yöneticisi uygulaması
// OOP: Inheritance (App'ten türer), Composition (VFS barındırır)
// ============================================================
class FileManager : public App {
public:
    // Composition: constructor'a VFS inject edilir
    FileManager(Window* w, VFS* vfs);

    // ---- App interface override'ları (POLYMORPHISM) ----
    void        onDraw(FramebufferDriver* fb) override;
    void        onKey(KeyEvent& ke)           override;
    void        onMouse(MouseEvent& me)       override;
    const char* getName()   const override { return "Dosyalar"; }
    uint32_t    getColor()  const override { return 0x0089B4FA; }  // Blue
    char        getSymbol() const override { return 'F'; }

private:
    VFS*   m_vfs;           // Composition: HAS-A VFS
    INode* m_currentDir;    // Mevcut dizin
    int    m_selectedItem;  // Seçili öğe index'i
    bool   m_dirty;         // Yeniden çizim gerekli mi

    // ---- Özel çizim yardımcıları ----
    void drawToolbar(FramebufferDriver* fb);
    void drawSidebar(FramebufferDriver* fb);
    void drawFileList(FramebufferDriver* fb);

    // Mevcut path'i yaz (en fazla 128 karakter)
    void buildPath(INode* node, char* buf, int bufsize);
};
