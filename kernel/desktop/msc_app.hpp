// msc_app.hpp — MertStudio Code Editör Uygulaması
// OOP: MertStudioCode : public App (INHERITANCE)
#pragma once
#include "app.hpp"

// ============================================================
// MertStudioCode — Basit metin editörü
// OOP: Inheritance (App'ten türer)
// ============================================================
class MertStudioCode : public App {
public:
    MertStudioCode(Window* w);

    // ---- App interface override'ları (POLYMORPHISM) ----
    void        onDraw(FramebufferDriver* fb) override;
    void        onKey(KeyEvent& ke)           override;
    void        onMouse(MouseEvent& me)       override;
    const char* getName()   const override { return "MSC Editor"; }
    uint32_t    getColor()  const override { return 0x00FAB387; }  // Peach
    char        getSymbol() const override { return 'M'; }

private:
    static constexpr int GUTTER_W   = 32;  // Sol satır numaraları alanı
    static constexpr int TEXT_SIZE  = 1024; // Metin tamponu boyutu

    char m_text[TEXT_SIZE]; // Metin içeriği
    int  m_textlen;          // Mevcut metin uzunluğu
    int  m_cursor;           // İmleç pozisyonu (m_text içinde)

    // Yardımcı: satır sayısını hesapla
    int  lineCount() const;

    // Yardımcı: pozisyonun satır/sütununu bul
    void posToLineCol(int pos, int& line, int& col) const;
};
