// settings_app.hpp — Ayarlar Uygulaması
// OOP: SettingsApp : public App (INHERITANCE)
#pragma once
#include "app.hpp"

// ============================================================
// SettingsApp — Sistem ayarları uygulaması
// OOP: Inheritance (App'ten türer)
// ============================================================
class SettingsApp : public App {
public:
    // Kategori listesi
    enum Category {
        APPEARANCE = 0,
        TERMINAL   = 1,
        SYSTEM     = 2,
        ABOUT      = 3
    };

    SettingsApp(Window* w);

    // ---- App interface override'ları (POLYMORPHISM) ----
    void        onDraw(FramebufferDriver* fb) override;
    void        onKey(KeyEvent& ke)           override;
    void        onMouse(MouseEvent& me)       override;
    const char* getName()   const override { return "Ayarlar"; }
    uint32_t    getColor()  const override { return 0x0094E2D5; }  // Teal
    char        getSymbol() const override { return 'S'; }

private:
    Category m_category;    // Aktif kategori
    int      m_bgChoice;    // Arka plan rengi seçimi (0-5)

    // ---- Özel çizim yardımcıları ----
    void drawSidebar(FramebufferDriver* fb);
    void drawContent(FramebufferDriver* fb);
    void drawAppearance(FramebufferDriver* fb, int cx, int cy, int cw, int ch);
    void drawTerminal(FramebufferDriver* fb, int cx, int cy, int cw, int ch);
    void drawSystem(FramebufferDriver* fb, int cx, int cy, int cw, int ch);
    void drawAbout(FramebufferDriver* fb, int cx, int cy, int cw, int ch);
};
